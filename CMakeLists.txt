cmake_minimum_required(VERSION 3.20)
project(yeetee C)

list(APPEND CMAKE_MODULE_PATH "$ENV{HOME}/.config/cmake")
include(std)

find_package(PkgConfig REQUIRED)
pkg_check_modules(NOTCURSES REQUIRED IMPORTED_TARGET notcurses)
pkg_check_modules(MPV REQUIRED IMPORTED_TARGET mpv)
pkg_check_modules(DANGLING REQUIRED IMPORTED_TARGET dangling)
pkg_check_modules(OPENSSL REQUIRED IMPORTED_TARGET openssl)

add_executable(yeetee
    src/main.c
    src/core/err.c
    src/core/conf.c
    src/auth/oauth.c
    src/auth/token.c
    src/api/innertube.c
    src/api/json.c
    src/api/ytdlp.c
    src/player/player.c
    src/player/render.c
    src/tui/tui.c
    src/tui/layout.c
    src/tui/input.c
    src/tui/feed.c
    src/tui/thumb.c
    src/tui/queue.c
    ext/cjson/cJSON.c
)

target_include_directories(yeetee PRIVATE include ext/cjson)
target_link_libraries(yeetee PRIVATE
    PkgConfig::DANGLING
    PkgConfig::NOTCURSES
    PkgConfig::MPV
    PkgConfig::OPENSSL
    m
)

std_invalidate_test_state(yeetee)

# tests
add_executable(test_auth tests/test_auth.c src/auth/oauth.c src/auth/token.c src/api/json.c src/core/err.c src/core/conf.c ext/cjson/cJSON.c)
target_include_directories(test_auth PRIVATE include ext/cjson)
target_link_libraries(test_auth PRIVATE PkgConfig::DANGLING PkgConfig::OPENSSL m)

add_executable(test_api tests/test_api.c src/api/innertube.c src/api/json.c src/api/ytdlp.c src/core/err.c ext/cjson/cJSON.c)
target_include_directories(test_api PRIVATE include ext/cjson)
target_link_libraries(test_api PRIVATE PkgConfig::DANGLING PkgConfig::OPENSSL m)

add_executable(test_player tests/test_player.c src/player/player.c src/core/err.c)
target_include_directories(test_player PRIVATE include)
target_link_libraries(test_player PRIVATE PkgConfig::DANGLING PkgConfig::MPV)

add_executable(test_tui tests/test_tui.c src/tui/input.c src/tui/queue.c src/core/err.c)
target_include_directories(test_tui PRIVATE include)
target_link_libraries(test_tui PRIVATE PkgConfig::DANGLING PkgConfig::NOTCURSES)

qemu_add_test(NAME auth_tests COMMAND test_auth ARCH x86_64)
qemu_add_test(NAME api_tests COMMAND test_api ARCH x86_64)
qemu_add_test(NAME player_tests COMMAND test_player ARCH x86_64)
qemu_add_test(NAME tui_tests COMMAND test_tui ARCH x86_64)

qemu_add_run_target()
