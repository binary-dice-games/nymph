add_executable(nymph-cli main.cpp)
target_link_libraries(nymph-cli PRIVATE nymph_cli)
set_target_properties(nymph-cli PROPERTIES OUTPUT_NAME nymph)
if(NYMPH_MCP)
  target_link_libraries(nymph-cli PRIVATE nymph_mcp)
  target_compile_definitions(nymph-cli PRIVATE NYMPH_HAS_MCP=1)
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
