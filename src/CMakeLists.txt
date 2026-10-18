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
target_include_directories(nymph PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_BINARY_DIR}/generated
)
target_link_libraries(nymph PUBLIC nymph_imgui imuml PRIVATE yaml nymph_stb)

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
  )
  target_link_libraries(nymph_mcp PUBLIC nymph_cli nymph_json)
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
  else()
    target_link_libraries(nymph PRIVATE ${CMAKE_DL_LIBS})
  endif()
endif()
