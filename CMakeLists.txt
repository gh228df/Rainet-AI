cmake_minimum_required(VERSION 3.14...3.24)
project(RNAB LANGUAGES C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_EXTENSIONS OFF)

# ─────────────────────────────────────────────
# Options
# ─────────────────────────────────────────────

option(RNAB_DEBUG       "Enable debug logging"                          OFF)
option(RNAB_BENCHMARK   "Include benchmark entry point"                 OFF)
option(ENABLE_STRIP     "Strip symbols from binary"                     ON)
option(BUILD_LIBRARY    "Build as shared library instead of executable" OFF)

set(TABLE_BITS 23 CACHE STRING "Transposition table size as power-of-two bits")

if(NOT TABLE_BITS MATCHES "^[0-9]+$")
    message(FATAL_ERROR "TABLE_BITS must be a non-negative integer")
endif()

# 26 is the relocation limit for most of the systems (> 2gb)

if(TABLE_BITS LESS 10 OR TABLE_BITS GREATER 26)
    message(FATAL_ERROR "TABLE_BITS out of supported range [10, 26]")
endif()

math(EXPR TABLE_SIZE "1 << ${TABLE_BITS}")

# Android is always a library
if(ANDROID)
    set(BUILD_LIBRARY ON CACHE BOOL "Android always builds as library" FORCE)
endif()

# ─────────────────────────────────────────────
# Target definition
# ─────────────────────────────────────────────

if(BUILD_LIBRARY)
    add_library(rnab SHARED rnab_engine/librnab.c)
else()
    add_executable(rnab simple_gui/rnab_main.c)
endif()

target_include_directories(rnab PRIVATE rnab_engine)
target_compile_definitions(rnab PRIVATE
    TABLE_SIZE=${TABLE_SIZE}
    $<$<BOOL:${RNAB_DEBUG}>:RNAB_DEBUG>
    $<$<BOOL:${RNAB_BENCHMARK}>:RNAB_BENCHMARK>
)

# ─────────────────────────────────────────────
# Compile flags
# ─────────────────────────────────────────────

target_compile_options(rnab PRIVATE
    -O3
    -ffunction-sections
    -fdata-sections
    -fomit-frame-pointer
    -fno-stack-protector
    -fno-exceptions
    -fno-unwind-tables
    -fno-asynchronous-unwind-tables
    -fno-ident
    -fvisibility=hidden
)

# ─────────────────────────────────────────────
# Link flags
# ─────────────────────────────────────────────

target_link_options(rnab PRIVATE
    -Wl,--gc-sections               # dead code elimination
    -Wl,--build-id=none             # no build-id note
    -Wl,--strip-all                 # strip all symbols
    $<$<BOOL:${ENABLE_STRIP}>:-s>
)

# ─────────────────────────────────────────────
# Compiler-specific flags
# ─────────────────────────────────────────────

if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
    target_link_options(rnab PRIVATE -fuse-ld=lld) # skip -flto
elseif(CMAKE_C_COMPILER_ID STREQUAL "Clang")
    target_compile_options(rnab PRIVATE -flto)
    target_link_options(rnab PRIVATE -fuse-ld=lld -flto)
endif()

# ─────────────────────────────────────────────
# libgcc compiler runtime symbols
# ─────────────────────────────────────────────

if (NOT ANDROID)
    execute_process(
        COMMAND ${CMAKE_C_COMPILER} --print-libgcc-file-name
        OUTPUT_VARIABLE LIBGCC_PATH
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    message(STATUS "libgcc: ${LIBGCC_PATH}")
    target_link_libraries(rnab PRIVATE ${LIBGCC_PATH})
endif()

# ─────────────────────────────────────────────
# Platform Windows
# ─────────────────────────────────────────────

if(WIN32)
    set(CMAKE_C_STANDARD_LIBRARIES "" CACHE STRING "" FORCE)

    target_compile_options(rnab PRIVATE
        -fno-stack-check
        -ffreestanding
        -mno-stack-arg-probe
        $<$<BOOL:${BUILD_LIBRARY}>:-fno-builtin>
    )
    target_link_options(rnab PRIVATE
        -static
        -nodefaultlibs
        -mno-stack-arg-probe
        -Wl,--no-insert-timestamp       # reproducible builds
        -Wl,--major-subsystem-version,5 # target XP and above
        -Wl,--minor-subsystem-version,1
        $<$<BOOL:${BUILD_LIBRARY}>:-nostdlib>
    )
    target_link_libraries(rnab PRIVATE kernel32)

# ─────────────────────────────────────────────
# Platform Android
# ─────────────────────────────────────────────

elseif(ANDROID)
    # The NDK toolchain injects flags we don't want, strip them all
    foreach(flag
        -fstack-protector-strong
        -funwind-tables
        -fno-limit-debug-info
        -static-libstdc++
        -D_FORTIFY_SOURCE=2
        -g
    )
        string(REPLACE "${flag}" "" CMAKE_C_FLAGS           "${CMAKE_C_FLAGS}")
        string(REPLACE "${flag}" "" CMAKE_C_FLAGS_RELEASE   "${CMAKE_C_FLAGS_RELEASE}")
    endforeach()

    # NDK also injects security link flags, strip those too
    foreach(flag -Wl,-z,relro -Wl,-z,now)
        string(REPLACE "${flag}" "" CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS}")
        string(REPLACE "${flag}" "" CMAKE_EXE_LINKER_FLAGS    "${CMAKE_EXE_LINKER_FLAGS}")
    endforeach()

    target_compile_options(rnab PRIVATE
        -fno-builtin
        -ffreestanding
    )
    target_link_options(rnab PRIVATE
        -nostdlib
        -static
        -Wl,-z,norelro              # no relro padding
        -Wl,--build-id=none         # NDK re-enables this, override it
        -Wl,--discard-all               # discard local symbols
        -Wl,--exclude-libs,ALL          # don't re-export static lib symbols
    )

# ─────────────────────────────────────────────
# Platform Linux
# ─────────────────────────────────────────────

else()
    if(BUILD_LIBRARY)
        target_compile_options(rnab PRIVATE
            -fno-builtin
            -ffreestanding
        )
        target_link_options(rnab PRIVATE
            -nostdlib
            -static
            -nodefaultlibs
            -Wl,-z,norelro          # no relro padding
            -Wl,-z,noseparate-code  # tighter segment layout
            -Wl,--no-eh-frame-hdr   # no .eh_frame_hdr
            -Wl,--discard-all               # discard local symbols
            -Wl,--exclude-libs,ALL          # don't re-export static lib symbols
        )
        find_program(OBJCOPY_EXECUTABLE
            NAMES llvm-objcopy objcopy
            REQUIRED
        )
        add_custom_command(TARGET rnab POST_BUILD
            COMMAND ${OBJCOPY_EXECUTABLE}
                --remove-section=.comment
                --remove-section=.note.gnu.property
                --remove-section=.note.gnu.gold-version
                --remove-section=.note.gnu.android.ident
                $<TARGET_FILE:rnab>
            COMMAND strip --strip-section-headers $<TARGET_FILE:rnab>
            COMMENT "Removing residual ELF sections"
        )
    endif()
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

message(STATUS "RNAB Build Configuration")
message(STATUS "Compiler:     ${CMAKE_C_COMPILER_ID} ${CMAKE_C_COMPILER_VERSION}")
message(STATUS "Platform:     ${CMAKE_SYSTEM_NAME} ${CMAKE_SYSTEM_PROCESSOR}")
message(STATUS "Library:      ${BUILD_LIBRARY}")
message(STATUS "Debug:        ${RNAB_DEBUG}")
message(STATUS "Benchmark:    ${RNAB_BENCHMARK}")
message(STATUS "Strip:        ${ENABLE_STRIP}")
message(STATUS "TABLE_SIZE:   ${TABLE_SIZE}  (2^${TABLE_BITS})")
