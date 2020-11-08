project(lyra)

file(GLOB_RECURSE NLOHMANN_HEADERS include/lyra/*.hpp)

add_library(lyra INTERFACE)

target_include_directories(lyra
    INTERFACE
        $<INSTALL_INTERFACE:include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)