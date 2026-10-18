# MIT License © 2026 Binary Dice Games
#
# Cross build for Windows x86-64 with MinGW-w64 (POSIX threads) from Linux;
# tests run under Wine. docs/building.md, "Release executables".
#
#   cmake -S . -B build-win -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/mingw-w64-x86_64.cmake

set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)
set(CMAKE_C_COMPILER x86_64-w64-mingw32-gcc-posix)
set(CMAKE_CXX_COMPILER x86_64-w64-mingw32-g++-posix)
set(CMAKE_RC_COMPILER x86_64-w64-mingw32-windres)
set(CMAKE_FIND_ROOT_PATH /usr/x86_64-w64-mingw32)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
find_program(NYMPH_WINE NAMES wine64 wine PATHS /usr/lib/wine)
if(NYMPH_WINE)
  set(CMAKE_CROSSCOMPILING_EMULATOR env WINEDEBUG=-all ${NYMPH_WINE})
endif()
