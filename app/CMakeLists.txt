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
if(NYMPH_RELEASE AND APPLE)
  find_program(NYMPH_CODESIGN codesign REQUIRED)
  find_program(NYMPH_STRIP NAMES strip HINTS "${CMAKE_STRIP}" REQUIRED)
  add_custom_command(TARGET nymph-cli POST_BUILD
    COMMAND "${NYMPH_STRIP}" -x "$<TARGET_FILE:nymph-cli>"
    COMMAND "${NYMPH_CODESIGN}" --force --sign - "$<TARGET_FILE:nymph-cli>"
    COMMENT "Stripping and signing nymph (ad hoc)"
    VERBATIM)
endif()
