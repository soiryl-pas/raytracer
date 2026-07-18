cmake_minimum_required(VERSION 3.28)
project(raytracer VERSION 0.0.2 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

add_subdirectory(lib)

add_executable(raytracer
    src/main.cpp
    src/image.cpp
    src/traceray.cpp
    src/random.cpp

    src/bvh/bvh.cpp
    src/bvh/bvh-sah.cpp

    src/geometry/aabb.cpp
    src/geometry/sphere.cpp
    src/geometry/mesh.cpp

    src/illum/material.cpp
    src/illum/phong.cpp

    src/parse/objparse.cpp
    src/parse/xmlparse.cpp
)

target_include_directories(raytracer PUBLIC include)
target_link_libraries(raytracer
    spng_static
    tinyxml2
)

target_compile_options(raytracer PRIVATE
    -Wall -Wextra -Wpedantic -Wconversion
    $<$<CONFIG:debug>:-Werror -fsanitize=address -fsanitize=undefined>
    $<$<CONFIG:release>:-O3>
)

target_link_options(raytracer PRIVATE
    $<$<CONFIG:debug>:-fsanitize=address -fsanitize=undefined>
)

if(RAYTRACER_TESTS)
    target_compile_definitions(raytracer PRIVATE RAYTRACER_TESTS)
endif()
