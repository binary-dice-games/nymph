# MSVC: link gtest against the same (DLL) CRT as the test executables.
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
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
# Uses libyaml and stb directly; nymph links them PRIVATE.
target_link_libraries(test_dependencies PRIVATE yaml nymph_stb)
nymph_add_test(test_soft_rasterizer test_soft_rasterizer.cpp)
nymph_add_test(test_figure_renderer test_figure_renderer.cpp)
nymph_add_test(test_png_meta test_png_meta.cpp)
# Decodes pixels with its own copy of stb_image.
target_link_libraries(test_png_meta PRIVATE nymph_stb)
nymph_add_test(test_yaml_doc test_yaml_doc.cpp)
nymph_add_test(test_table test_table.cpp)
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
target_compile_definitions(test_layout_elements PRIVATE NYMPH_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
nymph_add_test(test_cli test_cli.cpp)
target_link_libraries(test_cli PRIVATE nymph_cli)
nymph_add_test(test_docs test_docs.cpp)
target_link_libraries(test_docs PRIVATE nymph_cli)
target_compile_definitions(test_docs PRIVATE NYMPH_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
nymph_add_test(test_fonts test_fonts.cpp)
target_compile_definitions(test_fonts PRIVATE NYMPH_FONTS_DIR="${NYMPH_FONTS_DIR}")
nymph_add_test(test_fonts_on_demand test_fonts_on_demand.cpp)
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
nymph_add_test(test_uml_er test_uml_er.cpp)
nymph_add_test(test_uml_dfd_network test_uml_dfd_network.cpp)

# ── Web viewer (spec/04_web_viewer) ──────────────────────────────────────────

nymph_add_test(test_live_view test_live_view.cpp)
target_compile_definitions(test_live_view PRIVATE NYMPH_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
nymph_add_test(test_pick test_pick.cpp)
nymph_add_test(test_imuml_interactive test_imuml_interactive.cpp)
nymph_add_test(test_saved_view test_saved_view.cpp)

# ── MCP server (spec/06_claude_integration) ──────────────────────────────────

if(NYMPH_MCP)
  nymph_add_test(test_mcp test_mcp.cpp)
  target_link_libraries(test_mcp PRIVATE nymph_mcp)
  target_compile_definitions(test_mcp PRIVATE NYMPH_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
  if(TARGET nymph-cli)
    # string(JSON) needs CMake 3.19 in the script, whatever the project minimum.
    add_test(NAME test_mcp_process
      COMMAND ${CMAKE_COMMAND} -DNYMPH=$<TARGET_FILE:nymph-cli> -DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/mcp_process
              -P ${CMAKE_CURRENT_SOURCE_DIR}/mcp_process.cmake)
  endif()
endif()
if(NYMPH_MCP)
  nymph_add_test(test_mcp_tools test_mcp_tools.cpp)
  target_link_libraries(test_mcp_tools PRIVATE nymph_mcp)
  target_compile_definitions(test_mcp_tools PRIVATE NYMPH_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
endif()
