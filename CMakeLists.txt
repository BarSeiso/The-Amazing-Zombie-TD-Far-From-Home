cmake_minimum_required(VERSION 3.20)
project(FarFromHome LANGUAGES C)

include(FetchContent)
if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
FetchContent_Declare(raylib
    URL https://codeload.github.com/raysan5/raylib/tar.gz/c1ab645ca298a2801097931d1079b10ff7eb9df8
    URL_HASH SHA256=be3443c3fb97f7cbd18ae3f0ea2a73e6c1486128df33b639dce7e44070fc285d
    DOWNLOAD_NAME raylib-5.5.tar.gz)
FetchContent_MakeAvailable(raylib)

add_executable(far_from_home src/main.c src/game.c src/art.c src/audio.c src/story.c src/score.c)
target_compile_features(far_from_home PRIVATE c_std_99)
target_link_libraries(far_from_home PRIVATE raylib)
if(MSVC)
    target_compile_options(far_from_home PRIVATE /W4)
else()
    target_compile_options(far_from_home PRIVATE -Wall -Wextra -Wpedantic)
endif()
