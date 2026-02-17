cmake_minimum_required(VERSION 3.16)
project(picotility_tests C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Source files from main/
set(PICO_SOURCES
    ${CMAKE_SOURCE_DIR}/../main/Source/pico_vm.c
    ${CMAKE_SOURCE_DIR}/../main/Source/pico_ram.c
    ${CMAKE_SOURCE_DIR}/../main/Source/pico_cart.c
    ${CMAKE_SOURCE_DIR}/../main/Source/pico_png_cart.c
    ${CMAKE_SOURCE_DIR}/../main/Source/pico_graphics.c
    ${CMAKE_SOURCE_DIR}/../main/Source/pico_audio.c
    ${CMAKE_SOURCE_DIR}/../main/Source/pico_input.c
    ${CMAKE_SOURCE_DIR}/../main/Source/pico_lua_api.cpp
    ${CMAKE_SOURCE_DIR}/../main/Source/fontdata.cpp
)

# z8lua sources (compile as C++)
file(GLOB Z8LUA_SOURCES "${CMAKE_SOURCE_DIR}/../main/z8lua/*.c")
list(FILTER Z8LUA_SOURCES EXCLUDE REGEX "lua\\.c$")
list(FILTER Z8LUA_SOURCES EXCLUDE REGEX "luac\\.c$")
list(FILTER Z8LUA_SOURCES EXCLUDE REGEX "liolib\\.c$")
list(FILTER Z8LUA_SOURCES EXCLUDE REGEX "loslib\\.c$")
list(FILTER Z8LUA_SOURCES EXCLUDE REGEX "loadlib\\.c$")
list(FILTER Z8LUA_SOURCES EXCLUDE REGEX "ltests\\.c$")
list(FILTER Z8LUA_SOURCES EXCLUDE REGEX "lmathlib\\.c$")
list(FILTER Z8LUA_SOURCES EXCLUDE REGEX "ldblib\\.c$")

# Add C++ files
set(Z8LUA_CPP_SOURCES
    ${Z8LUA_SOURCES}
    ${CMAKE_SOURCE_DIR}/../main/z8lua/math_stubs.cpp
)

# Add executable
add_executable(test_runner
    test_runner.cpp
    ${PICO_SOURCES}
    ${Z8LUA_CPP_SOURCES}
)

target_include_directories(test_runner PRIVATE
    ${CMAKE_SOURCE_DIR}/../main/Include
    ${CMAKE_SOURCE_DIR}/../main/z8lua
)

# Force z8lua C files to compile as C++
set_source_files_properties(${Z8LUA_SOURCES} PROPERTIES LANGUAGE CXX)

target_compile_options(test_runner PRIVATE
    -Wno-write-strings
    -DLUA_USE_LONGJMP
)
