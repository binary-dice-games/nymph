# nymph

nymph is a command-line tool that turns a YAML file describing a chart
into a PNG image, and a nymph PNG back into its YAML (the YAML is stored
inside the image). It draws 2D and 3D plots with Dear ImGui, ImPlot and
ImPlot3D and rasterizes them on the CPU: no GPU, window or display needed.
It is meant for AI agents that need to show information as a chart.

It is a standalone version of wish's `nymph` module, with no wish or bison
dependency. **Status: designed, not implemented** -- see
[DESIGN.md](DESIGN.md) and [PLAN.md](PLAN.md).

## Quick Build

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build
```

See [docs/building.md](docs/building.md) for prerequisites, CMake options,
and platform notes.

## Quick Start

```cpp
#include <nymph.hpp>
#include <iostream>

int main() {
  std::cout << "nymph " << bdg::nymph::version() << "\n";
}
```

Link against the `nymph` CMake target (`nymph::nymph` alias).

## Project Layout

| Path | Contents |
|------|----------|
| `src/` | The `nymph` library (namespace `bdg::nymph`) |
| `app/` | The `nymph` command-line executable |
| `tests/` | GoogleTest suite, run with `ctest` |
| `cmake/` | CMake helper modules |
| `docs/` | Detailed documentation |
| `extern/` | Third-party dependencies (git submodules) |

## Core Concepts

| Concept | Description |
|---------|-------------|
| **Source** | One YAML document: `description`, `image` options, a `root` element tree, and `data` (CSV text or columns). |
| **Elements** | `Plot` / `Plot3D` and their series, with the same fields as wish's plot widgets. New types register a class; see [DESIGN.md](DESIGN.md#9-extension-points). |
| **Column references** | `$name` / `$2` in a field takes data from a column. |
| **Round trip** | The PNG carries its YAML; `nymph image.png` prints it back byte for byte. |

## Further Documentation

| File | Contents |
|------|----------|
| [docs/building.md](docs/building.md) | Prerequisites, CMake options, build commands |
| [DESIGN.md](DESIGN.md) | Architecture, source format, key abstractions, design decisions |
| [PLAN.md](PLAN.md) | Ordered implementation steps and their tests |
| [CHANGELOG.md](CHANGELOG.md) | User-facing changes between releases |
| [CLAUDE.md](CLAUDE.md) | Coding style and project rules for AI agents and contributors |

## License

MIT License © 2026 Binary Dice Games. See [LICENSE](LICENSE) for the full text.
