# Collect z8lua sources (compile as C++)
file(GLOB Z8LUA_SOURCES "z8lua/*.c")
# Exclude files we don't need
list(FILTER Z8LUA_SOURCES EXCLUDE REGEX "lua\\.c$")      # standalone interpreter
list(FILTER Z8LUA_SOURCES EXCLUDE REGEX "luac\\.c$")     # compiler
list(FILTER Z8LUA_SOURCES EXCLUDE REGEX "liolib\\.c$")   # file I/O (not needed)
list(FILTER Z8LUA_SOURCES EXCLUDE REGEX "loslib\\.c$")   # OS functions (not needed)
list(FILTER Z8LUA_SOURCES EXCLUDE REGEX "loadlib\\.c$")  # dynamic loading (not needed)
list(FILTER Z8LUA_SOURCES EXCLUDE REGEX "ltests\\.c$")   # test functions (not needed)
list(FILTER Z8LUA_SOURCES EXCLUDE REGEX "lmathlib\\.c$") # standard math (use pico8 math instead)
list(FILTER Z8LUA_SOURCES EXCLUDE REGEX "ldblib\\.c$")   # debug lib (not needed)

# Add math stubs
list(APPEND Z8LUA_SOURCES "z8lua/math_stubs.cpp")

# Rename .c to .cpp for C++ compilation (z8lua needs C++ for fix32.h)
set(Z8LUA_CPP_SOURCES "")
foreach(src ${Z8LUA_SOURCES})
    string(REGEX REPLACE "\\.c$" ".cpp" cpp_src ${src})
    if(NOT EXISTS ${cpp_src} AND EXISTS ${src})
        # Create symlink or copy - for now we'll set language directly
        list(APPEND Z8LUA_CPP_SOURCES ${src})
    else()
        list(APPEND Z8LUA_CPP_SOURCES ${cpp_src})
    endif()
endforeach()

# Collect our sources (both .c and .cpp)
file(GLOB PICO_SOURCES "Source/*.c" "Source/*.cpp")

idf_component_register(
    SRCS ${PICO_SOURCES} ${Z8LUA_SOURCES}
    INCLUDE_DIRS "Include" "z8lua"
    REQUIRES TactilitySDK
)

# Force C files in z8lua to compile as C++
set_source_files_properties(${Z8LUA_SOURCES} PROPERTIES LANGUAGE CXX)

# z8lua compile options
target_compile_options(${COMPONENT_LIB} PRIVATE 
    -Os                     # Optimize for size
    -fno-exceptions         # No C++ exceptions
    -fno-rtti               # No RTTI
    -Wno-write-strings      # Suppress string literal warnings
)

# z8lua needs longjmp for error handling since we can't use C++ exceptions
target_compile_definitions(${COMPONENT_LIB} PRIVATE
    LUA_USE_LONGJMP         # Use setjmp/longjmp instead of C++ exceptions
)

# z8lua requires C++17 for fix32.h
set_property(TARGET ${COMPONENT_LIB} PROPERTY CXX_STANDARD 17)
set_property(TARGET ${COMPONENT_LIB} PROPERTY CXX_STANDARD_REQUIRED ON)

# Enable link-time optimization
set_property(TARGET ${COMPONENT_LIB} PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)

