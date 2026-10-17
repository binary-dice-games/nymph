# Specifications

One folder per project, numbered in the order the projects were started:
`spec/<NN>_<name>/` holds that project's `DESIGN.md` and `PLAN.md` (and any
other notes it needs). Read this index first, then only the folder of the
project you are working on, or the folders whose topics match your task.

| Project | Status | Summary | Topics |
|---|---|---|---|
| [01_core_features](01_core_features/) | Done (MSYS2 / MSVC builds not yet verified) | The `nymph` tool and library: YAML chart source to PNG and back, on the CPU with ImGui, ImPlot and ImPlot3D and nymph's own software rasterizer. | Source format, named data tables, `$column` references, `source` key, element schema and registry, 2D / 3D plots, layout engine, labels, embedded fonts and text style, software rasterizer, PNG metadata, CLI, agent documentation. Its DESIGN.md is the reference for the current architecture. |
