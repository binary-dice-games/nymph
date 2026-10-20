# MSVC: link gtest against the same CRT as the test executables: the DLL
# one, or the static one of a release build (cmake/NymphRelease.cmake).
if(NYMPH_RELEASE)
  set(gtest_force_shared_crt OFF CACHE BOOL "" FORCE)
else()
  set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
endif()
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
add_subdirectory(
    "${PROJECT_SOURCE_DIR}/extern/googletest"
    "${PROJECT_BINARY_DIR}/googletest"
)

set_target_properties(gtest      PROPERTIES FOLDER extern)
set_target_properties(gtest_main PROPERTIES FOLDER extern)
set_target_properties(gmock      PROPERTIES FOLDER extern)
set_target_properties(gmock_main PROPERTIES FOLDER extern)

# Helper macro: creates a test executable linked against nymph and GoogleTest.
macro(nymph_add_test TESTNAME)
    add_executable(${TESTNAME} ${ARGN})
    target_link_libraries(${TESTNAME} PRIVATE nymph gtest gmock gtest_main)
    gtest_discover_tests(${TESTNAME})
    set_target_properties(${TESTNAME} PROPERTIES FOLDER tests)
endmacro()

nymph_add_test(test_version test_version.cpp)
nymph_add_test(test_dependencies test_dependencies.cpp)
nymph_add_test(test_charconv test_charconv.cpp)
# Uses libyaml and stb directly; nymph links them PRIVATE.
target_link_libraries(test_dependencies PRIVATE yaml nymph_stb)
nymph_add_test(test_soft_rasterizer test_soft_rasterizer.cpp)
nymph_add_test(test_figure_renderer test_figure_renderer.cpp)
nymph_add_test(test_png_meta test_png_meta.cpp)
# Decodes pixels with its own copy of stb_image.
target_link_libraries(test_png_meta PRIVATE nymph_stb)
nymph_add_test(test_yaml_doc test_yaml_doc.cpp)
nymph_add_test(test_table test_table.cpp)
nymph_add_test(test_source_edit test_source_edit.cpp)
nymph_add_test(test_field_value test_field_value.cpp)
nymph_add_test(test_editor_model test_editor_model.cpp)
nymph_add_test(test_inspector test_inspector.cpp)
nymph_add_test(test_view_edit_flow test_view_edit_flow.cpp)
nymph_add_test(test_schema test_schema.cpp)
nymph_add_test(test_bind test_bind.cpp)
nymph_add_test(test_plot2d test_plot2d.cpp)
# Reads the sample sources in tests/data in place.
target_compile_definitions(test_plot2d PRIVATE NYMPH_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
nymph_add_test(test_plot3d test_plot3d.cpp)
target_compile_definitions(test_plot3d PRIVATE NYMPH_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
nymph_add_test(test_gantt test_gantt.cpp)
nymph_add_test(test_sankey test_sankey.cpp)
nymph_add_test(test_layout test_layout.cpp)
nymph_add_test(test_layout_elements test_layout_elements.cpp)
nymph_add_test(test_ui_screen test_ui_screen.cpp)
nymph_add_test(test_ui_flow test_ui_flow.cpp)
nymph_add_test(test_ui_widgets test_ui_widgets.cpp)
nymph_add_test(test_placeholders test_placeholders.cpp)
nymph_add_test(test_ui_containers test_ui_containers.cpp)
nymph_add_test(test_ui_table test_ui_table.cpp)
nymph_add_test(test_ui_menus test_ui_menus.cpp)
nymph_add_test(test_live_view_ui test_live_view_ui.cpp)
target_compile_definitions(test_layout_elements PRIVATE NYMPH_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
nymph_add_test(test_cli test_cli.cpp)
target_link_libraries(test_cli PRIVATE nymph_cli)
nymph_add_test(test_docs test_docs.cpp)
target_link_libraries(test_docs PRIVATE nymph_cli)
target_compile_definitions(test_docs PRIVATE NYMPH_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
nymph_add_test(test_fonts test_fonts.cpp)
target_compile_definitions(test_fonts PRIVATE NYMPH_FONTS_DIR="${NYMPH_FONTS_DIR}")
nymph_add_test(test_fonts_on_demand test_fonts_on_demand.cpp)
target_link_libraries(test_fonts_on_demand PRIVATE nymph_stb)
target_compile_definitions(test_fonts_on_demand PRIVATE
  NYMPH_FONTS_DIR="${NYMPH_FONTS_DIR}" NYMPH_SOURCE_DIR="${PROJECT_SOURCE_DIR}")

# ── OpenGL rasterizer (spec/02_opengl_rasterizer) ────────────────────────────

# The nymph executable must not link any OpenGL library.
if(TARGET nymph-cli)
  if(MSVC)
    find_program(NYMPH_DUMPBIN dumpbin)
  endif()
  if(NYMPH_DUMPBIN)
    set(_nymph_linkage_tool -DDUMPBIN=${NYMPH_DUMPBIN})
  elseif(APPLE)
    find_program(NYMPH_OTOOL otool)
    if(NYMPH_OTOOL)
      set(_nymph_linkage_tool -DOTOOL=${NYMPH_OTOOL})
    endif()
  elseif(CMAKE_OBJDUMP)
    set(_nymph_linkage_tool -DOBJDUMP=${CMAKE_OBJDUMP})
  endif()
  if(_nymph_linkage_tool)
    add_test(NAME nymph_no_gl_linkage
      COMMAND ${CMAKE_COMMAND} -DBINARY=$<TARGET_FILE:nymph-cli> ${_nymph_linkage_tool}
              -P ${PROJECT_SOURCE_DIR}/cmake/NymphCheckLinkage.cmake)
  else()
    message(STATUS "nymph: no objdump or dumpbin found; skipping the OpenGL linkage test")
  endif()
endif()

# Runs with or without NYMPH_OPENGL: the fallback must work either way.
nymph_add_test(test_renderer test_renderer.cpp)
target_compile_definitions(test_renderer PRIVATE NYMPH_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
target_link_libraries(test_renderer PRIVATE nymph_stb)

if(NYMPH_OPENGL)
  nymph_add_test(test_gl_loader test_gl_loader.cpp)
  nymph_add_test(test_gl_device test_gl_device.cpp)
  nymph_add_test(test_gl_rasterizer test_gl_rasterizer.cpp)
  target_compile_definitions(test_gl_rasterizer PRIVATE NYMPH_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
endif()

# ── UML diagrams (spec/03_uml_diagrams) ──────────────────────────────────────

nymph_add_test(test_imuml_graph_layout test_imuml_graph_layout.cpp)
nymph_add_test(test_imuml_sequence_layout test_imuml_sequence_layout.cpp)
nymph_add_test(test_imuml_context test_imuml_context.cpp)
nymph_add_test(test_uml_class test_uml_class.cpp)
nymph_add_test(test_uml_state test_uml_state.cpp)
nymph_add_test(test_uml_activity_usecase test_uml_activity_usecase.cpp)
nymph_add_test(test_uml_sequence test_uml_sequence.cpp)
nymph_add_test(test_flowchart test_flowchart.cpp)
nymph_add_test(test_uml_item_style test_uml_item_style.cpp)
nymph_add_test(test_uml_er test_uml_er.cpp)
nymph_add_test(test_uml_dfd_network test_uml_dfd_network.cpp)

# ── Web viewer (spec/04_web_viewer) ──────────────────────────────────────────

nymph_add_test(test_live_view test_live_view.cpp)
target_compile_definitions(test_live_view PRIVATE NYMPH_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
nymph_add_test(test_live_view_edit test_live_view_edit.cpp)
target_compile_definitions(test_live_view_edit PRIVATE NYMPH_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
nymph_add_test(test_pick test_pick.cpp)
nymph_add_test(test_imuml_interactive test_imuml_interactive.cpp)
nymph_add_test(test_imuml_text_runs test_imuml_text_runs.cpp)
nymph_add_test(test_saved_view test_saved_view.cpp)

# `nymph view` up to the window (spec/04_web_viewer, plan step 14).
if(NYMPH_VIEW)
  nymph_add_test(test_view_command test_view_command.cpp)
  target_link_libraries(test_view_command PRIVATE nymph_window)
  nymph_add_test(test_window_icon test_window_icon.cpp)
  target_link_libraries(test_window_icon PRIVATE nymph_window)
endif()

# Script tests run a cross-built nymph through the toolchain's emulator,
# passed '|'-separated (a ';' list would split on the command line).
string(REPLACE ";" "|" _nymph_runner "${CMAKE_CROSSCOMPILING_EMULATOR}")

# ── MCP server (spec/06_claude_integration) ──────────────────────────────────

if(NYMPH_MCP)
  nymph_add_test(test_mcp test_mcp.cpp)
  target_link_libraries(test_mcp PRIVATE nymph_mcp)
  target_compile_definitions(test_mcp PRIVATE NYMPH_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
  if(TARGET nymph-cli)
    # string(JSON) needs CMake 3.19 in the script, whatever the project minimum.
    add_test(NAME test_mcp_process
      COMMAND ${CMAKE_COMMAND} -DNYMPH=$<TARGET_FILE:nymph-cli> -DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/mcp_process
              "-DRUNNER=${_nymph_runner}" -DEXPECT_VIEWER=$<BOOL:${NYMPH_MCP_VIEWER}>
              -P ${CMAKE_CURRENT_SOURCE_DIR}/mcp_process.cmake)
  endif()
endif()

# ── Release executables (spec/06_claude_integration step 7) ─────────────────

if(NYMPH_RELEASE AND TARGET nymph-cli)
  # Exact on MSVC x86-64, the toolchain the committed PNGs were drawn with:
  # another libm (glibc, mingw) rounds sin/cos/exp differently, which moves
  # a few pixels of the curved examples.
  string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" _nymph_cpu)
  if(MSVC AND _nymph_cpu MATCHES "^(x86_64|amd64)$")
    set(_nymph_exact ON)
  else()
    set(_nymph_exact OFF)
  endif()
  add_test(NAME nymph_release_examples
    COMMAND ${CMAKE_COMMAND} -DNYMPH=$<TARGET_FILE:nymph-cli> -DEXAMPLES=${PROJECT_SOURCE_DIR}/examples -DEXACT=${_nymph_exact}
            -DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/release_examples "-DRUNNER=${_nymph_runner}"
            -P ${CMAKE_CURRENT_SOURCE_DIR}/release_examples.cmake)
  if(NYMPH_MCPB_FILE)
    if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
      set(_nymph_platform win32)
    elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
      set(_nymph_platform darwin)
    else()
      set(_nymph_platform linux)
    endif()
    add_test(NAME nymph_release_mcpb
      COMMAND ${CMAKE_COMMAND} -DMCPB=${NYMPH_MCPB_FILE} -DVERSION=${PROJECT_VERSION} -DPLATFORM=${_nymph_platform}
              -DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/release_mcpb "-DRUNNER=${_nymph_runner}"
              -P ${CMAKE_CURRENT_SOURCE_DIR}/mcpb_check.cmake)
  endif()
  if(NYMPH_SKILL_FILE)
    add_test(NAME nymph_release_skill
      COMMAND ${CMAKE_COMMAND} -DSKILL_ZIP=${NYMPH_SKILL_FILE} -DSOURCE_DIR=${PROJECT_SOURCE_DIR}
              -DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/release_skill -P ${CMAKE_CURRENT_SOURCE_DIR}/skill_check.cmake)
  endif()
  if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    find_program(NYMPH_READELF NAMES readelf ${CMAKE_READELF})
    if(NYMPH_READELF)
      add_test(NAME nymph_release_linkage
        COMMAND ${CMAKE_COMMAND} -DREADELF=${NYMPH_READELF} -DNYMPH=$<TARGET_FILE:nymph-cli>
                -P ${CMAKE_CURRENT_SOURCE_DIR}/release_linkage.cmake)
    endif()
  elseif(APPLE)
    find_program(NYMPH_OTOOL otool)
    if(NYMPH_OTOOL)
      add_test(NAME nymph_release_linkage
        COMMAND ${CMAKE_COMMAND} -DOTOOL=${NYMPH_OTOOL} -DNYMPH=$<TARGET_FILE:nymph-cli>
                -P ${CMAKE_CURRENT_SOURCE_DIR}/release_linkage.cmake)
    endif()
    # The arm64 executable runs only while its signature is valid.
    add_test(NAME nymph_release_signature COMMAND codesign --verify --verbose "$<TARGET_FILE:nymph-cli>")
  endif()
endif()
if(NYMPH_MCP)
  nymph_add_test(test_mcp_tools test_mcp_tools.cpp)
  target_link_libraries(test_mcp_tools PRIVATE nymph_mcp)
  target_compile_definitions(test_mcp_tools PRIVATE NYMPH_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
endif()

# ── Claude Code plugin (spec/06_claude_integration step 8) ──────────────────

add_test(NAME nymph_plugin_in_sync
  COMMAND ${CMAKE_COMMAND} -DSOURCE_DIR=${PROJECT_SOURCE_DIR} -DVERSION=${PROJECT_VERSION}
          -P ${CMAKE_CURRENT_SOURCE_DIR}/plugin_sync_check.cmake)
if(NOT WIN32)
  find_program(NYMPH_SH sh)
  if(NYMPH_SH)
    add_test(NAME nymph_plugin_launcher
      COMMAND ${NYMPH_SH} ${CMAKE_CURRENT_SOURCE_DIR}/plugin_launcher.sh
              ${PROJECT_SOURCE_DIR}/claude/plugin/bin/nymph ${CMAKE_CURRENT_BINARY_DIR}/plugin_launcher)
    set_tests_properties(nymph_plugin_launcher PROPERTIES SKIP_RETURN_CODE 77)
  endif()
endif()
# Claude Code's own check of the marketplace and the plugin, when installed.
find_program(NYMPH_CLAUDE claude)
if(NYMPH_CLAUDE)
  add_test(NAME nymph_plugin_validate_marketplace COMMAND ${NYMPH_CLAUDE} plugin validate --strict ${PROJECT_SOURCE_DIR})
  add_test(NAME nymph_plugin_validate_plugin
    COMMAND ${NYMPH_CLAUDE} plugin validate --strict ${PROJECT_SOURCE_DIR}/claude/plugin)
endif()

# ── Install (spec/07_release_pipeline step 3) ───────────────────────────────

if(NYMPH_INSTALL AND TARGET nymph-cli)
  add_test(NAME nymph_install
    COMMAND ${CMAKE_COMMAND} -DBUILD_DIR=${PROJECT_BINARY_DIR} -DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/install
            -DVERSION=${PROJECT_VERSION} -DSOURCE=${PROJECT_SOURCE_DIR}/examples/dashboard.yaml
            "-DCONFIG=$<CONFIG>" -DWINDOWS=$<BOOL:${WIN32}> "-DRUNNER=${_nymph_runner}"
            -P ${CMAKE_CURRENT_SOURCE_DIR}/install_check.cmake)
endif()
add_test(NAME nymph_install_subproject
  COMMAND ${CMAKE_COMMAND} -DSOURCE_DIR=${PROJECT_SOURCE_DIR} -DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/install_subproject
          "-DGENERATOR=${CMAKE_GENERATOR}" -P ${CMAKE_CURRENT_SOURCE_DIR}/install_subproject.cmake)
set_tests_properties(nymph_install_subproject PROPERTIES TIMEOUT 600)
# The PATH scripts, in a scratch HOME / a stand-in for the user's Path.
if(NOT WIN32)
  find_program(NYMPH_SH sh)
  if(NYMPH_SH)
    add_test(NAME nymph_install_script_sh
      COMMAND ${NYMPH_SH} ${CMAKE_CURRENT_SOURCE_DIR}/install_script.sh
              ${PROJECT_SOURCE_DIR}/packaging/install/install.sh ${CMAKE_CURRENT_BINARY_DIR}/install_script_sh)
    set_tests_properties(nymph_install_script_sh PROPERTIES SKIP_RETURN_CODE 77)
  endif()
endif()
find_program(NYMPH_POWERSHELL NAMES pwsh powershell)
if(NYMPH_POWERSHELL)
  add_test(NAME nymph_install_script_ps1
    COMMAND ${NYMPH_POWERSHELL} -NoProfile -ExecutionPolicy Bypass -File ${CMAKE_CURRENT_SOURCE_DIR}/install_script.ps1
            ${PROJECT_SOURCE_DIR}/packaging/install/install.ps1 ${CMAKE_CURRENT_BINARY_DIR}/install_script_ps1)
endif()

# ── Plugin pin and version (spec/07_release_pipeline step 6) ────────────────

add_test(NAME nymph_plugin_sync_versions
  COMMAND ${CMAKE_COMMAND} -DSOURCE_DIR=${PROJECT_SOURCE_DIR} -DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/plugin_sync_versions
          -P ${CMAKE_CURRENT_SOURCE_DIR}/plugin_sync_versions.cmake)

# ── Gemini primer (spec/10_gemini_extension step 8) ─────────────────────────

add_test(NAME nymph_gemini_primer
  COMMAND ${CMAKE_COMMAND} -DSOURCE_DIR=${PROJECT_SOURCE_DIR} -DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/gemini_primer
          -P ${CMAKE_CURRENT_SOURCE_DIR}/gemini_primer_check.cmake)

# ── Chrome Web Store publish script (spec/07_release_pipeline step 8) ───────

find_program(NYMPH_NODE_TOOL node)
if(NYMPH_NODE_TOOL AND NOT WIN32)
  add_test(NAME nymph_chrome_publish
    COMMAND ${NYMPH_NODE_TOOL} ${CMAKE_CURRENT_SOURCE_DIR}/chrome_publish_test.mjs
            ${PROJECT_SOURCE_DIR}/packaging/chrome/publish.sh ${CMAKE_CURRENT_BINARY_DIR}/chrome_publish)
endif()
