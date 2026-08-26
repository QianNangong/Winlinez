# CMake toolchain file for 32-bit Windows (i686) builds from a Unix host.
# Usage:
#   cmake -B build -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-i686.toolchain.cmake
#   cmake --build build

set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86)

set(CMAKE_C_COMPILER    i686-w64-mingw32-gcc)
set(CMAKE_RC_COMPILER   i686-w64-mingw32-windres)

# search target libraries/headers only
set(CMAKE_FIND_ROOT_PATH /usr/i686-w64-mingw32)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
