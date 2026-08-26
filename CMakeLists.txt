cmake_minimum_required(VERSION 3.15)

# ---------------------------------------------------------------------------
# Unix convenience: auto-detect an i686 MinGW cross compiler so a plain
#   cmake -B build
# works out of the box.  Otherwise pass a toolchain file:
#   cmake -B build -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-i686.toolchain.cmake
# ---------------------------------------------------------------------------
if(NOT WIN32 AND NOT DEFINED CMAKE_C_COMPILER)
    find_program(MINGW_I686_GCC i686-w64-mingw32-gcc)
    if(MINGW_I686_GCC)
        set(CMAKE_SYSTEM_NAME Windows)
        set(CMAKE_C_COMPILER     i686-w64-mingw32-gcc)
        set(CMAKE_RC_COMPILER    i686-w64-mingw32-windres)
        message(STATUS "Cross-building for Win32 with ${MINGW_I686_GCC}")
    else()
        message(FATAL_ERROR
            "No Windows compiler found. Install mingw-w64 (i686 target), e.g.:\n"
            "  Debian/Ubuntu: apt install gcc-mingw-w64-i686\n"
            "  or configure with: -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-i686.toolchain.cmake")
    endif()
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
if(WINE_EXECUTABLE AND CMAKE_CROSSCOMPILING)
    add_test(NAME inflate_crc COMMAND ${WINE_EXECUTABLE} $<TARGET_FILE:inflate_test>)
else()
    add_test(NAME inflate_crc COMMAND inflate_test)
endif()
