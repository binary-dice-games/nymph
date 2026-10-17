// MIT License © 2026 Binary Dice Games
/// @file gl_api.cpp
/// @brief gl_api::load(): fills the function table by name.
#include "render/gl/gl_api.hpp"
#include <cstring>

namespace bdg::nymph {

namespace {

#define NYMPH_GL_NAME(ret, name, params) "gl" #name,
constexpr const char* kNames[] = {NYMPH_GL_FUNCTIONS(NYMPH_GL_NAME)};
#undef NYMPH_GL_NAME

/// Copies a looked-up address into a function pointer of any type.
template <typename Fn>
bool assign(Fn& fn, void* address) {
  static_assert(sizeof(Fn) == sizeof(void*), "function and data pointers differ in size");
  std::memcpy(&fn, &address, sizeof(fn));
  return address != nullptr;
}

} // namespace

std::string gl_api::load(gl_lookup_fn lookup, void* user) {
  std::string missing;
#define NYMPH_GL_LOAD(ret, name, params)                            \
  if (!assign(name, lookup("gl" #name, user)) && missing.empty()) { \
    missing = "gl" #name;                                           \
  }
  NYMPH_GL_FUNCTIONS(NYMPH_GL_LOAD)
#undef NYMPH_GL_LOAD
  return missing;
}

const char* const* gl_api::names(size_t& count) {
  count = sizeof(kNames) / sizeof(kNames[0]);
  return kNames;
}

} // namespace bdg::nymph
