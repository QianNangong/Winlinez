cmake_minimum_required(VERSION 3.15)

# ---------------------------------------------------------------------------
# Unix convenience: pick a Windows cross compiler so a plain
#   cmake -B build
# works out of the box.  Select the target with
#   cmake -B build -DCOLORLINEZ_ARCH=x86_64 -DCOLORLINEZ_TOOLCHAIN=llvm-mingw
# ARCH    : i686 (default) | x86_64 | aarch64
# TOOLCHAIN: mingw (default, system gcc) | llvm-mingw
# aarch64 is build-only (no wine test environment for it here).
# ---------------------------------------------------------------------------
set(COLORLINEZ_ARCH "i686" CACHE STRING "Target architecture: i686 | x86_64 | aarch64")
set_property(CACHE COLORLINEZ_ARCH PROPERTY STRINGS i686 x86_64 aarch64)
set(COLORLINEZ_TOOLCHAIN "mingw" CACHE STRING "mingw | llvm-mingw")
set_property(CACHE COLORLINEZ_TOOLCHAIN PROPERTY STRINGS mingw llvm-mingw)

if(NOT WIN32 AND NOT DEFINED CMAKE_C_COMPILER)
    if(COLORLINEZ_TOOLCHAIN STREQUAL "llvm-mingw")
        set(_cc ${COLORLINEZ_ARCH}-w64-mingw32-clang)
        set(_rc ${COLORLINEZ_ARCH}-w64-mingw32-windres)
    else()
        set(_cc ${COLORLINEZ_ARCH}-w64-mingw32-gcc)
        set(_rc ${COLORLINEZ_ARCH}-w64-mingw32-windres)
    endif()
    find_program(_cc_path ${_cc})
    if(NOT _cc_path)
        message(FATAL_ERROR
            "'${_cc}' not found in PATH. Install ${COLORLINEZ_TOOLCHAIN} for "
            "${COLORLINEZ_ARCH}, add it to PATH, or configure with "
            "-DCMAKE_C_COMPILER=<path> -DCMAKE_RC_COMPILER=<windres path>.")
    endif()
    set(CMAKE_SYSTEM_NAME Windows)
    set(CMAKE_C_COMPILER ${_cc_path})
    set(CMAKE_RC_COMPILER ${_rc})
    message(STATUS "Cross-building Win32 ${COLORLINEZ_ARCH} (${COLORLINEZ_TOOLCHAIN}) with ${CMAKE_C_COMPILER}")
endif()

project(ColorLinez C)
enable_language(RC)

# ---------------------------------------------------------------------------
# The game
# ---------------------------------------------------------------------------
add_executable(winlinez
    src/winlinez.c
    src/inflate.c
    src/gfxdata.c
    src/winlinez.rc
)
target_include_directories(winlinez PRIVATE src ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(winlinez PRIVATE gdi32 user32)
# CMake does not track files referenced from .rc scripts - declare them so
# editing a bitmap triggers recompilation of the resource object.
set_source_files_properties(src/winlinez.rc PROPERTIES DEPENDS
    "res/about_king.bmp;res/about_mask.bmp;res/icon.ico")

if(MINGW)
    target_link_options(winlinez PRIVATE -mwindows)
elseif(MSVC)
    target_link_options(winlinez PRIVATE "/SUBSYSTEM:WINDOWS")
endif()

# ---------------------------------------------------------------------------
# Regression test: the embedded sprite blob must inflate to the original
# 300,598-byte BMP (CRC-32 cd043a57).  On Unix hosts the test binary is
# a Windows EXE as well - run through wine if available.
# ---------------------------------------------------------------------------
add_executable(inflate_test
    src/test_inflate.c
    src/inflate.c
    src/gfxdata.c
)
target_include_directories(inflate_test PRIVATE src)

enable_testing()
find_program(WINE_EXECUTABLE wine)
if(COLORLINEZ_ARCH STREQUAL "aarch64")
    message(STATUS "aarch64 target: build-only (no wine test environment for it here)")
elseif(WINE_EXECUTABLE AND CMAKE_CROSSCOMPILING)
    add_test(NAME inflate_crc COMMAND ${WINE_EXECUTABLE} $<TARGET_FILE:inflate_test>)
else()
    add_test(NAME inflate_crc COMMAND inflate_test)
endif()
