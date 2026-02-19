set(CMAKE_C_COMPILER   "clang"   CACHE STRING "C compiler" FORCE)
set(CMAKE_CXX_COMPILER "clang++" CACHE STRING "C++ compiler" FORCE)

cmake_minimum_required(VERSION 3.14...3.24)
project(RNAB LANGUAGES CXX)

set(CMAKE_CXX_EXTENSIONS OFF)

# ─────────────────────────────────────────────
# Options
# ─────────────────────────────────────────────

option(RNAB_DEBUG    "Enable RNAB debug mode"                ON)
option(ENABLE_STRIP  "Strip symbols from binary (-s)"        ON)
option(BUILD_LIBRARY "Build as shared library instead of executable" OFF)

if(WIN32)
    option(BUILD_STATIC "Build static executable (Windows only)" ON)
    message(STATUS "Windows detected → static build option available (default ON)")
else()
    set(BUILD_STATIC OFF CACHE BOOL "Static build (disabled on non-Windows)" FORCE)
    message(STATUS "Non-Windows platform → forcing dynamic build (BUILD_STATIC=OFF)")
endif()

# ─────────────────────────────────────────────
# Global compile options
# ─────────────────────────────────────────────

add_compile_options(-O3 -flto)  # Enable -flto universally

# ─────────────────────────────────────────────
# Static build flags (Windows only, for executable)
# ─────────────────────────────────────────────

if(BUILD_STATIC AND NOT BUILD_LIBRARY)
    set(BUILD_SHARED_LIBS OFF)
    set(CMAKE_FIND_LIBRARY_SUFFIXES ".a")  # prefer .a over .dll/.so
    add_link_options(
        -static
        -static-libgcc
        -static-libstdc++
    )
    message(STATUS "Static linking enabled for executable")
endif()

# ─────────────────────────────────────────────
# Detect architecture family
# ─────────────────────────────────────────────

string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" PROC_LOWER)

if(PROC_LOWER MATCHES "x86_64|amd64|i[3-9]86|i686|x86|i386")
    set(IS_X86 TRUE)
    message(STATUS "Building for x86-family → including SIMD variants")
else()
    set(IS_X86 FALSE)
    message(STATUS "Non-x86 architecture (${CMAKE_SYSTEM_PROCESSOR}) → scalar only")
endif()

# ─────────────────────────────────────────────
# Helper to apply common per-target definitions
# ─────────────────────────────────────────────

function(apply_common_options target)
    if(RNAB_DEBUG)
        target_compile_definitions(${target} PRIVATE RNAB_DEBUG)
    endif()
endfunction()

# ─────────────────────────────────────────────
# Common source files (varies by build mode)
# ─────────────────────────────────────────────

if(BUILD_LIBRARY)
    set(COMMON_SOURCES rnab_engine/rnab.hpp)
    message(STATUS "Building as library -> using rnab.hpp as dispatcher source")
else()
    set(COMMON_SOURCES simple_gui/rnab_main.cpp)
    message(STATUS "Building as executable -> using rnab_main.cpp")
endif()

set(HEADER_AS_SOURCE rnab_engine/rnab_impl.hpp)
set_source_files_properties(${HEADER_AS_SOURCE} ${COMMON_SOURCES} PROPERTIES LANGUAGE CXX)

# ─────────────────────────────────────────────
# SIMD object libraries
# ─────────────────────────────────────────────

add_library(rnab_scalar OBJECT ${HEADER_AS_SOURCE})
target_compile_definitions(rnab_scalar PRIVATE TU_COMPILE SIMD_SUFFIX=scalar)
apply_common_options(rnab_scalar)

if(IS_X86)
    add_library(rnab_sse42 OBJECT ${HEADER_AS_SOURCE})
    target_compile_options(rnab_sse42 PRIVATE -msse4.2)
    target_compile_definitions(rnab_sse42 PRIVATE TU_COMPILE SIMD_SUFFIX=sse4_2)
    apply_common_options(rnab_sse42)

    add_library(rnab_avx OBJECT ${HEADER_AS_SOURCE})
    target_compile_options(rnab_avx PRIVATE -mavx)
    target_compile_definitions(rnab_avx PRIVATE TU_COMPILE SIMD_SUFFIX=avx)
    apply_common_options(rnab_avx)

    add_library(rnab_avx2 OBJECT ${HEADER_AS_SOURCE})
    target_compile_options(rnab_avx2 PRIVATE -mavx2 -mfma)
    target_compile_definitions(rnab_avx2 PRIVATE TU_COMPILE SIMD_SUFFIX=avx2)
    apply_common_options(rnab_avx2)

    add_library(rnab_avx512 OBJECT ${HEADER_AS_SOURCE})
    target_compile_options(rnab_avx512 PRIVATE -mavx512f -mavx512vl -mavx512bw -mavx512dq)
    target_compile_definitions(rnab_avx512 PRIVATE TU_COMPILE SIMD_SUFFIX=avx512f)
    apply_common_options(rnab_avx512)
endif()

# ─────────────────────────────────────────────
# Main target (executable or library)
# ─────────────────────────────────────────────

if(BUILD_LIBRARY)
    add_library(rnab SHARED
        ${COMMON_SOURCES}
        $<TARGET_OBJECTS:rnab_scalar>
    )
    message(STATUS "Building shared library: librnab.so (or .dll on Windows)")
else()
    add_executable(rnab
        ${COMMON_SOURCES}
        $<TARGET_OBJECTS:rnab_scalar>
    )
    message(STATUS "Building executable: rnab")
endif()

if(IS_X86)
    if(BUILD_LIBRARY)
        target_sources(rnab PRIVATE
            $<TARGET_OBJECTS:rnab_sse42>
            $<TARGET_OBJECTS:rnab_avx>
            $<TARGET_OBJECTS:rnab_avx2>
            $<TARGET_OBJECTS:rnab_avx512>
        )
    else()
        target_sources(rnab PRIVATE
            $<TARGET_OBJECTS:rnab_sse42>
            $<TARGET_OBJECTS:rnab_avx>
            $<TARGET_OBJECTS:rnab_avx2>
            $<TARGET_OBJECTS:rnab_avx512>
        )
    endif()
endif()

target_include_directories(rnab PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
apply_common_options(rnab)

# ─────────────────────────────────────────────
# Platform-specific libraries (executable only)
# ─────────────────────────────────────────────

if(NOT BUILD_LIBRARY)
    if(WIN32)
        target_link_libraries(rnab PRIVATE
            glfw3
            zstd
            opengl32
            gdi32
            pthread
        )
    else()
        target_link_libraries(rnab PRIVATE
            GL
            glfw
            zstd
        )
    endif()
endif()

# ─────────────────────────────────────────────
# Link-time options (executable only)
# ─────────────────────────────────────────────

if(NOT BUILD_LIBRARY)
    # This works on Linux (ld.lld) and Windows (lld-link when using MinGW-style clang)
    target_link_options(rnab PRIVATE -fuse-ld=lld -flto)  # Enable -flto at link time

    if(ENABLE_STRIP)
        target_link_options(rnab PRIVATE -s)
        message(STATUS "Symbol stripping enabled (-s) for executable")
    endif()
endif()

# ─────────────────────────────────────────────
# Summary
# ─────────────────────────────────────────────

message(STATUS "Compiler:      ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
if(NOT BUILD_LIBRARY)
    message(STATUS "Linker flag:   -fuse-ld=lld")
endif()
message(STATUS "BUILD_LIBRARY: ${BUILD_LIBRARY}")
message(STATUS "BUILD_STATIC:  ${BUILD_STATIC}")
message(STATUS "RNAB_DEBUG:    ${RNAB_DEBUG}")
message(STATUS "ENABLE_STRIP:  ${ENABLE_STRIP}")
