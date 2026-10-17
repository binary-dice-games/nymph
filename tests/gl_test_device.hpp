// MIT License © 2026 Binary Dice Games
/// @file gl_test_device.hpp
/// @brief A process-wide OpenGL device for the OpenGL tests, software GL
///        allowed, or the reason there is none (tests then skip).
#pragma once

#include <gtest/gtest.h>
#include "render/gl/gl_device.hpp"

namespace bdg::nymph {

/// @brief The shared test device (created on first use), or null.
inline gl_device* test_device(std::string* reason = nullptr) {
  static gl_device::result r = [] {
    gl_device_options o;
    o.allow_software_gl = true;
    return gl_device::create(o);
  }();
  if (reason != nullptr) {
    *reason = r.reason;
  }
  return r.device.get();
}

} // namespace bdg::nymph

/// Skips the current test when no OpenGL device can be created.
#define NYMPH_REQUIRE_GL()                                        \
  do {                                                            \
    std::string nymph_gl_reason;                                  \
    if (::bdg::nymph::test_device(&nymph_gl_reason) == nullptr) { \
      GTEST_SKIP() << "no OpenGL device: " << nymph_gl_reason;    \
    }                                                             \
  } while (0)
