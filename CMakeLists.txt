cmake_minimum_required(VERSION 3.14...3.24)
project(RNAB LANGUAGES C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_EXTENSIONS OFF)

# ─────────────────────────────────────────────
# Options
# ─────────────────────────────────────────────

option(RNAB_DEBUG           "Enable RNAB debug mode"                        OFF)
option(ENABLE_STRIP         "Strip symbols from binary (-s)"                ON)
option(BUILD_LIBRARY        "Build as shared library instead of executable" OFF)

include(CheckCSourceCompiles)

check_c_source_compiles("
    int main() {
        __uint128_t x = 0;
        return 0;
    }
" HAVE_UINT128)

if(NOT HAVE_UINT128)
    message(FATAL_ERROR "Compiler does not support __uint128_t")
endif()

set(TABLE_BITS 25 CACHE STRING "Transposition table size as power-of-two bits")

# Validate: must be integer
if(NOT TABLE_BITS MATCHES "^[0-9]+$")
    message(FATAL_ERROR "TABLE_BITS must be a non-negative integer")
endif()

# Range check
if(TABLE_BITS LESS 10 OR TABLE_BITS GREATER 32)
    message(FATAL_ERROR "TABLE_BITS out of supported range")
endif()

math(EXPR TABLE_SIZE "1 << ${TABLE_BITS}")

# Android always builds as library
if(ANDROID)
    set(BUILD_LIBRARY ON CACHE BOOL "Android always builds as library" FORCE)
    message(STATUS "Android target -> forcing BUILD_LIBRARY=ON")
endif()

# ─────────────────────────────────────────────
# Main target
# ─────────────────────────────────────────────

if(BUILD_LIBRARY)
    add_library(rnab SHARED rnab_engine/rnab.h)
    set_source_files_properties(rnab_engine/rnab.h PROPERTIES LANGUAGE C)
    message(STATUS "Building shared library (statically linked runtime)")
else()
    add_executable(rnab simple_gui/rnab_main.c)
    message(STATUS "Building executable")
endif()

target_include_directories(rnab PRIVATE rnab_engine)
target_compile_features(rnab PRIVATE c_std_17)
target_compile_options(rnab PRIVATE -O3)
target_compile_definitions(rnab PRIVATE TABLE_SIZE=${TABLE_SIZE})
set_target_properties(rnab PROPERTIES POSITION_INDEPENDENT_CODE ON)

if(RNAB_DEBUG)
    target_compile_definitions(rnab PRIVATE RNAB_DEBUG)
endif()

# ─────────────────────────────────────────────
# Link options
# ─────────────────────────────────────────────

if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
    target_link_options(rnab PRIVATE -fuse-ld=bfd -flto)
elseif(CMAKE_C_COMPILER_ID STREQUAL "Clang")
    target_link_options(rnab PRIVATE -fuse-ld=lld -flto)
endif()

target_compile_options(rnab PRIVATE -ffunction-sections -fdata-sections -fno-stack-protector -fomit-frame-pointer -fno-unwind-tables -fno-asynchronous-unwind-tables) 
target_link_options(rnab PRIVATE -Wl,--gc-sections)

# Static linking rules per platform:
#   Windows  -> always fully static (library or executable)
#   Android  -> only embed libgcc, leave libc/libm/libdl dynamic
#   Linux    -> static only for library; executable links dynamically
if(WIN32)
    target_link_options(rnab PRIVATE -static -static-libgcc)
    message(STATUS "Static linking: full (Windows)")
elseif(ANDROID)
    target_link_options(rnab PRIVATE -static-libgcc)
    message(STATUS "Static linking: libgcc only (Android)")
else() # Linux
    if(BUILD_LIBRARY)
        target_link_options(rnab PRIVATE -nostdlib)
        message(STATUS "Static linking: full (Linux library)")
    else()
        message(STATUS "Static linking: none (Linux executable)")
    endif()
endif()

if(ENABLE_STRIP)
    target_link_options(rnab PRIVATE -s)
endif()

# ─────────────────────────────────────────────
# Platform libraries (executable only)
# ─────────────────────────────────────────────

if(WIN32)
    target_link_libraries(rnab PRIVATE pthread)
endif()

if(NOT BUILD_LIBRARY)
    if(WIN32)
        target_link_libraries(rnab PRIVATE glfw3 zstd opengl32 gdi32 m)
    else()
        target_link_libraries(rnab PRIVATE GL glfw zstd m)
    endif()
endif()

# ─────────────────────────────────────────────
# Summary
# ─────────────────────────────────────────────

message(STATUS "Compiler:             ${CMAKE_C_COMPILER_ID} ${CMAKE_C_COMPILER_VERSION}")
message(STATUS "BUILD_LIBRARY:        ${BUILD_LIBRARY}")
message(STATUS "RNAB_DEBUG:           ${RNAB_DEBUG}")
message(STATUS "ENABLE_STRIP:         ${ENABLE_STRIP}")
message(STATUS "TABLE_SIZE:           ${TABLE_SIZE}")
