// MIT License © 2026 Binary Dice Games
/// @file test_gl_device.cpp
/// @brief spec/02_opengl_rasterizer plan step 3: headless OpenGL device
///        creation, context switching and failure reasons.
#include <gtest/gtest.h>
#include <cstdio>
#include <stdexcept>
#include <string>
#include "gl_test_device.hpp"
#include "render/gl/gl_device.hpp"

namespace bdg::nymph {
namespace {

#if defined(_WIN32)
const char* const kMissing = "nymph-no-such-opengl32.dll";
#else
const char* const kMissing = "nymph-no-such-libEGL.so";
#endif

/// The version a device's context reports, as major * 10 + minor.
int context_version(const gl_device& d) {
  GLint major = 0;
  GLint minor = 0;
  d.gl().GetIntegerv(GL_MAJOR_VERSION, &major);
  d.gl().GetIntegerv(GL_MINOR_VERSION, &minor);
  return major * 10 + minor;
}

/// A GL object name created in the current context, to tell contexts
/// apart: every context starts its texture names at 1, so a texture made
/// in one is unknown in another (no sharing).
bool owns_texture(const gl_device& d, GLuint name) {
  GLint bound = 0;
  d.gl().BindTexture(GL_TEXTURE_2D, name);
  const bool ok = d.gl().GetError() == GL_NO_ERROR;
  d.gl().BindTexture(GL_TEXTURE_2D, 0);
  (void)bound;
  return ok;
}

TEST(gl_device, MissingLibraryIsAReasonNotAnError) {
  gl_device_options o;
  o.library_names = {kMissing};
  const gl_device::result r = gl_device::create(o);
  EXPECT_EQ(r.device, nullptr);
  EXPECT_NE(r.reason.find(kMissing), std::string::npos) << r.reason;
}

TEST(gl_device, SoftwareRendererNamesAreRecognised) {
  EXPECT_TRUE(is_software_renderer("llvmpipe (LLVM 19.1.1, 256 bits)"));
  EXPECT_TRUE(is_software_renderer("GDI Generic"));
  EXPECT_TRUE(is_software_renderer("Google SwiftShader"));
  EXPECT_TRUE(is_software_renderer("D3D12 (Microsoft Basic Render Driver)"));
  EXPECT_FALSE(is_software_renderer("NVIDIA GeForce RTX 4070/PCIe/SSE2"));
  EXPECT_FALSE(is_software_renderer("AMD Radeon Graphics (radeonsi, gfx1103_r1, LLVM 17.0.6, DRM 3.57)"));
}

TEST(gl_device, CreatesAHeadless33CoreContext) {
  NYMPH_REQUIRE_GL();
  gl_device& d = *test_device();
  EXPECT_FALSE(d.renderer_name().empty());
  EXPECT_GE(d.max_size(), 1024);
  std::printf("OpenGL device: %s (max size %d)\n", d.renderer_name().c_str(), d.max_size());
  {
    const gl_device::current_scope scope(d);
    EXPECT_GE(context_version(d), 33);
    const char* version = reinterpret_cast<const char*>(d.gl().GetString(GL_VERSION));
    ASSERT_NE(version, nullptr);
  }
}

TEST(gl_device, PreviousContextIsCurrentAgainAfterTheScope) {
  NYMPH_REQUIRE_GL();
  gl_device& outer = *test_device();
  gl_device_options o;
  o.allow_software_gl = true;
  const gl_device::result inner = gl_device::create(o);
  ASSERT_NE(inner.device, nullptr) << inner.reason;

  const gl_device::current_scope outer_scope(outer);
  GLuint marker = 0;
  outer.gl().GenTextures(1, &marker);
  outer.gl().BindTexture(GL_TEXTURE_2D, marker); // the name now exists in `outer`
  outer.gl().BindTexture(GL_TEXTURE_2D, 0);
  {
    const gl_device::current_scope inner_scope(*inner.device);
    GLuint inner_marker = 0;
    inner.device->gl().GenTextures(1, &inner_marker);
    inner.device->gl().DeleteTextures(1, &inner_marker);
  }
  // Back in `outer`: its texture name is still valid and deletable.
  EXPECT_TRUE(owns_texture(outer, marker));
  outer.gl().DeleteTextures(1, &marker);

  // Also when the scope's body throws.
  try {
    const gl_device::current_scope inner_scope(*inner.device);
    throw std::runtime_error("boom");
  } catch (const std::runtime_error&) {
  }
  GLuint after = 0;
  outer.gl().GenTextures(1, &after);
  EXPECT_TRUE(owns_texture(outer, after));
  outer.gl().DeleteTextures(1, &after);
}

TEST(gl_device, CreatingAndDestroyingRepeatedlyWorks) {
  NYMPH_REQUIRE_GL();
  gl_device_options o;
  o.allow_software_gl = true;
  for (int i = 0; i < 20; ++i) {
    const gl_device::result r = gl_device::create(o);
    ASSERT_NE(r.device, nullptr) << "attempt " << i << ": " << r.reason;
    const gl_device::current_scope scope(*r.device);
    EXPECT_GE(context_version(*r.device), 33);
  }
}

TEST(gl_device, SoftwareOpenGLIsRefusedByDefault) {
  NYMPH_REQUIRE_GL();
  if (!is_software_renderer(test_device()->renderer_name())) {
    GTEST_SKIP() << "the test device is a GPU: " << test_device()->renderer_name();
  }
  const gl_device::result r = gl_device::create({});
  EXPECT_EQ(r.device, nullptr);
  EXPECT_NE(r.reason.find("software OpenGL"), std::string::npos) << r.reason;
}

} // namespace
} // namespace bdg::nymph
