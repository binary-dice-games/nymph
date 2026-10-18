# MIT License © 2026 Binary Dice Games
#
# The web viewer (spec/04_web_viewer/DESIGN.md section 4, "Web module"):
# the nymph library compiled to WebAssembly, with its JavaScript API.
# Output, in <build>/web/:
#   nymph-viewer.js         the public ES module (web/js/nymph-viewer.js)
#   nymph-viewer-core.js    Emscripten's loader, imported by it
#   nymph-viewer-core.wasm  the module
#   fonts/                  the fonts not embedded, fetched on demand

set(NYMPH_WEB_OUT "${PROJECT_BINARY_DIR}/web")

add_executable(nymph-viewer
  nymph_web.cpp
  webgl_presenter.hpp
  webgl_presenter.cpp
  # Dear ImGui's OpenGL3 backend, as WebGL2 (OpenGL ES 3.0).
  "${NYMPH_EXTERN_DIR}/imgui/backends/imgui_impl_opengl3.cpp"
)
target_include_directories(nymph-viewer PRIVATE "${NYMPH_EXTERN_DIR}/imgui/backends" "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_definitions(nymph-viewer PRIVATE IMGUI_IMPL_OPENGL_ES3)
target_link_libraries(nymph-viewer PRIVATE nymph)
set_target_properties(nymph-viewer PROPERTIES
  OUTPUT_NAME nymph-viewer-core
  SUFFIX ".js"
  RUNTIME_OUTPUT_DIRECTORY "${NYMPH_WEB_OUT}"
)
target_link_options(nymph-viewer PRIVATE
  -sMODULARIZE=1
  -sEXPORT_ES6=1
  -sEXPORT_NAME=createNymphCore
  -sENVIRONMENT=web,worker,node
  -sALLOW_MEMORY_GROWTH=1
  -sFILESYSTEM=0
  -sSTACK_SIZE=1MB
  -sMIN_WEBGL_VERSION=2
  -sMAX_WEBGL_VERSION=2
  "-sEXPORTED_FUNCTIONS=['_malloc','_free']"
  "-sEXPORTED_RUNTIME_METHODS=['UTF8ToString','HEAPU8','specialHTMLTargets']"
)

# The JavaScript API, next to the module.
add_custom_command(
  OUTPUT "${NYMPH_WEB_OUT}/nymph-viewer.js"
  COMMAND "${CMAKE_COMMAND}" -E copy "${CMAKE_CURRENT_SOURCE_DIR}/js/nymph-viewer.js" "${NYMPH_WEB_OUT}/nymph-viewer.js"
  DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/js/nymph-viewer.js"
  COMMENT "Copying nymph-viewer.js"
  VERBATIM)
add_custom_target(nymph-viewer-js ALL DEPENDS "${NYMPH_WEB_OUT}/nymph-viewer.js")
add_dependencies(nymph-viewer nymph-viewer-js)

# The demo page, with every example (source and PNG) and their list.
file(GLOB nymph_example_sources CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/examples/*.yaml")
list(SORT nymph_example_sources)
set(nymph_example_list "")
foreach(source IN LISTS nymph_example_sources)
  get_filename_component(name "${source}" NAME_WE)
  configure_file("${source}" "${NYMPH_WEB_OUT}/demo/examples/${name}.yaml" COPYONLY)
  configure_file("${PROJECT_SOURCE_DIR}/examples/${name}.png" "${NYMPH_WEB_OUT}/demo/examples/${name}.png" COPYONLY)
  list(APPEND nymph_example_list "\"${name}\"")
endforeach()
list(JOIN nymph_example_list ", " nymph_example_json)
file(WRITE "${NYMPH_WEB_OUT}/demo/examples/index.json" "[${nymph_example_json}]\n")
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/demo/index.html" "${NYMPH_WEB_OUT}/demo/index.html" COPYONLY)

# The fonts the module fetches (cmake/NymphFonts.cmake copied them to
# <build>/fonts at configure time).
file(COPY "${PROJECT_BINARY_DIR}/fonts" DESTINATION "${NYMPH_WEB_OUT}")

# ── Tests (Node) ─────────────────────────────────────────────────────────────

find_program(NYMPH_NODE node)
if(NYMPH_NODE)
  add_test(NAME nymph_web_node
    COMMAND "${NYMPH_NODE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/node/run.mjs" "${NYMPH_WEB_OUT}" "${PROJECT_SOURCE_DIR}")
  # In Chromium through Playwright (WebGL2 through SwiftShader without a
  # GPU); skipped (77) when Playwright is not installed.
  add_test(NAME nymph_web_browser
    COMMAND "${NYMPH_NODE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/browser/run.mjs" "${NYMPH_WEB_OUT}" "${PROJECT_SOURCE_DIR}")
  set_tests_properties(nymph_web_browser PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 600)
else()
  message(STATUS "nymph: node not found; skipping the web viewer's Node and browser tests")
endif()
