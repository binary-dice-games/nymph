add_executable(nymph-cli main.cpp)
target_link_libraries(nymph-cli PRIVATE nymph_cli)
set_target_properties(nymph-cli PROPERTIES OUTPUT_NAME nymph)
# Windows: the icon compiled into nymph.exe for Explorer (app/nymph.rc.in).
# The running window sets its own icon (src/window/window_icon.cpp).
if(WIN32)
  set(NYMPH_ICON_ICO "${PROJECT_SOURCE_DIR}/resources/icon.ico")
  configure_file(nymph.rc.in "${CMAKE_CURRENT_BINARY_DIR}/nymph.rc" @ONLY)
  target_sources(nymph-cli PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/nymph.rc")
  set_property(SOURCE "${CMAKE_CURRENT_BINARY_DIR}/nymph.rc" APPEND PROPERTY OBJECT_DEPENDS "${NYMPH_ICON_ICO}")
endif()
if(NYMPH_VIEW)
  target_link_libraries(nymph-cli PRIVATE nymph_window)
  target_compile_definitions(nymph-cli PRIVATE NYMPH_HAS_VIEW=1)
endif()
if(NYMPH_MCP)
  target_link_libraries(nymph-cli PRIVATE nymph_mcp)
  target_compile_definitions(nymph-cli PRIVATE NYMPH_HAS_MCP=1)
endif()

# Keeps docs/elements.md and docs/elements/ (the paged element reference,
# `nymph schema --markdown --paged`) in step with the program after every
# build; tests/test_docs.cpp fails when they are not. Only for a native
# build of nymph itself: not as a subproject, not cross-compiled (the
# executable cannot run here), not a release (a source tarball's docs are
# read-only in effect: they are already right).
if(PROJECT_IS_TOP_LEVEL AND NOT CMAKE_CROSSCOMPILING AND NOT NYMPH_RELEASE AND NOT NYMPH_WEB)
  set(_nymph_update_docs_default ON)
else()
  set(_nymph_update_docs_default OFF)
endif()
option(NYMPH_UPDATE_DOCS "Regenerate docs/elements.md and docs/elements/ after building nymph" ${_nymph_update_docs_default})
if(NYMPH_UPDATE_DOCS)
  add_custom_command(TARGET nymph-cli POST_BUILD
    COMMAND nymph-cli schema --markdown --paged "${PROJECT_SOURCE_DIR}/docs"
    COMMENT "Updating docs/elements.md and docs/elements/"
    VERBATIM)
endif()

# macOS release: ld64 has no -s, so strip after the link; stripping drops
# the linker's ad hoc signature, without which arm64 runs nothing
# (cmake/NymphRelease.cmake).
# NYMPH_CODESIGN_IDENTITY: "-" (ad hoc), or a Developer ID certificate in
# the keychain, signed with the hardened runtime and a timestamp, as
# notarization requires (spec/07_release_pipeline/DESIGN.md section 4.8).
if(NYMPH_RELEASE AND APPLE)
  set(NYMPH_CODESIGN_IDENTITY "-" CACHE STRING "codesign identity of the release executable ('-': ad hoc)")
  find_program(NYMPH_CODESIGN codesign REQUIRED)
  find_program(NYMPH_STRIP NAMES strip HINTS "${CMAKE_STRIP}" REQUIRED)
  if(NYMPH_CODESIGN_IDENTITY STREQUAL "-")
    set(_nymph_sign_options "")
  else()
    set(_nymph_sign_options --options runtime --timestamp)
  endif()
  add_custom_command(TARGET nymph-cli POST_BUILD
    COMMAND "${NYMPH_STRIP}" -x "$<TARGET_FILE:nymph-cli>"
    COMMAND "${NYMPH_CODESIGN}" --force ${_nymph_sign_options} --sign "${NYMPH_CODESIGN_IDENTITY}"
            "$<TARGET_FILE:nymph-cli>"
    COMMENT "Stripping and signing nymph (${NYMPH_CODESIGN_IDENTITY})"
    VERBATIM)
endif()
