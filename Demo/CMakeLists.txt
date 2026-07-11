cmake_minimum_required(VERSION 3.13)
project(picotility_demo C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Source paths (relative to project root)
set(PICO_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../main)

# z8lua sources (force-compiled as C++)
file(GLOB Z8LUA_SOURCES "${PICO_ROOT}/z8lua/*.c")

# Exclude standalone tools, I/O, OS, dynamic loading, tests, std math, debug
list(FILTER Z8LUA_SOURCES EXCLUDE REGEX "lua\\.c$")
list(FILTER Z8LUA_SOURCES EXCLUDE REGEX "luac\\.c$")
list(FILTER Z8LUA_SOURCES EXCLUDE REGEX "liolib\\.c$")
list(FILTER Z8LUA_SOURCES EXCLUDE REGEX "loslib\\.c$")
list(FILTER Z8LUA_SOURCES EXCLUDE REGEX "loadlib\\.c$")
list(FILTER Z8LUA_SOURCES EXCLUDE REGEX "ltests\\.c$")
list(FILTER Z8LUA_SOURCES EXCLUDE REGEX "lmathlib\\.c$")
list(FILTER Z8LUA_SOURCES EXCLUDE REGEX "ldblib\\.c$")

# Add PICO-8 math stubs (replaces standard math)
list(APPEND Z8LUA_SOURCES "${PICO_ROOT}/z8lua/math_stubs.cpp")

# Emulator core sources
# Exclude the Tactility/LVGL shell files: Picotility.cpp, main.cpp
file(GLOB PICO_SOURCES "${PICO_ROOT}/Source/*.c" "${PICO_ROOT}/Source/*.cpp")
list(FILTER PICO_SOURCES EXCLUDE REGEX "Picotility\\.cpp$")
list(FILTER PICO_SOURCES EXCLUDE REGEX "main\\.cpp$")

# WASM shim
set(SHIM_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/wasm_shim.c")

# Emscripten link flags (set before add_executable)
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} \
  -O2 \
  -s WASM=1 \
  -s MODULARIZE=1 \
  -s EXPORT_NAME=createPicotilityModule \
  -s ALLOW_MEMORY_GROWTH=0 \
  -s INITIAL_MEMORY=4194304 \
  -s MAXIMUM_MEMORY=4194304 \
  -s STACK_SIZE=1048576 \
  -s FILESYSTEM=0 \
  -s ENVIRONMENT=web \
  -s EXPORTED_FUNCTIONS=[_wasm_init,_wasm_shutdown,_wasm_load_cart,_wasm_run,_wasm_step,_wasm_stop,_wasm_set_button,_wasm_get_framebuffer,_wasm_get_dirty_top,_wasm_get_dirty_bottom,_wasm_needs_flip,_wasm_flip,_wasm_audio_fill,_wasm_get_fps,_wasm_get_frame_count,_wasm_get_error,_wasm_get_state,_malloc,_free] \
  -s EXPORTED_RUNTIME_METHODS=[ccall,cwrap,getValue,setValue,UTF8ToString,stringToUTF8,writeArrayToMemory] \
")

# Build the WASM module
add_executable(picotility ${PICO_SOURCES} ${Z8LUA_SOURCES} ${SHIM_SOURCES})

target_include_directories(picotility PRIVATE
    ${PICO_ROOT}/Include
    ${PICO_ROOT}/z8lua
)

target_compile_definitions(picotility PRIVATE
    LUA_USE_LONGJMP
)

# Force z8lua .c files to compile as C++
set_source_files_properties(${Z8LUA_SOURCES} PROPERTIES LANGUAGE CXX)

# z8lua compile options
set_source_files_properties(${Z8LUA_SOURCES} PROPERTIES
    COMPILE_OPTIONS "-Os;-fno-exceptions;-fno-rtti;-Wno-write-strings"
)

target_compile_options(picotility PRIVATE -O2 -flto)
