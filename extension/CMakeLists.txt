# MIT License © 2026 Binary Dice Games
#
# The browser extension (spec/05_browser_extension/DESIGN.md section 6),
# assembled from its sources and the web viewer. Output, in
# <build>/extension/:
#   nymph/                          the unpacked extension (chrome://extensions, "Load unpacked")
#   nymph-extension-<version>.zip   the same files, zipped

set(NYMPH_EXT_OUT "${PROJECT_BINARY_DIR}/extension")
set(NYMPH_EXT_DIR "${NYMPH_EXT_OUT}/nymph")
set(NYMPH_EXT_ZIP "${NYMPH_EXT_OUT}/nymph-extension-${PROJECT_VERSION}.zip")
set(NYMPH_WEB_OUT "${PROJECT_BINARY_DIR}/web")

configure_file(manifest.json.in "${NYMPH_EXT_OUT}/manifest.json" @ONLY)
file(GLOB_RECURSE NYMPH_EXT_SOURCES CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*" "${CMAKE_CURRENT_SOURCE_DIR}/icons/*")

add_custom_command(
  OUTPUT "${NYMPH_EXT_ZIP}"
  COMMAND "${CMAKE_COMMAND}" -E rm -rf "${NYMPH_EXT_DIR}"
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${NYMPH_EXT_DIR}/viewer"
  COMMAND "${CMAKE_COMMAND}" -E copy "${NYMPH_EXT_OUT}/manifest.json" "${NYMPH_EXT_DIR}/manifest.json"
  COMMAND "${CMAKE_COMMAND}" -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/src" "${NYMPH_EXT_DIR}/src"
  COMMAND "${CMAKE_COMMAND}" -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/icons" "${NYMPH_EXT_DIR}/icons"
  COMMAND "${CMAKE_COMMAND}" -E copy
    "${NYMPH_WEB_OUT}/nymph-viewer.js"
    "${NYMPH_WEB_OUT}/nymph-viewer-core.js"
    "${NYMPH_WEB_OUT}/nymph-viewer-core.wasm"
    "${NYMPH_EXT_DIR}/viewer/"
  COMMAND "${CMAKE_COMMAND}" -E copy_directory "${NYMPH_WEB_OUT}/fonts" "${NYMPH_EXT_DIR}/viewer/fonts"
  COMMAND "${CMAKE_COMMAND}" -E rm -f "${NYMPH_EXT_ZIP}"
  COMMAND "${CMAKE_COMMAND}" -E chdir "${NYMPH_EXT_DIR}" "${CMAKE_COMMAND}" -E tar cf "${NYMPH_EXT_ZIP}" --format=zip .
  DEPENDS
    "${NYMPH_EXT_OUT}/manifest.json"
    ${NYMPH_EXT_SOURCES}
    nymph-viewer nymph-viewer-js
    "${NYMPH_WEB_OUT}/nymph-viewer.js"
  COMMENT "Assembling the browser extension"
  VERBATIM)
add_custom_target(nymph-extension ALL DEPENDS "${NYMPH_EXT_ZIP}")

# ── Tests ────────────────────────────────────────────────────────────────────

find_program(NYMPH_NODE node)
if(NYMPH_NODE)
  add_test(NAME nymph_extension_node
    COMMAND "${NYMPH_NODE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/node/run.mjs" "${PROJECT_SOURCE_DIR}")
  # Chromium with the extension loaded, through Playwright; skipped (77)
  # when Playwright is not installed.
  add_test(NAME nymph_extension_browser
    COMMAND "${NYMPH_NODE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/browser/run.mjs" "${NYMPH_EXT_DIR}" "${PROJECT_SOURCE_DIR}")
  set_tests_properties(nymph_extension_browser PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 600)
endif()
