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

set_target_properties(index PROPERTIES
    SUFFIX ".html"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/dist"
)

target_link_options(index PRIVATE
    "-sUSE_GLFW=3"
    "-sINITIAL_MEMORY=67108864"
    "-sALLOW_MEMORY_GROWTH=1"
    "-sSTACK_SIZE=1048576"
    "-sEXPORTED_FUNCTIONS=_main,_web_unlock_audio"
    "-sEXPORTED_RUNTIME_METHODS=ccall"
    "--shell-file=${CMAKE_CURRENT_SOURCE_DIR}/shell.html"
)
