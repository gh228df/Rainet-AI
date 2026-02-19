cmake_minimum_required(VERSION 3.14...3.24)
project(RNAB LANGUAGES CXX)

set(CMAKE_CXX_EXTENSIONS OFF)

add_compile_options(-O3)

# ─────────────────────────────────────────────
# Options
# ─────────────────────────────────────────────

option(RNAB_DEBUG    "Enable RNAB debug mode"                ON)
option(ENABLE_STRIP  "Strip symbols from binary (-s)"        ON)

# ─────────────────────────────────────────────
# Detect architecture family
# ─────────────────────────────────────────────

string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" PROC_LOWER)

if(PROC_LOWER MATCHES "x86_64|amd64|i[3-9]86|i686|x86|i386")
    set(IS_X86 TRUE)
    message(STATUS "Building for x86-family -> including SIMD variants")
else()
    set(IS_X86 FALSE)
    message(STATUS "Non-x86 architecture (${CMAKE_SYSTEM_PROCESSOR}) -> scalar only")
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
# Common source files
# ─────────────────────────────────────────────

set(COMMON_SOURCES rnab_main.cpp)

set(HEADER_AS_SOURCE rnab_impl.hpp)
set_source_files_properties(${HEADER_AS_SOURCE} PROPERTIES LANGUAGE CXX)

# ─────────────────────────────────────────────
# SIMD object libraries
# ─────────────────────────────────────────────

# Scalar (always built)
add_library(rnab_scalar OBJECT ${HEADER_AS_SOURCE})
target_compile_definitions(rnab_scalar PRIVATE TU_COMPILE SIMD_SUFFIX=scalar)
apply_common_options(rnab_scalar)

if(IS_X86)
    # SSE4.2
    add_library(rnab_sse42 OBJECT ${HEADER_AS_SOURCE})
    target_compile_options(rnab_sse42 PRIVATE -msse4.2)
    target_compile_definitions(rnab_sse42 PRIVATE TU_COMPILE SIMD_SUFFIX=sse4_2)
    apply_common_options(rnab_sse42)

    # AVX
    add_library(rnab_avx OBJECT ${HEADER_AS_SOURCE})
    target_compile_options(rnab_avx PRIVATE -mavx)
    target_compile_definitions(rnab_avx PRIVATE TU_COMPILE SIMD_SUFFIX=avx)
    apply_common_options(rnab_avx)

    # AVX2
    add_library(rnab_avx2 OBJECT ${HEADER_AS_SOURCE})
    target_compile_options(rnab_avx2 PRIVATE -mavx2 -mfma)
    target_compile_definitions(rnab_avx2 PRIVATE TU_COMPILE SIMD_SUFFIX=avx2)
    apply_common_options(rnab_avx2)

    # AVX-512
    add_library(rnab_avx512 OBJECT ${HEADER_AS_SOURCE})
    target_compile_options(rnab_avx512 PRIVATE -mavx512f -mavx512vl -mavx512bw -mavx512dq)
    target_compile_definitions(rnab_avx512 PRIVATE TU_COMPILE SIMD_SUFFIX=avx512f)
    apply_common_options(rnab_avx512)
endif()

# ─────────────────────────────────────────────
# Executable
# ─────────────────────────────────────────────

add_executable(rnab
    ${COMMON_SOURCES}
    $<TARGET_OBJECTS:rnab_scalar>
)

if(IS_X86)
    target_sources(rnab PRIVATE
        $<TARGET_OBJECTS:rnab_sse42>
        $<TARGET_OBJECTS:rnab_avx>
        $<TARGET_OBJECTS:rnab_avx2>
        $<TARGET_OBJECTS:rnab_avx512>
    )
endif()

target_include_directories(rnab PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

apply_common_options(rnab)

# ─────────────────────────────────────────────
# Link-time options
# ─────────────────────────────────────────────

if(ENABLE_STRIP)
    target_link_options(rnab PRIVATE -s)
    message(STATUS "Symbol stripping enabled")
endif()

target_link_libraries(rnab PRIVATE GL glfw zstd)

# ─────────────────────────────────────────────
# Linker selection (Clang only)
# ─────────────────────────────────────────────

target_link_options(rnab PRIVATE -fuse-ld=lld)
message(STATUS "Using LLD linker")

# ─────────────────────────────────────────────
# Summary
# ─────────────────────────────────────────────

message(STATUS "RNAB_DEBUG   : ${RNAB_DEBUG}")
message(STATUS "ENABLE_STRIP : ${ENABLE_STRIP}")
