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
    mcp/resources.hpp
    mcp/resources.cpp
  )
  target_link_libraries(nymph_mcp PUBLIC nymph_cli nymph_json)

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
