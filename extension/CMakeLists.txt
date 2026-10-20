# MIT License © 2026 Binary Dice Games
#
# The browser extension (spec/05_browser_extension/DESIGN.md section 6 and
# spec/11_cross_browser_extension/DESIGN.md section 5.3), assembled from its
# sources and the web viewer. Output, in <build>/extension/:
#   nymph/                          the Chromium package, unpacked (Chrome, Edge: "Load unpacked")
#   nymph-extension-<version>.zip   the same files, zipped
#   nymph-firefox/, nymph-extension-firefox-<version>.zip   the Firefox package
#   nymph-safari/                   the Safari web extension folder (Xcode converter input)

set(NYMPH_EXT_OUT "${PROJECT_BINARY_DIR}/extension")
set(NYMPH_EXT_DIR "${NYMPH_EXT_OUT}/nymph")
set(NYMPH_EXT_FIREFOX_DIR "${NYMPH_EXT_OUT}/nymph-firefox")
set(NYMPH_EXT_SAFARI_DIR "${NYMPH_EXT_OUT}/nymph-safari")
set(NYMPH_WEB_OUT "${PROJECT_BINARY_DIR}/web")

# The Firefox add-on id (spec/11_cross_browser_extension/DESIGN.md section
# 5.1). It cannot change once published.
set(NYMPH_FIREFOX_ID "nymph@binarydicegames.com" CACHE STRING
  "Firefox add-on id of the extension")

file(GLOB_RECURSE NYMPH_EXT_SOURCES CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*" "${CMAKE_CURRENT_SOURCE_DIR}/icons/*")

# Assembles one browser's package from the shared files and its manifest
# (spec/11_cross_browser_extension/DESIGN.md sections 5.1 and 5.3).
#   browser  chromium | firefox | safari (selects the manifest keys)
#   dir      output folder name under <build>/extension/
#   zip      zip file name, or empty for none
function(nymph_extension_package browser dir zip)
  set(out_dir "${NYMPH_EXT_OUT}/${dir}")
  if(browser STREQUAL "chromium")
    set(NYMPH_EXT_BROWSER_KEYS "\n  \"minimum_chrome_version\": \"116\",")
    set(NYMPH_EXT_PERMISSIONS "\"storage\", \"contextMenus\"")
    set(NYMPH_EXT_BACKGROUND "\"service_worker\": \"src/background/service-worker.js\"")
  elseif(browser STREQUAL "firefox")
    set(NYMPH_EXT_BROWSER_KEYS
      "\n  \"browser_specific_settings\": {\"gecko\": {\"id\": \"${NYMPH_FIREFOX_ID}\", \"strict_min_version\": \"140.0\", \"data_collection_permissions\": {\"required\": [\"none\"]}}},")
    set(NYMPH_EXT_PERMISSIONS "\"storage\", \"contextMenus\"")
    set(NYMPH_EXT_BACKGROUND "\"scripts\": [\"src/background/service-worker.js\"]")
  elseif(browser STREQUAL "safari")
    set(NYMPH_EXT_BROWSER_KEYS "")
    set(NYMPH_EXT_PERMISSIONS "\"storage\"")
    set(NYMPH_EXT_BACKGROUND "\"scripts\": [\"src/background/service-worker.js\"]")
  else()
    message(FATAL_ERROR "unknown extension browser '${browser}'")
  endif()
  configure_file(manifest.json.in "${NYMPH_EXT_OUT}/manifest-${browser}.json" @ONLY)

  set(commands
    COMMAND "${CMAKE_COMMAND}" -E rm -rf "${out_dir}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${out_dir}/viewer"
    COMMAND "${CMAKE_COMMAND}" -E copy "${NYMPH_EXT_OUT}/manifest-${browser}.json" "${out_dir}/manifest.json"
    COMMAND "${CMAKE_COMMAND}" -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/src" "${out_dir}/src"
    COMMAND "${CMAKE_COMMAND}" -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/icons" "${out_dir}/icons"
    COMMAND "${CMAKE_COMMAND}" -E copy
      "${NYMPH_WEB_OUT}/nymph-viewer.js"
      "${NYMPH_WEB_OUT}/nymph-viewer-core.js"
      "${NYMPH_WEB_OUT}/nymph-viewer-core.wasm"
      "${out_dir}/viewer/"
    COMMAND "${CMAKE_COMMAND}" -E copy_directory "${NYMPH_WEB_OUT}/fonts" "${out_dir}/viewer/fonts")
  if(zip)
    set(output "${NYMPH_EXT_OUT}/${zip}")
    list(APPEND commands
      COMMAND "${CMAKE_COMMAND}" -E rm -f "${output}"
      COMMAND "${CMAKE_COMMAND}" -E chdir "${out_dir}" "${CMAKE_COMMAND}" -E tar cf "${output}" --format=zip .)
  else()
    set(output "${out_dir}.stamp")
    list(APPEND commands COMMAND "${CMAKE_COMMAND}" -E touch "${output}")
  endif()
  add_custom_command(
    OUTPUT "${output}"
    ${commands}
    DEPENDS
      "${NYMPH_EXT_OUT}/manifest-${browser}.json"
      ${NYMPH_EXT_SOURCES}
      nymph-viewer nymph-viewer-js
      "${NYMPH_WEB_OUT}/nymph-viewer.js"
    COMMENT "Assembling the ${browser} browser extension"
    VERBATIM)
  set(NYMPH_EXT_OUTPUT_${browser} "${output}" PARENT_SCOPE)
endfunction()

nymph_extension_package(chromium nymph "nymph-extension-${PROJECT_VERSION}.zip")
nymph_extension_package(firefox nymph-firefox "nymph-extension-firefox-${PROJECT_VERSION}.zip")
nymph_extension_package(safari nymph-safari "")
add_custom_target(nymph-extension ALL DEPENDS
  "${NYMPH_EXT_OUTPUT_chromium}" "${NYMPH_EXT_OUTPUT_firefox}" "${NYMPH_EXT_OUTPUT_safari}")

# ── Tests ────────────────────────────────────────────────────────────────────

find_program(NYMPH_NODE node)
if(NYMPH_NODE)
  add_test(NAME nymph_extension_node
    COMMAND "${NYMPH_NODE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/node/run.mjs" "${PROJECT_SOURCE_DIR}" "${NYMPH_EXT_OUT}")
  # Chromium with the extension loaded, through Playwright; skipped (77)
  # when Playwright is not installed.
  add_test(NAME nymph_extension_browser
    COMMAND "${NYMPH_NODE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/browser/run.mjs" "${NYMPH_EXT_DIR}" "${PROJECT_SOURCE_DIR}")
  set_tests_properties(nymph_extension_browser PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 600)
endif()
