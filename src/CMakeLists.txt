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
)
add_library(nymph::nymph ALIAS nymph)
target_include_directories(nymph PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_BINARY_DIR}/generated
)
target_link_libraries(nymph PUBLIC nymph_imgui PRIVATE yaml nymph_stb)
