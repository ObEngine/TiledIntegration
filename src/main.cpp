#include <fstream>
#include <string>
#include <vector>

#include <logger.hpp>
#include <lyra/arguments.hpp>
#include <lyra/lyra.hpp>
#include <nlohmann/json.hpp>
#include <vili/node.hpp>
#include <vili/writer.hpp>

nlohmann::json::object_t load_tiled_map(const std::string& filepath)
{
    std::ifstream input_file(filepath);
    nlohmann::json tiled_json;
    input_file >> tiled_json;

    return tiled_json;
}

nlohmann::json::object_t load_tiled_tileset(const std::string& filepath)
{
    std::string json_filepath = filepath;
    const auto first_dot = json_filepath.find('.');
    json_filepath = std::string(json_filepath.begin(), json_filepath.begin() + first_dot);
    json_filepath += ".json";
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
    const vili::integer base_x_i = sprite_rect.at("x");
    const vili::integer base_y_i = sprite_rect.at("y");
    const double base_x = base_x_i;
    const double base_y = base_y_i;
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

vili::object export_obe_scene(
    const std::string& vili_filename, nlohmann::json::object_t tmx_json)
{
    std::string scene_name = vili_filename;
    const auto first_dot = scene_name.find('.');
    scene_name = std::string(scene_name.begin(), scene_name.begin() + first_dot);
    vili::object obe_scene;
    obe_scene["Meta"] = vili::object { { "name", scene_name } };

    obe_scene["View"] = vili::object { { "size", 1.0 },
        { "position",
            vili::object { { "x", 0.0 }, { "y", 0.0 }, { "unit", "SceneUnits" } } },
        { "referential", "TopLeft" } };

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
    std::unordered_map<uint32_t, std::string> objectsIds;
    for (const auto& tmx_layer : tmx_json["layers"])
    {
        if (tmx_layer["type"] == "tilelayer")
        {
            std::string layer_id = tmx_layer["name"];
            layer_id = vili::utils::string::replace(layer_id, " ", "_");
            std::vector<int> layer_data = tmx_layer["data"];
            vili::array tiles_data = vili::array {};
            for (const int tile : layer_data)
            {
                tiles_data.push_back(tile);
            }
            obe_scene["Tiles"]["layers"][layer_id] = vili::object {};
            vili::node& obe_layer = obe_scene["Tiles"]["layers"][layer_id];

            obe_layer["x"] = tmx_layer["x"].get<int>();
            obe_layer["y"] = tmx_layer["y"].get<int>();
            obe_layer["width"] = tmx_layer["width"].get<int>();
            obe_layer["height"] = tmx_layer["height"].get<int>();
            obe_layer["layer"] = layer--;
            obe_layer["visible"] = tmx_layer["visible"].get<bool>();
            obe_layer["opacity"] = tmx_layer["opacity"].get<int>();
            obe_layer["tiles"] = tiles_data;
        }
        else if (tmx_layer["type"] == "objectgroup")
        {
            for (const auto& object : tmx_layer["objects"])
            {
                if (object.contains("point") && object.at("point").get<bool>())
                {
                    const std::string gameObjectId = object.at("name").get<std::string>();
                    objectsIds[object.at("id")] = gameObjectId;
                    game_objects[gameObjectId] = vili::object { { "type",
                        object.at("type").get<std::string>() } };
                    vili::node& gameObject = game_objects[gameObjectId];
                    gameObject["Requires"]
                        = vili::object { { "x", object.at("x").get<float>() },
                              { "y", object.at("y").get<float>() } };
                    if (object.contains("properties"))
                    {
                        for (const auto& object_property : object["properties"])
                        {
                            const std::string property_type = object_property["type"];
                            if (property_type == "int")
                            {
                                gameObject["Requires"]
                                          [object_property["name"].get<std::string>()]
                                    = object_property["value"].get<int>();
                            }
                            if (property_type == "int")
                            {
                                gameObject["Requires"]
                                          [object_property["name"].get<std::string>()]
                                    = object_property["value"].get<int>();
                            }
                            else if (property_type == "float")
                            {
                                gameObject["Requires"]
                                          [object_property["name"].get<std::string>()]
                                    = object_property["value"].get<float>();
                            }
                            else if (property_type == "object")
                            {
                                gameObject["Requires"]
                                          [object_property["name"].get<std::string>()]
                                    = objectsIds[object_property["value"].get<int>()];
                            }
                            else if (property_type == "string" || property_type == "color"
                                || property_type == "file")
                            {
                                gameObject["Requires"]
                                          [object_property["name"].get<std::string>()]
                                    = object_property["value"].get<std::string>();
                            }
                        }
                    }
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
            }
        }
        else if (tmx_layer["type"] == "imagelayer")
        {
            vili::node new_sprite = vili::object {};
            std::string sprite_id = tmx_layer["name"];
            const auto& sprite_properties = tmx_layer["properties"];
            const float width = find_property<float>(sprite_properties, "width");
            const float height = find_property<float>(sprite_properties, "height");
            new_sprite["rect"] = vili::object { { "x", tmx_layer["x"].get<int>() },
                { "y", tmx_layer["y"].get<int>() }, { "width", width },
                { "height", height } };
            new_sprite["path"] = tmx_layer["image"].get<std::string>();
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
        const auto first_dot = tileset_id.find('.');
        tileset_id = std::string(tileset_id.begin(), tileset_id.begin() + first_dot);
        obe_scene["Tiles"]["sources"][tileset_id] = vili::object {};
        vili::node& vili_tileset = obe_scene["Tiles"]["sources"][tileset_id];

        vili_tileset["firstTileId"] = tmx_tileset["firstgid"].get<int>();
        nlohmann::json::object_t tileset_json = load_tiled_tileset(tmx_tileset["source"]);
        vili_tileset["columns"] = tileset_json["columns"].get<int>();
        vili_tileset["tilecount"] = tileset_json["tilecount"].get<int>();
        vili_tileset["margin"] = tileset_json["margin"].get<int>();
        vili_tileset["spacing"] = tileset_json["spacing"].get<int>();
        vili_tileset["tile"]
            = vili::object { { "width", tileset_json["tilewidth"].get<int>() },
                  { "height", tileset_json["tileheight"].get<int>() } };
        vili_tileset["image"]
            = vili::object { { "width", tileset_json["imagewidth"].get<int>() },
                  { "height", tileset_json["imageheight"].get<int>() },
                  { "path", tileset_json["image"].get<std::string>() } };

        vili::node collisions = vili::array {};
        vili::node animated_tiles = vili::array {};
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
                vili::node new_collision
                    = vili::object { { "id", tmx_tile.at("id").get<int>() },
                          { "points", vili::array {} } };
                const auto& collision
                    = tmx_tile.at("objectgroup")
                          .at("objects")[0]; // TODO: Handle multiple collisions
                vili::node& collision_points = new_collision.at("points");
                if (collision.contains("polygon"))
                {
                    const int x = collision.at("x");
                    const int y = collision.at("y");
                    for (const auto& tmx_collision_point : collision.at("polygon"))
                    {
                        collision_points.push(vili::object {
                            { "x", tmx_collision_point.at("x").get<int>() + x },
                            { "y", tmx_collision_point.at("y").get<int>() + y } });
                    }
                }
                else
                {
                    const int x = collision.at("x");
                    const int y = collision.at("y");
                    const int width = collision.at("width");
                    const int height = collision.at("height");
                    collision_points.push(vili::object { { "x", x }, { "y", y } });
                    collision_points.push(
                        vili::object { { "x", x + width }, { "y", y } });
                    collision_points.push(
                        vili::object { { "x", x + width }, { "y", y + height } });
                    collision_points.push(
                        vili::object { { "x", x }, { "y", y + height } });
                }
                if (collision.contains("properties"))
                {
                    const auto& collision_properties = collision.at("properties");
                    if (contains_property(collision_properties, "tag"))
                    {
                        new_collision["tag"]
                            = find_property<std::string>(collision_properties, "tag");
                    }
                }
                collisions.push(new_collision);
            }
        }

        if (!animated_tiles.empty())
        {
            vili_tileset["animations"] = animated_tiles;
        }

        if (!collisions.empty())
        {
            vili_tileset["collisions"] = collisions;
        }
    }

    if (!game_objects.empty())
    {
        obe_scene["GameObjects"] = game_objects;
    }

    return obe_scene;
}

void run(const std::string& input_file, const std::string& output_file)
{
    vili::object obe_scene = export_obe_scene(output_file, load_tiled_map(input_file));
    vili::writer::dump_options options;
    options.array.items_per_line = obe_scene["Tiles"]["width"];
    options.object.items_per_line = 1;
    std::string scene_dump = vili::writer::dump(obe_scene, options);
    std::ofstream scene_file;
    scene_file.open(output_file);
    scene_file.write(scene_dump.c_str(), scene_dump.size());
    scene_file.close();
}

struct TiledIntegrationArgs
{
    std::string input_file;
    std::string output_file;
};

TiledIntegrationArgs parse_args(int argc, char** argv)
{
    using namespace obe::tiled_integration;

    TiledIntegrationArgs args;

    const lyra::cli cli = lyra::arg(args.input_file, "input_file").required(true)
        | lyra::arg(args.output_file, "output_file").required(true);
    const lyra::parse_result result = cli.parse({ argc, argv });
    logger->info("[ObEngine] Tiled Integration started");
    logger->info("  - Input file : {}", args.input_file);
    logger->info("  - Output file : {}", args.output_file);
    if (!result)
    {
        logger->error("Error in command line: {}", result.errorMessage());
        exit(1);
    }

    return args;
}

int main(int argc, char** argv)
{
    using namespace obe::tiled_integration;
    init_logger();

    const TiledIntegrationArgs args = parse_args(argc, argv);

#if defined _DEBUG || defined _NDEBUG || defined NDEBUG
    run(args.input_file, args.output_file);
#else
    try
    {
        run(args.input_file, args.output_file);
    }
    catch (const std::exception& e)
    {
        logger->error(e.what());
    }
#endif

    logger->info("Done :)");

    return 0;
}