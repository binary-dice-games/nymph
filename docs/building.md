# nymph — Building

nymph targets **Linux, MSYS2, and native Windows (MSVC)**.

---

## Prerequisites

| Tool | Minimum version | Notes |
|------|----------------|-------|
| CMake | 3.14 | |
| C++ compiler | C++20 | GCC 10+, Clang 12+, MSVC 2022 |
| Git | any | Required to check out submodules |

Dependencies live in `extern/` as git submodules. Configuring from a git
checkout initializes the ones the chosen options need, so
`git submodule update --init --recursive` is optional.

### MSYS2 (Windows host)

From an **MSYS2 MSYS** shell:

```bash
pacman -S --needed mingw-w64-x86_64-cmake mingw-w64-x86_64-ninja mingw-w64-x86_64-gcc
```

Configuring and building from there is identical to Linux.

### Native Windows (MSVC)

Use a *Developer PowerShell for VS 2022* (or run `vcvars64.bat`) so `cl.exe`
is on `PATH`, then build with the Ninja or Visual Studio generator.

---

## Build

```sh
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure -j
```

Outputs:

| Target | Output |
|--------|--------|
| `nymph` | Static library |
| `nymph-cli` | `build/app/nymph` executable |
| `test_*` | GoogleTest executables under `build/tests/` |

---

## CMake options

| Option | Default | Description |
|--------|---------|-------------|
| `NYMPH_BUILD_TESTS` | `ON` when top-level | Build the GoogleTest suite |

---

## Using nymph from another CMake project

```cmake
add_subdirectory(extern/nymph)
target_link_libraries(my_app PRIVATE nymph::nymph)
```

Tests and the `nymph` executable are skipped when nymph is not the
top-level project.

---

## Formatting

Format C++ sources with `clang-format` using the repository's
`.clang-format` (`format-all.ps1` formats the whole tree on Windows).
