# nymph — Building

nymph targets **Linux, MSYS2, and native Windows (MSVC)**.

---

## Prerequisites

| Tool | Minimum version | Notes |
|------|----------------|-------|
| CMake | 3.14 | |
| C++ compiler | C++20 | GCC 10+, Clang 12+, MSVC 2022 |
| Git | any | Required to check out submodules |

Dependencies live in `extern/` as git submodules and are compiled from
source; no system packages are needed. Configuring from a git checkout
initializes them, so `git submodule update --init --recursive` is
optional.

| Submodule | Version | Used for |
|-----------|---------|----------|
| `extern/imgui` | v1.92.9b | Frame generation and fonts (no backend: nymph rasterizes on the CPU) |
| `extern/implot` | v1.0 | 2D plots |
| `extern/implot3d` | v0.4 | 3D plots |
| `extern/libyaml` | 0.2.5 | Parsing the YAML source |
| `extern/stb` | `31c1ad3` | PNG encoding, zlib |
| `extern/googletest` | v1.15.2 | Tests only (`NYMPH_BUILD_TESTS`) |

To move a dependency to another version, check out the new commit inside
the submodule **and stage it** (`git add extern/<name>`): configuring runs
`git submodule update`, which resets every submodule to the staged
commit.

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
| `nymph_imgui`, `yaml` | Static libraries of the dependencies |
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
