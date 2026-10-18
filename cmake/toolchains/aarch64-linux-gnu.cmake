# MIT License © 2026 Binary Dice Games
#
# Cross build for Linux arm64 from Linux x86-64; tests run under QEMU.
# docs/building.md, "Release executables".
#
#   cmake -S . -B build-arm64 -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/aarch64-linux-gnu.cmake

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)
set(CMAKE_C_COMPILER aarch64-linux-gnu-gcc)
set(CMAKE_CXX_COMPILER aarch64-linux-gnu-g++)
set(CMAKE_FIND_ROOT_PATH /usr/aarch64-linux-gnu)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
find_program(NYMPH_QEMU NAMES qemu-aarch64 qemu-aarch64-static)
if(NYMPH_QEMU)
  set(CMAKE_CROSSCOMPILING_EMULATOR ${NYMPH_QEMU} -L /usr/aarch64-linux-gnu)
endif()
