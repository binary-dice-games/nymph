# Specifications

One folder per project, numbered in the order the projects were started:
`spec/<NN>_<name>/` holds that project's `DESIGN.md` and `PLAN.md` (and any
other notes it needs). Read this index first, then only the folder of the
project you are working on, or the folders whose topics match your task.

| Project | Status | Summary | Topics |
|---|---|---|---|
| [01_core_features](01_core_features/) | Done (MSYS2 / MSVC builds not yet verified) | The `nymph` tool and library: YAML chart source to PNG and back, on the CPU with ImGui, ImPlot and ImPlot3D and nymph's own software rasterizer. | Source format, named data tables, `$column` references, `source` key, element schema and registry, 2D / 3D plots, axis scales / ticks / second Y axis, error bars, bar groups, annotations, `ColorScale`, Gantt charts (`GanttChart`, ISO dates), layout engine, labels, embedded fonts and text style, software rasterizer, PNG metadata, CLI, agent documentation. Its DESIGN.md is the reference for the current architecture. |
| [02_opengl_rasterizer](02_opengl_rasterizer/) | Done (verified on Linux with Mesa llvmpipe and a MinGW build under Wine; hardware GPU, native MSVC and MSYS2 not yet) | Optional OpenGL rasterizer chosen with `--rasterizer software\|opengl` (default OpenGL, silent fallback to software); OpenGL loaded at run time, never linked. | Rasterizer backend interface, OpenGL 3.3 rasterizer, run-time loading (`dlopen` / `LoadLibraryW`), headless EGL and WGL contexts, fallback rules, `renderer` library API, `--rasterizer` and `nymph rasterizers`, `NYMPH_OPENGL` build option, linkage test. |
| [03_uml_diagrams](03_uml_diagrams/) | Done (Linux; MSYS2 / MSVC builds not yet verified) | UML diagrams (class, sequence, state, activity, use case) as nymph elements described in YAML, mermaid-like, drawn by ImUml, a new ImPlot-style ImGui library with Graphviz dot-style automatic layout. | ImUml library (`imuml/`), dot-style layered graph layout (network simplex, mincross), dot layout hints, sequence layout, diagram elements (`src/elements/uml/`), `text_array` field type, fit-to-rectangle zoom, ImUml context in `render_frames()`, `Flowchart` (`FlowNode`, `FlowGroup`, `FlowLink`). |
