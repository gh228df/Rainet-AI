cmake_minimum_required(VERSION 3.14...3.24)
project(RNAB LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_EXTENSIONS OFF)

# ─────────────────────────────────────────────
# Options
# ─────────────────────────────────────────────

option(RNAB_DEBUG    "Enable RNAB debug mode"                        OFF)
option(ENABLE_STRIP  "Strip symbols from binary (-s)"                ON)
option(BUILD_LIBRARY "Build as shared library instead of executable" OFF)

# Android always builds as library
if(ANDROID)
    set(BUILD_LIBRARY ON CACHE BOOL "Android always builds as library" FORCE)
    message(STATUS "Android target -> forcing BUILD_LIBRARY=ON")
endif()

# ─────────────────────────────────────────────
# Dependencies
# ─────────────────────────────────────────────

include(FetchContent)

FetchContent_Declare(
    boost_unordered
    GIT_REPOSITORY https://github.com/MikePopoloski/boost_unordered.git
    GIT_SHALLOW    TRUE
    # No GIT_TAG: always fetches the latest commit on the default branch.
    # Pin to a tag (e.g. GIT_TAG v1.88.0) if you need reproducible builds.
)

FetchContent_MakeAvailable(boost_unordered)

# ─────────────────────────────────────────────
# Detect architecture
# ─────────────────────────────────────────────

string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" _proc)
if(_proc MATCHES "x86_64|amd64|i[3-9]86|i686|x86|i386")
    set(IS_X86 TRUE)
    message(STATUS "x86 architecture -> SIMD variants will be built")
else()
    set(IS_X86 FALSE)
    message(STATUS "Non-x86 architecture (${CMAKE_SYSTEM_PROCESSOR}) -> scalar only")
endif()

# ─────────────────────────────────────────────
# Helper: options shared across all object libs
# ─────────────────────────────────────────────

function(configure_object_lib target)
    target_include_directories(${target} PRIVATE
        ${boost_unordered_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}
    )
    target_compile_features(${target} PRIVATE cxx_std_20)
    set_target_properties(${target} PROPERTIES POSITION_INDEPENDENT_CODE ON)
    if(RNAB_DEBUG)
        target_compile_definitions(${target} PRIVATE RNAB_DEBUG)
    endif()
endfunction()

# ─────────────────────────────────────────────
# SIMD object libraries
# ─────────────────────────────────────────────

set(IMPL_HEADER rnab_engine/rnab_impl.hpp)
set_source_files_properties(${IMPL_HEADER} PROPERTIES LANGUAGE CXX)

add_library(rnab_scalar OBJECT ${IMPL_HEADER})
target_compile_definitions(rnab_scalar PRIVATE TU_COMPILE SIMD_SUFFIX=scalar)
configure_object_lib(rnab_scalar)

if(IS_X86)
    foreach(variant IN ITEMS
            "sse42;-msse4.2;sse4_2"
            "avx;-mavx;avx"
            "avx2;-mavx2 -mfma;avx2"
            "avx512;-mavx512f -mavx512vl -mavx512bw -mavx512dq;avx512f")

        list(GET variant 0 name)
        list(GET variant 1 flags)
        list(GET variant 2 suffix)

        add_library(rnab_${name} OBJECT ${IMPL_HEADER})
        separate_arguments(flag_list UNIX_COMMAND "${flags}")
        target_compile_options(rnab_${name} PRIVATE ${flag_list})
        target_compile_definitions(rnab_${name} PRIVATE TU_COMPILE SIMD_SUFFIX=${suffix})
        configure_object_lib(rnab_${name})
    endforeach()
endif()

# ─────────────────────────────────────────────
# Collect all object libraries
# ─────────────────────────────────────────────

set(ALL_OBJECTS $<TARGET_OBJECTS:rnab_scalar>)
if(IS_X86)
    list(APPEND ALL_OBJECTS
        $<TARGET_OBJECTS:rnab_sse42>
        $<TARGET_OBJECTS:rnab_avx>
        $<TARGET_OBJECTS:rnab_avx2>
        $<TARGET_OBJECTS:rnab_avx512>
    )
endif()

# ─────────────────────────────────────────────
# Main target
# ─────────────────────────────────────────────

if(BUILD_LIBRARY)
    set(MAIN_SOURCE rnab_engine/rnab.hpp)
    set_source_files_properties(${MAIN_SOURCE} PROPERTIES LANGUAGE CXX)
    add_library(rnab SHARED ${MAIN_SOURCE} ${ALL_OBJECTS})
    message(STATUS "Building shared library (statically linked runtime)")
else()
    set(MAIN_SOURCE simple_gui/rnab_main.cpp)
    add_executable(rnab ${MAIN_SOURCE} ${ALL_OBJECTS})
    message(STATUS "Building executable")
endif()

target_include_directories(rnab PRIVATE
    ${boost_unordered_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_compile_features(rnab PRIVATE cxx_std_20)
target_compile_options(rnab PRIVATE -O3)

if(RNAB_DEBUG)
    target_compile_definitions(rnab PRIVATE RNAB_DEBUG)
endif()

# ─────────────────────────────────────────────
# Link options
# ─────────────────────────────────────────────

target_link_options(rnab PRIVATE -fuse-ld=lld -flto)

# Static linking rules per platform:
#   Windows  -> always fully static (library or executable)
#   Android  -> only embed libstdc++/libgcc, leave libc/libm/libdl dynamic
#   Linux    -> static only for library; executable links dynamically
if(WIN32)
    target_link_options(rnab PRIVATE -static -static-libstdc++ -static-libgcc)
    message(STATUS "Static linking: full (Windows)")
elseif(ANDROID)
    target_link_options(rnab PRIVATE -static-libstdc++ -static-libgcc)
    message(STATUS "Static linking: libstdc++/libgcc only (Android)")
else() # Linux
    if(BUILD_LIBRARY)
        target_link_options(rnab PRIVATE -static -static-libstdc++ -static-libgcc)
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
        target_link_libraries(rnab PRIVATE glfw3 zstd opengl32 gdi32)
    else()
        target_link_libraries(rnab PRIVATE GL glfw zstd)
    endif()
endif()

# ─────────────────────────────────────────────
# Summary
# ─────────────────────────────────────────────

message(STATUS "Compiler:      ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "BUILD_LIBRARY: ${BUILD_LIBRARY}")
message(STATUS "RNAB_DEBUG:    ${RNAB_DEBUG}")
message(STATUS "ENABLE_STRIP:  ${ENABLE_STRIP}")
message(STATUS "IS_X86:        ${IS_X86}")
