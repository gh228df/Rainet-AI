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

set(TABLE_BITS 22 CACHE STRING "Transposition table size as power-of-two bits")

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
target_compile_options(rnab PRIVATE -O3 -ffunction-sections -fdata-sections -fno-stack-protector -fno-exceptions -fomit-frame-pointer -fno-unwind-tables -fno-asynchronous-unwind-tables)
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

target_link_options(rnab PRIVATE -Wl,--gc-sections)

if(WIN32)
    if(BUILD_LIBRARY)
        target_compile_options(rnab PRIVATE -mno-stack-arg-probe -fno-stack-check -fno-builtin -ffreestanding)
        target_link_options(rnab PRIVATE -static -nostdlib -nodefaultlibs -mno-stack-arg-probe -Wl,--major-subsystem-version,5 -Wl,--minor-subsystem-version,1)
    else()
        target_compile_options(rnab PRIVATE -mno-stack-arg-probe -fno-stack-check -ffreestanding)
        target_link_options(rnab PRIVATE -static -mno-stack-arg-probe -Wl,--major-subsystem-version,5 -Wl,--minor-subsystem-version,1)
    endif()
    
    set(CMAKE_C_STANDARD_LIBRARIES "" CACHE STRING "" FORCE)
    target_link_libraries(rnab PRIVATE kernel32)

    message(STATUS "Static linking: full (Windows)")
elseif(ANDROID)
    string(REPLACE "-fstack-protector-strong" "" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
    string(REPLACE "-g" "" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
    string(REPLACE "-static-libstdc++" "" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
    string(REPLACE "-funwind-tables" "" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
    string(REPLACE "-D_FORTIFY_SOURCE=2" "" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")

    target_compile_options(rnab PRIVATE -fno-builtin -ffreestanding)
    target_link_options(rnab PRIVATE -nostdlib -static -Wl,--error-limit=0)

    message(STATUS "Static linking: full (Android)")
else() # Linux
    if(BUILD_LIBRARY)
        target_compile_options(rnab PRIVATE -fno-builtin -ffreestanding)
        target_link_options(rnab PRIVATE -nostdlib -static -nodefaultlibs)

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
