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
