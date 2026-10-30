cmake_minimum_required(VERSION 3.16)
project(rpg2lx VERSION 0.5.0 LANGUAGES C CXX)
option(RPG2LX_WITH_SDL "Build the graphical game library menu and SDL2 MV renderer" OFF)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

include_directories(include)

add_library(rpg2lx_core STATIC
    src/core/inspector.cpp
    src/mv/map.cpp
    src/mv/session.cpp
    src/mv/game_state.cpp
    src/mv/project_source.cpp
    src/mv/tile_layout.cpp
    src/mv/mv_audio.cpp
    src/mv/mv_engine.cpp
    src/render/font_renderer.cpp
    src/launcher/library.cpp
    src/launcher/model.cpp
    src/js/js_runtime.cpp
    third_party/quickjs/dtoa.c
    third_party/quickjs/libregexp.c
    third_party/quickjs/libunicode.c
    third_party/quickjs/quickjs.c
    third_party/stb/stb_vorbis.c
)

target_include_directories(rpg2lx_core PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/quickjs"
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/font8x8"
)
target_include_directories(rpg2lx_core SYSTEM PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/stb"
)

if(RPG2LX_WITH_SDL)
    find_package(SDL2 CONFIG REQUIRED)
    target_compile_definitions(rpg2lx_core PUBLIC RPG2LX_WITH_SDL)
    target_link_libraries(rpg2lx_core PUBLIC SDL2::SDL2)
endif()

add_executable(rpg2lx
    src/cli/main.cpp
    src/launcher/frontend.cpp
)

target_link_libraries(rpg2lx PRIVATE rpg2lx_core)
if(WIN32)
    target_link_libraries(rpg2lx PRIVATE shell32)
endif()
if(RPG2LX_WITH_SDL)
    target_sources(rpg2lx PRIVATE src/render/map_view.cpp src/launcher/library_view.cpp)
    if(WIN32 AND TARGET SDL2::SDL2)
        add_custom_command(TARGET rpg2lx POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:SDL2::SDL2>" "$<TARGET_FILE_DIR:rpg2lx>")
    endif()
endif()

enable_testing()
add_executable(test_inspector tests/test_inspector.cpp)
target_link_libraries(test_inspector PRIVATE rpg2lx_core)
add_test(NAME InspectorTests COMMAND test_inspector "${CMAKE_CURRENT_SOURCE_DIR}/fixtures")
set_tests_properties(InspectorTests PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")

add_executable(test_json tests/test_json.cpp)
target_link_libraries(test_json PRIVATE rpg2lx_core)
add_test(NAME JsonTests COMMAND test_json)

add_executable(test_map tests/test_map.cpp)
target_link_libraries(test_map PRIVATE rpg2lx_core)
add_test(NAME MapTests COMMAND test_map "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/valid_project/data/Map001.json")

add_executable(test_library tests/test_library.cpp)
target_link_libraries(test_library PRIVATE rpg2lx_core)
add_test(NAME LibraryTests COMMAND test_library "${CMAKE_CURRENT_SOURCE_DIR}/fixtures")

add_executable(test_session tests/test_session.cpp)
target_link_libraries(test_session PRIVATE rpg2lx_core)
add_test(NAME SessionTests COMMAND test_session "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/playable_project")

add_executable(test_game_state tests/test_game_state.cpp)
target_link_libraries(test_game_state PRIVATE rpg2lx_core)
add_test(NAME GameStateTests COMMAND test_game_state "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/playable_project")

add_executable(test_js_runtime tests/test_js_runtime.cpp)
target_link_libraries(test_js_runtime PRIVATE rpg2lx_core)
add_test(NAME JsRuntimeTests COMMAND test_js_runtime)

add_executable(project_scenario tests/project_scenario.cpp)
target_link_libraries(project_scenario PRIVATE rpg2lx_core)

if(RPG2LX_WITH_SDL)
    target_sources(project_scenario PRIVATE src/render/map_view.cpp)
    add_executable(test_game_view tests/test_game_view.cpp src/render/map_view.cpp src/launcher/library_view.cpp)
    target_link_libraries(test_game_view PRIVATE rpg2lx_core)
    add_test(NAME GameViewTests COMMAND test_game_view "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/playable_project")
    set_tests_properties(GameViewTests PROPERTIES ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

    add_executable(test_mv_engine tests/test_mv_engine.cpp)
    target_link_libraries(test_mv_engine PRIVATE rpg2lx_core)
    add_test(NAME MvEngineTests COMMAND test_mv_engine)
    set_tests_properties(MvEngineTests PROPERTIES ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")

    add_test(NAME LauncherSmoke COMMAND rpg2lx launcher --home "${CMAKE_CURRENT_BINARY_DIR}/smoke-home" --smoke-frames 2)
    set_tests_properties(LauncherSmoke PROPERTIES ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")
endif()

find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND)
    add_test(NAME NativeCliTests COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_cli_native.py" "$<TARGET_FILE:rpg2lx>")
    if(RPG2LX_WITH_SDL)
        add_test(NAME NativeGameSmoke COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_game_smoke.py" "$<TARGET_FILE:rpg2lx>")
    endif()
endif()
