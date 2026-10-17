add_executable(nymph-cli main.cpp)
target_link_libraries(nymph-cli PRIVATE nymph)
set_target_properties(nymph-cli PROPERTIES OUTPUT_NAME nymph)
