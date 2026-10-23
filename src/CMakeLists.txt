configure_file(version.hpp.in "${CMAKE_CURRENT_BINARY_DIR}/generated/version.hpp" @ONLY)

add_library(nymph STATIC
  nymph.hpp
  nymph.cpp
  render/image.hpp
  render/texture_store.hpp
  render/texture_store.cpp
  render/soft_rasterizer.hpp
  render/soft_rasterizer.cpp
  render/raster_backend.hpp
  render/renderer_options.hpp
  render/image_options.hpp
  render/draw_context.hpp
  render/ui_state.hpp
  render/draw_context.cpp
  render/text_style.hpp
  render/text_style.cpp
  render/figure_renderer.hpp
  render/figure_renderer.cpp
  render/frame_setup.hpp
  render/frame_setup.cpp
  view/live_view.hpp
  view/live_view.cpp
  core/limits.hpp
  png/png_meta.hpp
  png/png_meta.cpp
  core/dates.hpp
  core/error.hpp
  core/suggest.hpp
  core/sha256.hpp
  core/sha256.cpp
  core/charconv.hpp
  core/charconv.cpp
  render/gl/shared_library.hpp
  render/gl/shared_library.cpp
  fonts/embedded_fonts.hpp
  fonts/fonts.cpp
  fonts/font_testing.hpp
  fonts/fonts_used.hpp
  fonts/fonts_used.cpp
  document/yaml_doc.hpp
  document/yaml_doc.cpp
  document/view.hpp
  document/view.cpp
  document/table.hpp
  document/table.cpp
  document/bind.hpp
  document/bind.cpp
  edit/source_edit.hpp
  edit/source_edit.cpp
  edit/yaml_text.hpp
  edit/tree_edit.hpp
  edit/tree_edit.cpp
  edit/element_starter.hpp
  edit/element_starter.cpp
  edit/tree_menu.hpp
  edit/tree_menu.cpp
  edit/data_edit.hpp
  edit/data_edit.cpp
  edit/csv_file.hpp
  edit/csv_file.cpp
  edit/table_grid.hpp
  edit/table_grid.cpp
  edit/field_value.hpp
  edit/field_value.cpp
  edit/editor_model.hpp
  edit/editor_model.cpp
  edit/element_icons.hpp
  edit/element_icons.cpp
  edit/image_fields.hpp
  edit/image_fields.cpp
  edit/inspector.hpp
  edit/inspector.cpp
  edit/number_box.hpp
  edit/number_box.cpp
  edit/edit_toolbar.hpp
  edit/edit_toolbar.cpp
  edit/workspace_layout.hpp
  mermaid/mermaid_internal.hpp
  mermaid/mermaid.cpp
  mermaid/reader.cpp
  mermaid/yaml_writer.cpp
  mermaid/pie.cpp
  mermaid/flowchart.cpp
  mermaid/sequence.cpp
  mermaid/class.cpp
  mermaid/state.cpp
  mermaid/er.cpp
  mermaid/gantt.cpp
  mermaid/mindmap.cpp
  mermaid/charts.cpp
  mermaid/style_sheet.cpp
  elements/plot_style.hpp
  elements/plot_style.cpp
  elements/plot2d.hpp
  elements/plot2d.cpp
  elements/plot_pick.hpp
  elements/plot_pick.cpp
  elements/plot3d.hpp
  elements/plot3d.cpp
  elements/gantt.hpp
  elements/gantt.cpp
  elements/sankey.hpp
  elements/sankey.cpp
  elements/layout_elements.hpp
  elements/layout_elements.cpp
  elements/shape_element.hpp
  elements/shape_element.cpp
  elements/ui/ui_common.hpp
  elements/ui/ui_common.cpp
  elements/ui/screen.cpp
  elements/ui/flow.cpp
  elements/ui/ui_flags.hpp
  elements/ui/ui_flags.cpp
  elements/ui/value_widgets.cpp
  elements/ui/image_widgets.cpp
  elements/ui/containers.cpp
  elements/ui/tables.cpp
  elements/ui/menus.cpp
  placeholders/placeholders.hpp
  placeholders/placeholders.cpp
  placeholders/svg_shapes.hpp
  placeholders/svg_shapes.cpp
  shapes/shape_catalog.hpp
  shapes/shape_catalog.cpp
  elements/ui/widgets.cpp
  elements/ui/register_ui.cpp
  elements/uml/uml.hpp
  elements/uml/uml_common.hpp
  elements/uml/uml_common.cpp
  elements/uml/class_diagram.cpp
  elements/uml/sequence_diagram.cpp
  elements/uml/state_diagram.cpp
  elements/uml/activity_diagram.cpp
  elements/uml/usecase_diagram.cpp
  elements/uml/flowchart.cpp
  elements/uml/er_diagram.cpp
  elements/uml/dataflow_diagram.cpp
  elements/uml/network_diagram.cpp
  elements/uml/component_diagram.cpp
  elements/uml/deployment_diagram.cpp
  elements/uml/package_diagram.cpp
  elements/uml/object_diagram.cpp
  elements/uml/tree_diagrams.cpp
  layout/layout.hpp
  layout/layout.cpp
  layout/sankey_layout.hpp
  layout/sankey_layout.cpp
  elements/register_builtin.hpp
  elements/register_builtin.cpp
  schema/value.hpp
  schema/name_table.hpp
  schema/name_table.cpp
  schema/field_desc.hpp
  schema/node.hpp
  schema/element_class.hpp
  schema/registry.hpp
  schema/schema.cpp
  schema/convert.hpp
  schema/convert.cpp
)
add_library(nymph::nymph ALIAS nymph)
include("${PROJECT_SOURCE_DIR}/cmake/NymphFonts.cmake")
nymph_embed_fonts(nymph)
include("${PROJECT_SOURCE_DIR}/cmake/NymphPlaceholders.cmake")
nymph_embed_placeholders(nymph)
include("${PROJECT_SOURCE_DIR}/cmake/NymphShapes.cmake")
nymph_embed_shapes(nymph)
target_include_directories(nymph PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_BINARY_DIR}/generated
)
target_link_libraries(nymph PUBLIC nymph_imgui imuml PRIVATE yaml nymph_stb)
# The run-time loader (shared_library): OpenGL, and the window of `nymph view`.
if(NOT WIN32)
  target_link_libraries(nymph PRIVATE ${CMAKE_DL_LIBS})
endif()

# std::from_chars for double: missing from Apple's libc++, where
# core/charconv.cpp uses its own parser (spec/07_release_pipeline/DESIGN.md
# section 4.8).
include(CheckCXXSourceCompiles)
set(CMAKE_REQUIRED_FLAGS "${CMAKE_CXX_FLAGS}")
check_cxx_source_compiles("
#include <charconv>
int main() {
  const char text[] = \"1.5\";
  double value = 0.0;
  return std::from_chars(text, text + 3, value).ec == std::errc{} ? 0 : 1;
}" NYMPH_HAS_FLOAT_FROM_CHARS)
unset(CMAKE_REQUIRED_FLAGS)
if(NYMPH_HAS_FLOAT_FROM_CHARS)
  set_property(SOURCE core/charconv.cpp APPEND PROPERTY COMPILE_DEFINITIONS NYMPH_HAS_FLOAT_FROM_CHARS=1)
endif()

# ── nymph_cli — the command line over the library ───────────────────────────

# Kept out of `nymph` so library users (and the web viewer) do not link the
# command line or std::filesystem.
add_library(nymph_cli STATIC
  cli/cli.hpp
  cli/cli.cpp
  cli/cli_support.hpp
  cli/cli_support.cpp
  cli/schema_doc.hpp
  cli/schema_doc.cpp
)
target_link_libraries(nymph_cli PUBLIC nymph)

# The sources of examples/, compiled in for `nymph example` and the MCP
# tool example_chart.
set(_cli_out "${CMAKE_CURRENT_BINARY_DIR}/generated/cli")
file(MAKE_DIRECTORY "${_cli_out}")
file(GLOB _example_sources CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/examples/*.yaml")
add_custom_command(
  OUTPUT "${_cli_out}/cli_examples.cpp"
  COMMAND "${CMAKE_COMMAND}" "-DEXAMPLES=${PROJECT_SOURCE_DIR}/examples" "-DOUTPUT=${_cli_out}/cli_examples.txt"
          -P "${PROJECT_SOURCE_DIR}/cmake/NymphExampleBundle.cmake"
  COMMAND "${CMAKE_COMMAND}" "-DINPUT=${_cli_out}/cli_examples.txt" "-DOUTPUT=${_cli_out}/cli_examples.cpp"
          "-DSYMBOL=cli_examples" -P "${PROJECT_SOURCE_DIR}/cmake/NymphEmbedFile.cmake"
  DEPENDS ${_example_sources} "${PROJECT_SOURCE_DIR}/cmake/NymphExampleBundle.cmake"
          "${PROJECT_SOURCE_DIR}/cmake/NymphEmbedFile.cmake"
  COMMENT "Embedding examples/"
  VERBATIM)
target_sources(nymph_cli PRIVATE "${_cli_out}/cli_examples.cpp")

# ── nymph_mcp — the MCP server over the library and the CLI helpers ─────────

# spec/06_claude_integration/DESIGN.md section 4.1. Above nymph_cli, whose
# helpers its tools share, so the executable dispatches `nymph mcp`.
if(NYMPH_MCP)
  add_library(nymph_mcp STATIC
    mcp/json_rpc.hpp
    mcp/json_rpc.cpp
    mcp/mcp_session.hpp
    mcp/mcp_session.cpp
    mcp/mcp_server.hpp
    mcp/mcp_server.cpp
    mcp/server_options.hpp
    mcp/base64.hpp
    mcp/base64.cpp
    mcp/tools.hpp
    mcp/tools.cpp
    mcp/resources.hpp
    mcp/resources.cpp
    # The running editor's MCP server and how nymph mcp reaches it
    # (spec/14_editor_workspace/DESIGN.md section 3).
    ipc/local_socket.hpp
    ipc/local_socket.cpp
    ipc/editor_registry.hpp
    ipc/editor_registry.cpp
    mcp/editor_tools.hpp
    mcp/editor_tools.cpp
    mcp/editor_server.hpp
    mcp/editor_server.cpp
    mcp/editor_bridge.hpp
    mcp/editor_bridge.cpp
  )
  target_link_libraries(nymph_mcp PUBLIC nymph_cli nymph_json)
  if(WIN32)
    target_link_libraries(nymph_mcp PRIVATE ws2_32) # sockets: a system library
  endif()

  # The agent text and the guides the server hands to clients
  # (spec/06_claude_integration/DESIGN.md sections 4.5 and 5.2).
  set(_mcp_skill "${PROJECT_SOURCE_DIR}/claude/skill/nymph/SKILL.md")
  set(_mcp_agent_guide "${PROJECT_SOURCE_DIR}/docs/agent-guide.md")
  set(_mcp_elements "${PROJECT_SOURCE_DIR}/docs/elements.md")
  set(_mcp_out "${CMAKE_CURRENT_BINARY_DIR}/generated/mcp")
  file(MAKE_DIRECTORY "${_mcp_out}")
  foreach(_doc skill agent_guide elements)
    set(_file "${_mcp_${_doc}}")
    add_custom_command(
      OUTPUT "${_mcp_out}/mcp_${_doc}.cpp"
      COMMAND "${CMAKE_COMMAND}" "-DINPUT=${_file}" "-DOUTPUT=${_mcp_out}/mcp_${_doc}.cpp" "-DSYMBOL=mcp_${_doc}"
              -P "${PROJECT_SOURCE_DIR}/cmake/NymphEmbedFile.cmake"
      DEPENDS "${_file}" "${PROJECT_SOURCE_DIR}/cmake/NymphEmbedFile.cmake"
      COMMENT "Embedding ${_file}"
      VERBATIM)
    target_sources(nymph_mcp PRIVATE "${_mcp_out}/mcp_${_doc}.cpp")
  endforeach()

  # The MCP App page, built by the NYMPH_WEB build (web/CMakeLists.txt,
  # nymph-claude-app); without it the server offers only PNGs.
  set(NYMPH_MCP_VIEWER "" CACHE FILEPATH
    "The MCP App page to embed (<build-web>/claude/viewer.html); empty for none")
  if(NYMPH_MCP_VIEWER)
    if(NOT EXISTS "${NYMPH_MCP_VIEWER}")
      message(FATAL_ERROR "nymph: NYMPH_MCP_VIEWER names '${NYMPH_MCP_VIEWER}', which does not exist (build the web viewer first)")
    endif()
    add_custom_command(
      OUTPUT "${_mcp_out}/mcp_viewer.cpp"
      COMMAND "${CMAKE_COMMAND}" "-DINPUT=${NYMPH_MCP_VIEWER}" "-DOUTPUT=${_mcp_out}/mcp_viewer.cpp" "-DSYMBOL=mcp_viewer"
              -P "${PROJECT_SOURCE_DIR}/cmake/NymphEmbedFile.cmake"
      DEPENDS "${NYMPH_MCP_VIEWER}" "${PROJECT_SOURCE_DIR}/cmake/NymphEmbedFile.cmake"
      COMMENT "Embedding the MCP App ${NYMPH_MCP_VIEWER}"
      VERBATIM)
    target_sources(nymph_mcp PRIVATE "${_mcp_out}/mcp_viewer.cpp")
    target_compile_definitions(nymph_mcp PRIVATE NYMPH_MCP_HAS_VIEWER=1)
  endif()
endif()

# ── nymph_window — `nymph view`, the viewer in a native window ──────────────

# spec/04_web_viewer/DESIGN.md section 10. A window and an OpenGL 3.3
# context of the system's own libraries (Win32 and opengl32.dll, or X11 and
# libGL.so.1), every function looked up at run time (nothing is linked); Dear ImGui's OpenGL3
# backend (which loads OpenGL itself, at run time) draws the live_view into
# it, as the web viewer does with WebGL.
if(NYMPH_VIEW)
  add_library(nymph_window STATIC
    window/view_command.hpp
    window/view_command.cpp
    window/viewer_window.hpp
    window/viewer_window.cpp
  window/editor_panel.hpp
  window/editor_panel.cpp
    window/window_host.hpp
    window/window_icon.hpp
    window/window_icon.cpp
    "${NYMPH_EXTERN_DIR}/imgui/backends/imgui_impl_opengl3.cpp"
  )
  # The window's icon: resources/icon.png, compiled in (window_icon.cpp).
  set(_nymph_icon_png "${PROJECT_SOURCE_DIR}/resources/icon.png")
  set(_nymph_icon_cpp "${CMAKE_CURRENT_BINARY_DIR}/generated/window_icon_png.cpp")
  add_custom_command(
    OUTPUT "${_nymph_icon_cpp}"
    COMMAND "${CMAKE_COMMAND}" "-DINPUT=${_nymph_icon_png}" "-DOUTPUT=${_nymph_icon_cpp}"
            "-DSYMBOL=window_icon_png" -P "${PROJECT_SOURCE_DIR}/cmake/NymphEmbedFile.cmake"
    DEPENDS "${_nymph_icon_png}" "${PROJECT_SOURCE_DIR}/cmake/NymphEmbedFile.cmake"
    COMMENT "Embedding the window icon"
    VERBATIM)
  target_sources(nymph_window PRIVATE "${_nymph_icon_cpp}")
  # One window implementation per platform (window_host.hpp); no header or
  # library of either is needed.
  if(WIN32)
    target_sources(nymph_window PRIVATE window/window_win32.cpp)
  else()
    target_sources(nymph_window PRIVATE window/window_x11.cpp)
    target_link_libraries(nymph_window PRIVATE ${CMAKE_DL_LIBS})
  endif()
  target_include_directories(nymph_window PRIVATE "${NYMPH_EXTERN_DIR}/imgui/backends")
  target_link_libraries(nymph_window PUBLIC nymph_cli)
  # The editor as an MCP server for agents (spec/14_editor_workspace).
  if(NYMPH_MCP)
    target_link_libraries(nymph_window PUBLIC nymph_mcp)
    target_compile_definitions(nymph_window PRIVATE NYMPH_EDITOR_MCP=1)
  endif()
endif()

# ── OpenGL rasterizer (optional; OpenGL itself is loaded at run time) ────────

if(NYMPH_OPENGL)
  target_sources(nymph PRIVATE
    render/gl/shared_library.hpp
    render/gl/shared_library.cpp
    render/gl/gl_api.hpp
    render/gl/gl_api.cpp
    render/gl/gl_device.hpp
    render/gl/gl_device.cpp
    render/gl_rasterizer.hpp
    render/gl_rasterizer.cpp
  )
  # EGL and WGL share nothing but the interface: one file each.
  if(WIN32)
    target_sources(nymph PRIVATE render/gl/gl_device_win.cpp)
  else()
    target_sources(nymph PRIVATE render/gl/gl_device_posix.cpp)
  endif()
  target_compile_definitions(nymph PRIVATE NYMPH_HAS_OPENGL=1)
  if(WIN32)
    # System libraries for the hidden window and pixel format; not OpenGL.
    target_link_libraries(nymph PRIVATE user32 gdi32)
  endif()
endif()
