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
  core/limits.hpp
  png/png_meta.hpp
  png/png_meta.cpp
  core/error.hpp
  core/suggest.hpp
  fonts/embedded_fonts.hpp
  fonts/fonts.cpp
  document/yaml_doc.hpp
  document/yaml_doc.cpp
  document/table.hpp
  document/table.cpp
  document/bind.hpp
  document/bind.cpp
  elements/plot_style.hpp
  elements/plot_style.cpp
  elements/plot2d.hpp
  elements/plot2d.cpp
  elements/plot3d.hpp
  elements/plot3d.cpp
  elements/layout_elements.hpp
  elements/layout_elements.cpp
  layout/layout.hpp
  layout/layout.cpp
  cli/cli.hpp
  cli/cli.cpp
  cli/schema_doc.hpp
  cli/schema_doc.cpp
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
target_link_libraries(nymph PUBLIC nymph_imgui PRIVATE yaml nymph_stb)

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
