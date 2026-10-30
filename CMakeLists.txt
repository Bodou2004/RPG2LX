cmake_minimum_required(VERSION 3.16)
project(rpg2lx VERSION 0.1.0 LANGUAGES CXX)

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
)

target_include_directories(rpg2lx_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# CLI executable
add_executable(rpg2lx
    src/cli/main.cpp
)

target_link_libraries(rpg2lx PRIVATE rpg2lx_core)

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

find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND)
    add_test(NAME NativeCliTests COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_cli_native.py" "$<TARGET_FILE:rpg2lx>")
endif()
