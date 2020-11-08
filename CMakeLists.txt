cmake_minimum_required(VERSION 3.6)

project(tiled_integration)

add_subdirectory(extlibs/nlohmann)
add_subdirectory(extlibs/vili)
add_subdirectory(extlibs/lyra)
add_subdirectory(extlibs/spdlog)

set(TILED_INTEGRATION_HEADERS
    include/logger.hpp)
set(TILED_INTEGRATION_SOURCES
    src/main.cpp
    src/logger.cpp
)

add_executable(tiled_integration ${TILED_INTEGRATION_HEADERS} ${TILED_INTEGRATION_SOURCES})

target_link_libraries(tiled_integration nlohmann)
target_link_libraries(tiled_integration vili)
target_link_libraries(tiled_integration lyra)
target_link_libraries(tiled_integration spdlog)

target_include_directories(tiled_integration
    PUBLIC
        $<INSTALL_INTERFACE:include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 17)
set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD_REQUIRED ON)
set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_EXTENSIONS OFF)

if (NOT DEFINED BUILD_TESTS)
    set(BUILD_TESTS OFF CACHE BOOL "Build Tiled Integration Tests ?")
endif()

if (BUILD_TESTS)
    add_subdirectory(extlibs/catch)
    add_subdirectory(tests)
endif()