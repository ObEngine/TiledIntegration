#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include <logger.hpp>
#include <lyra/arguments.hpp>
#include <lyra/lyra.hpp>
#include <nlohmann/json.hpp>
#include <vili/node.hpp>
#include <vili/writer.hpp>

struct TiledIntegrationArgs
{
    std::string input_file;
    std::string output_file;
    std::string cwd;
};

using Version = std::vector<signed int>;
Version tiled_version;

Version str_to_version(const std::string& str_version)
{
    Version version;
    std::stringstream ss_version(str_version);
    for (std::string str_number; std::getline(ss_version, str_number, '.');)
    {
        version.push_back(std::stoi(str_number));
    }
    return version;
}


std::string normalize_path(std::string path)
{
    std::replace(path.begin(), path.end(), '\\', '/');
    return path;
}

nlohmann::json::object_t load_tiled_map(const std::string& filepath)
{
    std::ifstream input_file(filepath);
    std::ifstream ifs(filepath);
    std::string content(
        (std::istreambuf_iterator<char>(ifs)), (std::istreambuf_iterator<char>()));
    nlohmann::json tiled_json;
    input_file >> tiled_json;

    return tiled_json;
}

nlohmann::json::object_t load_tiled_tileset(const std::string& directory, const std::string& filename)
{
    std::string json_filepath = std::filesystem::canonical(std::filesystem::path(directory) / filename).string();
    const auto first_dot = json_filepath.find_last_of('.');
    json_filepath = std::string(json_filepath.begin(), json_filepath.begin() + first_dot);
    json_filepath += ".json";
    logger->debug("    Loading tileset at path {}", json_filepath);
    std::ifstream input_file(json_filepath);
    nlohmann::json tileset_json;
    input_file >> tileset_json;

    return tileset_json;
}

template <class T>
T find_property(
    const nlohmann::json::value_type& properties, const std::string& property_name)
{
    for (const auto& property : properties)
    {
        if (property.at("name") == property_name)
        {
            return property.at("value").get<T>();
        }
    }
    throw std::runtime_error("Could not find property " + property_name);
}

bool contains_property(
    const nlohmann::json::value_type& properties, const std::string& property_name)
{
    for (const auto& property : properties)
    {
        if (property.at("name") == property_name)
        {
            return true;
        }
    }
    return false;
}

void repeat_sprites(vili::node& sprites, const std::string& base_id,
    const vili::node& base_sprite, uint32_t repeat_x, uint32_t repeat_y)
{
    const vili::node& sprite_rect = base_sprite.at("rect");
    const vili::number base_x = sprite_rect.at("x");
    const vili::number base_y = sprite_rect.at("y");
    const double base_width = sprite_rect.at("width");
    const double base_height = sprite_rect.at("height");
    uint32_t id = 1;
    for (uint32_t x = 0; x < repeat_x; x++)
    {
        for (uint32_t y = 0; y < repeat_y; y++)
        {
            const double new_x = base_x + base_width * x;
            const double new_y = base_y + base_height * y;

            vili::node new_sprite = base_sprite;
            new_sprite["rect"]["x"] = new_x;
            new_sprite["rect"]["y"] = new_y;

            sprites[base_id + "_" + std::to_string(id)] = new_sprite;
            id++;
        }
    }
}

vili::node create_game_object(const nlohmann::json::value_type& object, const std::unordered_map<uint32_t, std::string>& objects_ids)
{
    std::string object_type_property = "class";
    if (tiled_version < str_to_version("1.9")) {
        object_type_property = "type";
    }

    const std::string object_type = object.at(object_type_property).get<std::string>();
    vili::node game_object = vili::object { { "type", object_type } };
    game_object["Requires"]
        = vili::object{ { "x", object.at("x").get<float>() },
              { "y", object.at("y").get<float>() }, {"width", object.at("width").get<float>()}, {"height", object.at("height").get<float>()}, {"rotation", object.at("rotation").get<float>() } };
    if (object.contains("properties"))
    {
        for (const auto& object_property : object["properties"])
        {
            const std::string property_type = object_property["type"];
            if (property_type == "bool")
            {
                game_object["Requires"]
                    [object_property["name"].get<std::string>()]
                = object_property["value"].get<bool>();
            }
            if (property_type == "int")
            {
                game_object["Requires"]
                    [object_property["name"].get<std::string>()]
                = object_property["value"].get<int>();
            }
            else if (property_type == "float")
            {
                game_object["Requires"]
                    [object_property["name"].get<std::string>()]
                = object_property["value"].get<float>();
            }
            else if (property_type == "object")
            {
                const int object_id = object_property["value"].get<int>();
                if (objects_ids.find(object_id) == objects_ids.end())
                {
                    throw std::runtime_error(
                        fmt::format("Object of type {} depends on other object with id "
                                    "{}, please reorder them",
                            object_type, object_id));
                }
                game_object["Requires"]
                    [object_property["name"].get<std::string>()]
                    = objects_ids.at(object_id);
            }
            else if (property_type == "string" || property_type == "color"
                || property_type == "file")
            {
                game_object["Requires"]
                    [object_property["name"].get<std::string>()]
                = object_property["value"].get<std::string>();
            }
        }
    }
    return game_object;
}

std::string replace(
    std::string subject, const std::string& search, const std::string& replace)
{
    size_t pos = 0;
    while ((pos = subject.find(search, pos)) != std::string::npos)
    {
        subject.replace(pos, search.length(), replace);
        pos += replace.length();
    }
    return subject;
}

std::string make_object_id(const std::string& base_id, uint32_t amount_of_objects)
{
    return replace(base_id, "{index}", std::to_string(amount_of_objects));
}

vili::object export_obe_scene(
    const std::string& base_folder, const std::string& scene_folder, const std::string& vili_filename, nlohmann::json::object_t tmx_json)
{
    std::string scene_name = vili_filename;
    auto last_slash = scene_name.find_last_of("/");
    last_slash = (last_slash != std::string::npos) ? last_slash + 1 : 0;
    const auto first_dot = scene_name.find('.', last_slash);
    scene_name = std::string(scene_name.begin(), scene_name.begin() + first_dot);
    vili::object obe_scene;
    obe_scene["Meta"] = vili::object { { "name", scene_name } };

    obe_scene["View"] = vili::object { { "size", 1.0 },
        { "position",
            vili::object { { "x", 0.0 }, { "y", 0.0 }, { "unit", "SceneUnits" } } },
        { "referential", "TopLeft" } };

    tiled_version = str_to_version(tmx_json["version"]);
    std::string object_type_property = "class";
    if (tiled_version < str_to_version("1.9"))
    {
        object_type_property = "type";
    }

    obe_scene["Tiles"] = vili::object {};

    obe_scene["Tiles"]["tileWidth"] = tmx_json["tilewidth"].get<int>();
    obe_scene["Tiles"]["tileHeight"] = tmx_json["tileheight"].get<int>();

    obe_scene["Tiles"]["width"] = tmx_json["width"].get<int>();
    obe_scene["Tiles"]["height"] = tmx_json["height"].get<int>();

    obe_scene["Tiles"]["layers"] = vili::object {};

    int layer = tmx_json["layers"].size();
    vili::node game_objects = vili::object {};
    vili::node collisions = vili::object {};
    vili::node sprites = vili::object {};
    std::unordered_map<uint32_t, std::string> objects_ids;
    for (const auto& tmx_layer : tmx_json["layers"])
    {
        std::optional<int> custom_layer;
        if (tmx_layer.contains("properties"))
        {
            const auto& layer_properties = tmx_layer.at("properties");
            if (contains_property(layer_properties, "layer"))
            {
                custom_layer = find_property<int>(layer_properties, "layer");
            }
        }

        if (tmx_layer["type"] == "tilelayer")
        {
            std::string layer_id = tmx_layer["name"];
            layer_id = vili::utils::string::replace(layer_id, " ", "_");
            std::vector<int> layer_data = tmx_layer["data"];
            vili::array tiles_data = vili::array {};
            for (const uint32_t tile : layer_data)
            {
                tiles_data.push_back(vili::integer{ tile });
            }
            obe_scene["Tiles"]["layers"][layer_id] = vili::object {};
            vili::node& obe_layer = obe_scene["Tiles"]["layers"][layer_id];

            obe_layer["x"] = tmx_layer["x"].get<int>();
            obe_layer["y"] = tmx_layer["y"].get<int>();
            obe_layer["width"] = tmx_layer["width"].get<int>();
            obe_layer["height"] = tmx_layer["height"].get<int>();
            obe_layer["layer"] = custom_layer ? custom_layer.value() : layer--;
            obe_layer["visible"] = tmx_layer["visible"].get<bool>();
            obe_layer["opacity"] = tmx_layer["opacity"].get<int>();
            obe_layer["tiles"] = tiles_data;
        }
        else if (tmx_layer["type"] == "objectgroup")
        {
            for (const auto& object : tmx_layer["objects"])
            {
                if (object.contains(object_type_property)
                    && !object.at(object_type_property).get<std::string>().empty())
                {
                    uint32_t object_id = object.at("id");
                    std::string game_object_id = object.at("name");
                    game_object_id = make_object_id(game_object_id, objects_ids.size());
                    game_objects[game_object_id] = create_game_object(object, objects_ids);
                    objects_ids[object_id] = game_object_id;
                }
                else if (object.contains("polygon"))
                {
                    vili::node new_collision = vili::object {};
                    new_collision["points"] = vili::array {};
                    const int x = object.at("x");
                    const int y = object.at("y");
                    for (const auto& tmx_collision_point : object.at("polygon"))
                    {
                        new_collision["points"].push(vili::object {
                            { "x", tmx_collision_point.at("x").get<int>() + x },
                            { "y", tmx_collision_point.at("y").get<int>() + y } });
                    }
                    new_collision["unit"] = "ScenePixels";
                    std::string collision_id = object.at("name").get<std::string>();
                    if (collision_id.empty())
                    {
                        collision_id
                            = "collider_" + std::to_string(object.at("id").get<int>());
                    }
                    collisions[collision_id] = new_collision;
                }
                else
                {
                    
                }
            }
        }
        else if (tmx_layer["type"] == "imagelayer")
        {
            vili::node new_sprite = vili::object {};
            std::string sprite_id = tmx_layer["name"];
            const auto& sprite_properties = tmx_layer["properties"];
            const float width = find_property<float>(sprite_properties, "width");
            const float height = find_property<float>(sprite_properties, "height");
            new_sprite["rect"] = vili::object { { "x", tmx_layer["x"].get<float>() },
                { "y", tmx_layer["y"].get<float>() }, { "width", width },
                { "height", height } };

            std::string image_path = tmx_layer["image"].get<std::string>();
            image_path = (std::filesystem::path(base_folder) / image_path).string();
            image_path = std::filesystem::relative(image_path).string();
            image_path = replace(image_path, "\\", "/");
            new_sprite["path"] = image_path;
            if (contains_property(sprite_properties, "xTransform")
                || contains_property(sprite_properties, "yTransform"))
            {
                new_sprite["transform"] = vili::object {
                    { "x", find_property<std::string>(sprite_properties, "xTransform") },
                    { "y", find_property<std::string>(sprite_properties, "yTransform") }
                };
            }

            if (contains_property(sprite_properties, "layer"))
            {
                new_sprite["layer"] = find_property<int>(sprite_properties, "layer");
            }

            if (contains_property(sprite_properties, "repeat_x")
                || contains_property(sprite_properties, "repeat_y"))
            {
                repeat_sprites(sprites, sprite_id, new_sprite,
                    find_property<int>(sprite_properties, "repeat_x"),
                    find_property<int>(sprite_properties, "repeat_y"));
            }

            sprites[sprite_id] = new_sprite;
        }
    }

    if (!sprites.empty())
    {
        obe_scene["Sprites"] = sprites;
    }

    if (!collisions.empty())
    {
        obe_scene["Collisions"] = collisions;
    }

    obe_scene["Tiles"]["sources"] = vili::object {};
    for (const auto& tmx_tileset : tmx_json["tilesets"])
    {
        std::string tileset_id = tmx_tileset["source"];
        auto last_slash = tileset_id.find_last_of("/");
        last_slash = (last_slash != std::string::npos) ? last_slash + 1 : 0;
        const auto first_dot = tileset_id.find('.', last_slash);
        tileset_id = std::string(tileset_id.begin() + last_slash, tileset_id.begin() + first_dot);
        obe_scene["Tiles"]["sources"][tileset_id] = vili::object {};
        vili::node& vili_tileset = obe_scene["Tiles"]["sources"][tileset_id];

        vili_tileset["firstTileId"] = tmx_tileset["firstgid"].get<int>();
        const std::string tileset_source = tmx_tileset["source"];
        nlohmann::json::object_t tileset_json
            = load_tiled_tileset(scene_folder, tileset_source);
        vili_tileset["columns"] = tileset_json["columns"].get<int>();
        vili_tileset["tilecount"] = tileset_json["tilecount"].get<int>();
        vili_tileset["margin"] = tileset_json["margin"].get<int>();
        vili_tileset["spacing"] = tileset_json["spacing"].get<int>();
        vili_tileset["tile"]
            = vili::object { { "width", tileset_json["tilewidth"].get<int>() },
                  { "height", tileset_json["tileheight"].get<int>() } };
        std::filesystem::path tileset_directory = std::filesystem::canonical(
            std::filesystem::path(scene_folder) / tileset_source)
                                .remove_filename();
        std::string image_path = tileset_json["image"].get<std::string>();
        image_path = (tileset_directory / image_path).string();
        image_path = std::filesystem::relative(image_path, base_folder).string();
        image_path = replace(image_path, "\\", "/");
        vili_tileset["image"]
            = vili::object { { "width", tileset_json["imagewidth"].get<int>() },
                  { "height", tileset_json["imageheight"].get<int>() },
                  { "path", image_path } };

        vili::node tileset_collisions = vili::array {};
        vili::node animated_tiles = vili::array {};
        vili::node tilesets_game_objects = vili::array {};
        for (const auto& tmx_tile : tileset_json["tiles"])
        {
            if (tmx_tile.contains("animation"))
            {
                vili::node new_animated_tile
                    = vili::object { { "id", tmx_tile.at("id").get<int>() },
                          { "frames", vili::array {} } };
                for (const auto& tile_animation_frame : tmx_tile.at("animation"))
                {
                    vili::node new_animation_frame = vili::object {
                        { "clock", tile_animation_frame.at("duration").get<int>() },
                        { "tileid", tile_animation_frame.at("tileid").get<int>() }
                    };
                    new_animated_tile["frames"].push(new_animation_frame);
                }
                animated_tiles.push(new_animated_tile);
            }
            if (tmx_tile.contains("objectgroup"))
            {
                for (const auto& object : tmx_tile.at("objectgroup").at("objects"))
                {
                    if (object.contains("polygon") || !object.contains("point"))
                    {
                        vili::node new_collision
                            = vili::object { { "id", tmx_tile.at("id").get<int>() },
                                  { "points", vili::array {} } };
                        vili::node& collision_points = new_collision.at("points");
                        if (object.contains("polygon"))
                        {
                            const int x = object.at("x");
                            const int y = object.at("y");
                            for (const auto& tmx_collision_point : object.at("polygon"))
                            {
                                collision_points.push(vili::object {
                                    { "x", tmx_collision_point.at("x").get<int>() + x },
                                    { "y",
                                        tmx_collision_point.at("y").get<int>() + y } });
                            }
                        }
                        else if (!object.contains("point"))
                        {
                            const int x = object.at("x");
                            const int y = object.at("y");
                            const int width = object.at("width");
                            const int height = object.at("height");
                            collision_points.push(
                                vili::object { { "x", x }, { "y", y } });
                            collision_points.push(
                                vili::object { { "x", x + width }, { "y", y } });
                            collision_points.push(
                                vili::object { { "x", x + width }, { "y", y + height } });
                            collision_points.push(
                                vili::object { { "x", x }, { "y", y + height } });
                        }
                        if (object.contains("properties"))
                        {
                            const auto& collision_properties = object.at("properties");
                            if (contains_property(collision_properties, "tag"))
                            {
                                new_collision["tag"] = find_property<std::string>(
                                    collision_properties, "tag");
                            }
                        }
                        new_collision["unit"] = "ScenePixels";
                        tileset_collisions.push(new_collision);
                    }
                    else if (object.contains("point") && object.at("point").get<bool>())
                    {
                        uint32_t object_id = object.at("id").get<int>()
                            + tmx_tileset["firstgid"].get<int>() - 1;
                        std::string game_object_id = object.at("name");
                        vili::node new_game_object
                            = create_game_object(object, objects_ids);
                        new_game_object["tileId"] = vili::integer { object_id };
                        new_game_object["id"] = game_object_id;
                        tilesets_game_objects.push(new_game_object);
                    }
                }
            }
        }

        if (!animated_tiles.empty())
        {
            vili_tileset["animations"] = animated_tiles;
        }

        if (!tileset_collisions.empty())
        {
            vili_tileset["collisions"] = tileset_collisions;
        }

        if (!tilesets_game_objects.empty())
        {
            vili_tileset["objects"] = tilesets_game_objects;
        }
    }

    if (!game_objects.empty())
    {
        obe_scene["GameObjects"] = game_objects;
    }

    return obe_scene;
}

void run(const TiledIntegrationArgs& args)
{
    const std::string scene_folder = std::filesystem::path(args.input_file).parent_path().string();
    vili::object obe_scene = export_obe_scene(
        args.cwd, scene_folder, args.output_file, load_tiled_map(args.input_file));
    vili::writer::dump_options options;
    options.array.items_per_line.any = obe_scene["Tiles"]["width"];
    options.object.items_per_line.any = 1;
    std::string scene_dump = vili::writer::dump(obe_scene, options);
    std::ofstream scene_file;
    scene_file.open(args.output_file);
    scene_file.write(scene_dump.c_str(), scene_dump.size());
    scene_file.close();
}

TiledIntegrationArgs parse_args(int argc, char** argv)
{
    TiledIntegrationArgs args;

    const lyra::cli cli = lyra::arg(args.input_file, "input_file").required(true)
        | lyra::arg(args.output_file, "output_file").required(true)
        | lyra::arg(args.cwd, "current_working_directory").required(false);
    const lyra::parse_result result = cli.parse({ argc, argv });

    args.input_file = normalize_path(args.input_file);
    args.output_file = normalize_path(args.output_file);
    args.cwd = normalize_path(args.cwd);
    if (args.cwd.empty())
    {
        args.cwd = std::filesystem::current_path().string();
    }
    logger->info("[ObEngine] Tiled Integration started");
    logger->info("  - Input file : {}", args.input_file);
    logger->info("  - Output file : {}", args.output_file);
    logger->info("  - Current working directory : {}", args.cwd);

    if (!result)
    {
        logger->error("Error in command line: {}", result.errorMessage());
        exit(1);
    }

    return args;
}

int main(int argc, char** argv)
{
    init_logger();

    const TiledIntegrationArgs args = parse_args(argc, argv);

#if defined _DEBUG || defined _NDEBUG
    run(args);
#else

    bool no_error = true;
    try
    {
        run(args);
    }
    catch (const std::exception& e)
    {
        no_error = false;
        logger->error(e.what());
    }
    if (no_error)
    {
        logger->info("Done :)");
    }
    else
    {
        logger->info("Something went wrong :(");
    }
#endif

    

    return 0;
}