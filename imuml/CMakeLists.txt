# ImUml: UML diagrams for Dear ImGui (spec/03_uml_diagrams/DESIGN.md).
# Depends only on ImGui: IMUML_IMGUI_TARGET names the target providing it.

if(NOT DEFINED IMUML_IMGUI_TARGET)
  set(IMUML_IMGUI_TARGET nymph_imgui)
endif()

add_library(imuml STATIC
  imuml.h
  imuml_internal.h
  imuml.cpp
  imuml_items.cpp
  imuml_render.cpp
  imuml_graph_layout.cpp
  imuml_sequence_layout.cpp
)
target_include_directories(imuml PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(imuml PUBLIC ${IMUML_IMGUI_TARGET})
set_target_properties(imuml PROPERTIES FOLDER imuml)
