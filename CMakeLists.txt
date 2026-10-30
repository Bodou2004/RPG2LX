cmake_minimum_required(VERSION 3.16)
project(rpg2lx VERSION 0.1.0 LANGUAGES CXX)
option(RPG2LX_WITH_SDL "Build the graphical game library menu" OFF)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Add compiler warnings
if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

include_directories(include)

# Core library
add_library(rpg2lx_core STATIC
    src/core/inspector.cpp
    src/mv/map.cpp
    src/launcher/library.cpp
    src/launcher/model.cpp
)

target_include_directories(rpg2lx_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# CLI executable
add_executable(rpg2lx
    src/cli/main.cpp
    src/launcher/frontend.cpp
)

target_link_libraries(rpg2lx PRIVATE rpg2lx_core)
if(WIN32)
    target_link_libraries(rpg2lx PRIVATE shell32)
endif()
if(RPG2LX_WITH_SDL)
    find_package(SDL2 CONFIG REQUIRED)
    target_compile_definitions(rpg2lx PRIVATE RPG2LX_WITH_SDL)
    target_include_directories(rpg2lx PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/third_party/font8x8")
    target_link_libraries(rpg2lx PRIVATE SDL2::SDL2)
    if(WIN32 AND TARGET SDL2::SDL2)
        add_custom_command(TARGET rpg2lx POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:SDL2::SDL2>" "$<TARGET_FILE_DIR:rpg2lx>")
    endif()
endif()

# Tests
enable_testing()
add_executable(test_inspector
    tests/test_inspector.cpp
)

target_link_libraries(test_inspector PRIVATE rpg2lx_core)

# CTest definition passing absolute fixtures path
add_test(NAME InspectorTests COMMAND test_inspector "${CMAKE_CURRENT_SOURCE_DIR}/fixtures")
set_tests_properties(InspectorTests PROPERTIES
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
)

add_executable(test_json tests/test_json.cpp)
target_link_libraries(test_json PRIVATE rpg2lx_core)
add_test(NAME JsonTests COMMAND test_json)

add_executable(test_map tests/test_map.cpp)
target_link_libraries(test_map PRIVATE rpg2lx_core)
add_test(NAME MapTests COMMAND test_map "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/valid_project/data/Map001.json")

add_executable(test_library tests/test_library.cpp)
target_link_libraries(test_library PRIVATE rpg2lx_core)
add_test(NAME LibraryTests COMMAND test_library "${CMAKE_CURRENT_SOURCE_DIR}/fixtures")

if(RPG2LX_WITH_SDL)
    add_test(NAME LauncherSmoke COMMAND rpg2lx launcher --home "${CMAKE_CURRENT_BINARY_DIR}/smoke-home"
        --smoke-frames 2)
    set_tests_properties(LauncherSmoke PROPERTIES ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")
endif()

find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND)
    add_test(NAME NativeCliTests COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_cli_native.py" "$<TARGET_FILE:rpg2lx>")
endif()
