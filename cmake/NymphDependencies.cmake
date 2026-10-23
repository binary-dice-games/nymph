# Third-party dependencies, built from the git submodules in extern/.
#
# Defines:
#   nymph_imgui  -- static: Dear ImGui core + ImPlot + ImPlot3D, no backends
#                   (nymph is its own platform and renderer backend).
#   yaml         -- static: libyaml, built from its sources here rather than
#                   through its own CMakeLists.txt (which requires CMake 3.0,
#                   rejected by CMake 4, pulls in CTest and adds install and
#                   export rules).
#   nymph_stb    -- interface: the stb include directory. The single
#                   implementation of each stb header lives in nymph's own
#                   sources.
#   nymph_json   -- interface: nlohmann/json (header-only), with NYMPH_MCP.

nymph_ensure_submodules(extern/imgui extern/implot extern/implot3d extern/libyaml extern/stb)

set(NYMPH_EXTERN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/extern")

# ── nymph_imgui ───────────────────────────────────────────────────────────────

add_library(nymph_imgui STATIC
  ${NYMPH_EXTERN_DIR}/imgui/imgui.cpp
  ${NYMPH_EXTERN_DIR}/imgui/imgui_draw.cpp
  ${NYMPH_EXTERN_DIR}/imgui/imgui_tables.cpp
  ${NYMPH_EXTERN_DIR}/imgui/imgui_widgets.cpp
  ${NYMPH_EXTERN_DIR}/implot/implot.cpp
  ${NYMPH_EXTERN_DIR}/implot/implot_items.cpp
  ${NYMPH_EXTERN_DIR}/implot3d/implot3d.cpp
  ${NYMPH_EXTERN_DIR}/implot3d/implot3d_items.cpp
  ${NYMPH_EXTERN_DIR}/implot3d/implot3d_meshes.cpp
)
target_include_directories(nymph_imgui PUBLIC
  ${NYMPH_EXTERN_DIR}/imgui
  ${NYMPH_EXTERN_DIR}/implot
  ${NYMPH_EXTERN_DIR}/implot3d
)
# No OS integration: no clipboard/IME (Win32), no "open URL" shell call.
# Keeps the executable free of user32/imm32/shell32 on Windows.
# IMGUI_DISABLE_OBSOLETE_FUNCTIONS is not set: ImPlot (v1.0) still calls
# ImDrawList overloads that define deletes.
target_compile_definitions(nymph_imgui PUBLIC
  IMGUI_DISABLE_WIN32_FUNCTIONS
  IMGUI_DISABLE_DEFAULT_SHELL_FUNCTIONS
)
set_target_properties(nymph_imgui PROPERTIES FOLDER extern)
if(NYMPH_WEB)
  # WebGL2 has no glDrawElementsBaseVertex, so ImGui's OpenGL3 backend
  # cannot honour vertex offsets there; 32-bit indices keep plots of more
  # than 65,535 vertices drawable (spec/04_web_viewer/DESIGN.md section 6).
  target_compile_definitions(nymph_imgui PUBLIC "ImDrawIdx=unsigned int")
endif()

# ── yaml (libyaml) ────────────────────────────────────────────────────────────

add_library(yaml STATIC
  ${NYMPH_EXTERN_DIR}/libyaml/src/api.c
  ${NYMPH_EXTERN_DIR}/libyaml/src/dumper.c
  ${NYMPH_EXTERN_DIR}/libyaml/src/emitter.c
  ${NYMPH_EXTERN_DIR}/libyaml/src/loader.c
  ${NYMPH_EXTERN_DIR}/libyaml/src/parser.c
  ${NYMPH_EXTERN_DIR}/libyaml/src/reader.c
  ${NYMPH_EXTERN_DIR}/libyaml/src/scanner.c
  ${NYMPH_EXTERN_DIR}/libyaml/src/writer.c
)
target_include_directories(yaml PUBLIC ${NYMPH_EXTERN_DIR}/libyaml/include)
# The version macros libyaml's generated config.h would carry (0.2.5, the
# pinned tag in .gitmodules).
target_compile_definitions(yaml
  PRIVATE
    YAML_VERSION_MAJOR=0
    YAML_VERSION_MINOR=2
    YAML_VERSION_PATCH=5
    YAML_VERSION_STRING="0.2.5"
    $<$<C_COMPILER_ID:MSVC>:_CRT_SECURE_NO_WARNINGS>
  PUBLIC
    YAML_DECLARE_STATIC
)
set_target_properties(yaml PROPERTIES FOLDER extern)

# ── nymph_stb ─────────────────────────────────────────────────────────────────

add_library(nymph_stb INTERFACE)
target_include_directories(nymph_stb INTERFACE ${NYMPH_EXTERN_DIR}/stb)

# ── nymph_json (nlohmann/json) ────────────────────────────────────────────────

if(NYMPH_MCP OR NYMPH_VIEW OR NYMPH_WEB)
  nymph_ensure_submodules(extern/json)
  # The headers only: the project's own CMakeLists.txt adds install rules.
  add_library(nymph_json INTERFACE)
  target_include_directories(nymph_json INTERFACE ${NYMPH_EXTERN_DIR}/json/include)
endif()
