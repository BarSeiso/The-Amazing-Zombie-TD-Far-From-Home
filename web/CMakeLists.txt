cmake_minimum_required(VERSION 3.20)
project(FarFromHomeWeb LANGUAGES C)

if(NOT EMSCRIPTEN)
    message(FATAL_ERROR "Konfigurasi ini khusus Emscripten. Jalankan dengan: emcmake cmake -S web -B build-web")
endif()

include(FetchContent)
if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()

set(PLATFORM "Web" CACHE STRING "" FORCE)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES OFF CACHE BOOL "" FORCE)

FetchContent_Declare(raylib
    URL https://codeload.github.com/raysan5/raylib/tar.gz/c1ab645ca298a2801097931d1079b10ff7eb9df8
    URL_HASH SHA256=be3443c3fb97f7cbd18ae3f0ea2a73e6c1486128df33b639dce7e44070fc285d
    DOWNLOAD_NAME raylib-5.5.tar.gz)
FetchContent_MakeAvailable(raylib)

add_executable(index
    main_web.c
    ../src/game.c
    ../src/art.c
    ../src/audio.c
    ../src/story.c
    ../src/score.c
)

target_include_directories(index PRIVATE ../src)
target_compile_features(index PRIVATE c_std_99)
target_compile_options(index PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(index PRIVATE raylib)

set(WEB_DIST_DIR "${CMAKE_BINARY_DIR}/dist")

set_target_properties(index PROPERTIES
    SUFFIX ".html"
    RUNTIME_OUTPUT_DIRECTORY "${WEB_DIST_DIR}"
)

target_link_options(index PRIVATE
    "-sUSE_GLFW=3"
    "-sINITIAL_MEMORY=67108864"
    "-sALLOW_MEMORY_GROWTH=1"
    "-sSTACK_SIZE=1048576"
    "-sEXPORTED_FUNCTIONS=_main,_web_unlock_audio,_web_set_mobile_mode,_web_sync_viewport,_web_cmd_play,_web_cmd_story_next,_web_cmd_story_skip,_web_cmd_select_shop,_web_cmd_place_confirm,_web_cmd_place_cancel,_web_cmd_tower_target,_web_cmd_tower_upgrade,_web_cmd_tower_sell,_web_cmd_tower_deselect,_web_cmd_start_or_speed,_web_cmd_toggle_pause,_web_cmd_toggle_help,_web_cmd_toggle_settings,_web_cmd_toggle_motion,_web_cmd_adjust_music,_web_cmd_adjust_sfx,_web_cmd_retry,_web_cmd_to_menu"
    "-sEXPORTED_RUNTIME_METHODS=ccall,UTF8ToString"
    "--shell-file=${CMAKE_CURRENT_SOURCE_DIR}/shell.html"
)

add_custom_command(TARGET index POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/mobile_ui.js"
        "${CMAKE_CURRENT_SOURCE_DIR}/manifest.json"
        "${CMAKE_CURRENT_SOURCE_DIR}/icon.svg"
        "${WEB_DIST_DIR}/"
)
