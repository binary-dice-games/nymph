# nymph

nymph is a command-line tool that turns a YAML file describing a chart
into a PNG image, and a nymph PNG back into its YAML (the YAML is stored
inside the image). It draws 2D and 3D plots, labels and layouts with Dear
ImGui, ImPlot and ImPlot3D and rasterizes them on the CPU with its own
software rasterizer: no GPU, window or display needed.
It is meant for AI agents that need to show information as a chart.

It is a standalone version of wish's `nymph` module, with no wish or bison
dependency. **Status: in progress** (the `nymph` command and the library
render 2D and 3D plots, layouts and labels; wish image import and the
agent documentation are next) -- see
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

```sh
cat > chart.yaml <<'YAML'
root:
  type: Plot
  title: Revenue
  children:
    - {type: PlotLine, label: revenue, ys: $revenue}
data: {revenue: [12, 15, 11, 18]}
YAML
nymph chart.yaml        # writes chart.png (no display or GPU needed)
nymph chart.png         # prints the YAML back
```

See [docs/cli.md](docs/cli.md) for every command. As a library:

```cpp
#include <nymph.hpp>
#include <fstream>

int main() {
  const char* source =
      "root:\n"
      "  type: Plot\n"
      "  title: Revenue\n"
      "  children:\n"
      "    - {type: PlotLine, label: revenue, ys: $revenue}\n"
      "data: {revenue: [12, 15, 11, 18]}\n";
  const std::vector<uint8_t> png = bdg::nymph::render_png(source); // throws bdg::nymph::error
  std::ofstream("chart.png", std::ios::binary).write((const char*)png.data(), png.size());
}
```

Link against the `nymph` CMake target (`nymph::nymph` alias). The PNG
carries its source: `bdg::nymph::extract_source(png)` returns it.

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
| **Elements** | `Plot` / `Plot3D` and their series, plus `VerticalLayout` / `HorizontalLayout`, `Spring`, `Label` and separators to combine them, with the same fields as wish's widgets. New types register a class; see [DESIGN.md](DESIGN.md#9-extension-points). |
| **Column references** | `$name` / `$2` in a field takes data from a column. |
| **Round trip** | The PNG carries its YAML; `nymph image.png` prints it back byte for byte. |

## Further Documentation

| File | Contents |
|------|----------|
| [docs/building.md](docs/building.md) | Prerequisites, CMake options, build commands |
| [docs/cli.md](docs/cli.md) | The `nymph` command: render, extract, check, schema, exit codes, errors |
| [DESIGN.md](DESIGN.md) | Architecture, source format, key abstractions, design decisions |
| [PLAN.md](PLAN.md) | Ordered implementation steps and their tests |
| [CHANGELOG.md](CHANGELOG.md) | User-facing changes between releases |
| [CLAUDE.md](CLAUDE.md) | Coding style and project rules for AI agents and contributors |

## License

MIT License © 2026 Binary Dice Games. See [LICENSE](LICENSE) for the full text.
