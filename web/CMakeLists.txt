# MIT License © 2026 Binary Dice Games
#
# The web viewer (spec/04_web_viewer/DESIGN.md section 4, "Web module"):
# the nymph library compiled to WebAssembly, with its JavaScript API.
# Output, in <build>/web/:
#   nymph-viewer.js         the public ES module (web/js/nymph-viewer.js)
#   nymph-viewer-core.js    Emscripten's loader, imported by it
#   nymph-viewer-core.wasm  the module
#   nymph-viewer.single.js  all three in one file (the wasm inlined, about a third larger)
#   fonts/                  the fonts not embedded, fetched on demand
#   demo/                   every example, live next to its PNG

set(NYMPH_WEB_OUT "${PROJECT_BINARY_DIR}/web")

# Compiled once, linked twice (with the wasm beside the loader, and inlined).
add_library(nymph_web_objects OBJECT
  nymph_web.cpp
  webgl_presenter.hpp
  webgl_presenter.cpp
  # Dear ImGui's OpenGL3 backend, as WebGL2 (OpenGL ES 3.0).
  "${NYMPH_EXTERN_DIR}/imgui/backends/imgui_impl_opengl3.cpp"
)
target_include_directories(nymph_web_objects PRIVATE "${NYMPH_EXTERN_DIR}/imgui/backends" "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_definitions(nymph_web_objects PRIVATE IMGUI_IMPL_OPENGL_ES3)
target_link_libraries(nymph_web_objects PUBLIC nymph)

set(NYMPH_WEB_LINK_OPTIONS
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

add_executable(nymph-viewer $<TARGET_OBJECTS:nymph_web_objects>)
target_link_libraries(nymph-viewer PRIVATE nymph)
set_target_properties(nymph-viewer PROPERTIES
  OUTPUT_NAME nymph-viewer-core
  SUFFIX ".js"
  RUNTIME_OUTPUT_DIRECTORY "${NYMPH_WEB_OUT}"
)
target_link_options(nymph-viewer PRIVATE ${NYMPH_WEB_LINK_OPTIONS})

# One file: the loader with the wasm inlined, and the API appended to the
# same ES module (it then uses createNymphCore in scope, not an import).
add_executable(nymph-viewer-single $<TARGET_OBJECTS:nymph_web_objects>)
target_link_libraries(nymph-viewer-single PRIVATE nymph)
set_target_properties(nymph-viewer-single PROPERTIES
  OUTPUT_NAME nymph-viewer.single
  SUFFIX ".js"
  RUNTIME_OUTPUT_DIRECTORY "${NYMPH_WEB_OUT}"
  LINK_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/js/nymph-viewer.js"
)
target_link_options(nymph-viewer-single PRIVATE
  ${NYMPH_WEB_LINK_OPTIONS}
  -sSINGLE_FILE=1
  "--extern-post-js=${CMAKE_CURRENT_SOURCE_DIR}/js/nymph-viewer.js"
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

# ── The editor module (spec/15_web_app/DESIGN.md sections 4 and 5) ──────────

# A second module, loaded only to edit, so the viewer stays small: the whole
# library plus the editor workspace, drawn into its own WebGL2 canvas.
# Output in <build>/web/editor/: nymph-editor.js (the API, which imports
# ../nymph-viewer.js for its font loader), nymph-editor-core.js and .wasm.
set(NYMPH_EDITOR_OUT "${NYMPH_WEB_OUT}/editor")
add_executable(nymph-editor editor/nymph_editor_web.cpp)
target_link_libraries(nymph-editor PRIVATE nymph_workspace)
set_target_properties(nymph-editor PROPERTIES
  OUTPUT_NAME nymph-editor-core
  SUFFIX ".js"
  RUNTIME_OUTPUT_DIRECTORY "${NYMPH_EDITOR_OUT}"
)
set(NYMPH_EDITOR_LINK_OPTIONS ${NYMPH_WEB_LINK_OPTIONS})
list(TRANSFORM NYMPH_EDITOR_LINK_OPTIONS REPLACE "createNymphCore" "createNymphEditorCore")
target_link_options(nymph-editor PRIVATE ${NYMPH_EDITOR_LINK_OPTIONS})
add_custom_command(
  OUTPUT "${NYMPH_EDITOR_OUT}/nymph-editor.js"
  COMMAND "${CMAKE_COMMAND}" -E copy "${CMAKE_CURRENT_SOURCE_DIR}/js/nymph-editor.js" "${NYMPH_EDITOR_OUT}/nymph-editor.js"
  DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/js/nymph-editor.js"
  COMMENT "Copying nymph-editor.js"
  VERBATIM)
add_custom_target(nymph-editor-js ALL DEPENDS "${NYMPH_EDITOR_OUT}/nymph-editor.js")
add_dependencies(nymph-editor nymph-editor-js)

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

# ── The nymph MCP App (spec/06_claude_integration/DESIGN.md section 4.2) ────

# One HTML page with the single-file viewer and the host bridge inlined; the
# native build embeds it with NYMPH_MCP_VIEWER=<build-web>/claude/viewer.html.
set(NYMPH_CLAUDE_APP "${PROJECT_BINARY_DIR}/claude/viewer.html")
add_custom_command(
  OUTPUT "${NYMPH_CLAUDE_APP}"
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${PROJECT_BINARY_DIR}/claude"
  COMMAND "${CMAKE_COMMAND}"
          "-DTEMPLATE=${PROJECT_SOURCE_DIR}/claude/app/viewer.html.in"
          "-DVIEWER=$<TARGET_FILE:nymph-viewer-single>"
          "-DAPP=${PROJECT_SOURCE_DIR}/claude/app/app.js"
          "-DOUTPUT=${NYMPH_CLAUDE_APP}"
          -P "${PROJECT_SOURCE_DIR}/cmake/NymphClaudeApp.cmake"
  DEPENDS nymph-viewer-single
          "${PROJECT_SOURCE_DIR}/claude/app/viewer.html.in"
          "${PROJECT_SOURCE_DIR}/claude/app/app.js"
          "${PROJECT_SOURCE_DIR}/cmake/NymphClaudeApp.cmake"
  COMMENT "Writing the nymph MCP App"
  VERBATIM)
add_custom_target(nymph-claude-app ALL DEPENDS "${NYMPH_CLAUDE_APP}")

# ── Tests (Node) ─────────────────────────────────────────────────────────────

find_program(NYMPH_NODE node)
if(NYMPH_NODE)
  add_test(NAME nymph_web_node
    COMMAND "${NYMPH_NODE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/node/run.mjs" "${NYMPH_WEB_OUT}" "${PROJECT_SOURCE_DIR}")
  # The editor module, headless (spec/15_web_app plan step 6), and both
  # modules' sizes against web/size_budget.json (plan step 1).
  add_test(NAME nymph_web_editor_node
    COMMAND "${NYMPH_NODE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/node/editor.mjs" "${NYMPH_WEB_OUT}" "${PROJECT_SOURCE_DIR}")
  add_test(NAME nymph_web_size
    COMMAND "${NYMPH_NODE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/node/size.mjs" "${NYMPH_WEB_OUT}" "${PROJECT_SOURCE_DIR}")
  # In Chromium through Playwright (WebGL2 through SwiftShader without a
  # GPU); skipped (77) when Playwright is not installed.
  add_test(NAME nymph_web_browser
    COMMAND "${NYMPH_NODE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/browser/run.mjs" "${NYMPH_WEB_OUT}" "${PROJECT_SOURCE_DIR}")
  set_tests_properties(nymph_web_browser PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 600)
else()
  message(STATUS "nymph: node not found; skipping the web viewer's Node and browser tests")
endif()
