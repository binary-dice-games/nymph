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

add_executable(nymph-viewer nymph_web.cpp)
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
  "-sEXPORTED_FUNCTIONS=['_malloc','_free']"
  "-sEXPORTED_RUNTIME_METHODS=['UTF8ToString','HEAPU8']"
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

# The fonts the module fetches (cmake/NymphFonts.cmake copied them to
# <build>/fonts at configure time).
file(COPY "${PROJECT_BINARY_DIR}/fonts" DESTINATION "${NYMPH_WEB_OUT}")

# ── Tests (Node) ─────────────────────────────────────────────────────────────

find_program(NYMPH_NODE node)
if(NYMPH_NODE)
  add_test(NAME nymph_web_node
    COMMAND "${NYMPH_NODE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/node/run.mjs" "${NYMPH_WEB_OUT}" "${PROJECT_SOURCE_DIR}")
else()
  message(STATUS "nymph: node not found; skipping the web viewer's Node tests")
endif()
