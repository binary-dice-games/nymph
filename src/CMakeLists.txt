configure_file(version.hpp.in "${CMAKE_CURRENT_BINARY_DIR}/generated/version.hpp" @ONLY)

add_library(nymph STATIC
  nymph.hpp
  nymph.cpp
  render/image.hpp
  render/texture_store.hpp
  render/texture_store.cpp
  render/soft_rasterizer.hpp
  render/soft_rasterizer.cpp
  render/image_options.hpp
  render/draw_context.hpp
  render/draw_context.cpp
  render/figure_renderer.hpp
  render/figure_renderer.cpp
  core/limits.hpp
  png/png_meta.hpp
  png/png_meta.cpp
  core/error.hpp
  document/yaml_doc.hpp
  document/yaml_doc.cpp
  document/table.hpp
  document/table.cpp
  document/bind.hpp
  document/bind.cpp
  document/legacy.hpp
  document/legacy.cpp
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
target_include_directories(nymph PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_BINARY_DIR}/generated
)
target_link_libraries(nymph PUBLIC nymph_imgui PRIVATE yaml nymph_stb)
