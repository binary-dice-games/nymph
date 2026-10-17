# nymph

nymph is a Binary Dice Games C++ project. It is at the scaffolding stage:
the build, test and documentation layout is in place, and the
implementation is still to come.

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

To be written as the implementation lands.

## Further Documentation

| File | Contents |
|------|----------|
| [docs/building.md](docs/building.md) | Prerequisites, CMake options, build commands |
| [CHANGELOG.md](CHANGELOG.md) | User-facing changes between releases |
| [CLAUDE.md](CLAUDE.md) | Coding style and project rules for AI agents and contributors |

## License

MIT License © 2026 Binary Dice Games. See [LICENSE](LICENSE) for the full text.
