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
set(COLORLINEZ_TOOLCHAIN "mingw" CACHE STRING "mingw | llvm-mingw | watcom")
set_property(CACHE COLORLINEZ_TOOLCHAIN PROPERTY STRINGS mingw llvm-mingw watcom)

if(NOT WIN32 AND NOT DEFINED CMAKE_C_COMPILER
   AND NOT COLORLINEZ_TOOLCHAIN STREQUAL "watcom")
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

option(COLORLINEZ_UNICODE "Use Unicode (W) APIs for all text" ON)

# ---------------------------------------------------------------------------
# Open Watcom 2.0 branch: OW's drivers (wcl386/wrc/wlink) do not fit the
# CMake compiler model, so the build is driven as a custom recipe.
# Tools are located through PATH or the WATCOM environment variable.
# ---------------------------------------------------------------------------
if(COLORLINEZ_TOOLCHAIN STREQUAL "watcom")
    project(ColorLinez NONE)                 # no host compiler required

    if(CMAKE_HOST_WIN32)
        set(_ow_bindir binnt)                # Windows-hosted OW tools
        set(_list_sep "\\;")
    else()
        set(_ow_bindir binl)                 # Linux-hosted OW tools
        set(_list_sep ":")
    endif()

    if(DEFINED ENV{WATCOM})
        # prefer the Watcom-owned tools: generic names like "wrc" are
        # ambiguous on Unix systems
        find_program(WCL386_EXECUTABLE wcl386 HINTS "$ENV{WATCOM}/${_ow_bindir}")
        find_program(WRC_EXECUTABLE   wrc    HINTS "$ENV{WATCOM}/${_ow_bindir}")
        find_program(WLINK_EXECUTABLE wlink HINTS "$ENV{WATCOM}/${_ow_bindir}")
    else()
        find_program(WCL386_EXECUTABLE wcl386)
        find_program(WRC_EXECUTABLE   wrc)
        find_program(WLINK_EXECUTABLE wlink)
    endif()
    if(NOT WCL386_EXECUTABLE OR NOT WRC_EXECUTABLE OR NOT WLINK_EXECUTABLE)
        message(FATAL_ERROR
            "Open Watcom tools (wcl386/wrc/wlink) not found in PATH and the "
            "WATCOM environment variable is not set. Install Open Watcom 2.0, "
            "add its ${_ow_bindir} directory to PATH, or set WATCOM=<prefix>.")
    endif()
    get_filename_component(_ow_bindir_real "${WCL386_EXECUTABLE}" DIRECTORY)
    get_filename_component(COLORLINEZ_WATCOM_ROOT "${_ow_bindir_real}/.." ABSOLUTE)
    message(STATUS "Open Watcom: ${COLORLINEZ_WATCOM_ROOT}")

    if(COLORLINEZ_UNICODE)
        set(_ow_defs -dUNICODE -d_UNICODE)
    else()
        set(_ow_defs "")
    endif()

    set(_ow_env ${CMAKE_COMMAND} -E env
        "WATCOM=${COLORLINEZ_WATCOM_ROOT}"
        "INCLUDE=${COLORLINEZ_WATCOM_ROOT}/h/nt${_list_sep}${COLORLINEZ_WATCOM_ROOT}/h"
        "PATH=${COLORLINEZ_WATCOM_ROOT}/${_ow_bindir}${_list_sep}$ENV{PATH}")

    # outputs land next to the sources (src/ is covered by .gitignore)
    add_custom_command(
        OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/src/winlinez.exe
        COMMAND ${_ow_env} ${WCL386_EXECUTABLE} -q -c -bt=nt ${_ow_defs}
                winlinez.c inflate.c gfxdata.c
        COMMAND ${_ow_env} ${WRC_EXECUTABLE} -q -r -bt=nt -I. -I.. winlinez.rc
        COMMAND ${_ow_env} ${WCL386_EXECUTABLE} -q -bt=nt -l=nt_win
                winlinez.o inflate.o gfxdata.o winlinez.res
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/src
        DEPENDS src/winlinez.c src/inflate.c src/gfxdata.c src/winlinez.rc
                res/icon.ico res/about_king.bmp res/about_mask.bmp
        VERBATIM)
    add_custom_target(winlinez ALL
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/winlinez.exe)

    add_custom_command(
        OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/src/inflate_test.exe
        COMMAND ${_ow_env} ${WCL386_EXECUTABLE} -q -c -bt=nt ${_ow_defs}
                test_inflate.c inflate.c gfxdata.c
        COMMAND ${_ow_env} ${WCL386_EXECUTABLE} -q -bt=nt -l=nt
                -fe=inflate_test.exe test_inflate.o inflate.o gfxdata.o
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/src
        DEPENDS src/test_inflate.c src/inflate.c src/gfxdata.c
                ${CMAKE_CURRENT_SOURCE_DIR}/src/winlinez.exe
        VERBATIM)
    add_custom_target(inflate_test ALL
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/inflate_test.exe)

    enable_testing()
    find_program(WINE_EXECUTABLE wine)
    if(WINE_EXECUTABLE)
        add_test(NAME inflate_crc
                 COMMAND ${WINE_EXECUTABLE}
                 ${CMAKE_CURRENT_SOURCE_DIR}/src/inflate_test.exe)
    else()
        message(STATUS "wine not found: the OW inflate_crc test will not run")
    endif()
    return()
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

if(COLORLINEZ_UNICODE)
    target_compile_definitions(winlinez PRIVATE UNICODE _UNICODE)
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
