// MIT License © 2026 Binary Dice Games
/// @file test_gl_loader.cpp
/// @brief spec/02_opengl_rasterizer plan step 2: shared_library and the
///        gl_api function table, with no OpenGL needed.
#include <gtest/gtest.h>
#include <cstring>
#include <map>
#include <string>
#include <vector>
#include "render/gl/gl_api.hpp"
#include "render/gl/shared_library.hpp"

namespace bdg::nymph {
namespace {

#if defined(_WIN32)
const char* const kMissing = "nymph-no-such-library.dll";
const char* const kRuntime = "msvcrt.dll";
#else
const char* const kMissing = "nymph-no-such-library.so";
const char* const kRuntime = "libc.so.6";
#endif

// ── shared_library ───────────────────────────────────────────────────────────

TEST(shared_library, MissingLibraryFailsWithItsName) {
  const std::vector<std::string> names{kMissing};
  std::string error;
  const shared_library lib = shared_library::open(names, error);
  EXPECT_FALSE(lib.loaded());
  EXPECT_NE(error.find(kMissing), std::string::npos) << error;
  EXPECT_EQ(lib.symbol("strlen"), nullptr);
}

TEST(shared_library, OpensTheFirstNameThatLoads) {
  const std::vector<std::string> names{kMissing, kRuntime};
  std::string error;
  shared_library lib = shared_library::open(names, error);
  ASSERT_TRUE(lib.loaded()) << error;
  EXPECT_EQ(lib.name(), kRuntime);
  EXPECT_NE(lib.symbol("strlen"), nullptr);
  EXPECT_EQ(lib.symbol("nymph_no_such_symbol"), nullptr);

  shared_library moved = std::move(lib);
  EXPECT_FALSE(lib.loaded());
  EXPECT_TRUE(moved.loaded());
  EXPECT_NE(moved.symbol("strlen"), nullptr);
}

TEST(shared_library, NoNamesIsAnError) {
  std::string error;
  EXPECT_FALSE(shared_library::open({}, error).loaded());
  EXPECT_FALSE(error.empty());
}

// ── gl_api ───────────────────────────────────────────────────────────────────

void dummy_function() {}

/// Answers every name except those in `missing`.
struct fake_lookup {
  std::vector<std::string> missing;
  std::vector<std::string> asked;
  static void* lookup(const char* name, void* user) {
    auto* self = static_cast<fake_lookup*>(user);
    self->asked.emplace_back(name);
    for (const std::string& m : self->missing) {
      if (m == name) {
        return nullptr;
      }
    }
    void (*fn)() = &dummy_function;
    void* address = nullptr;
    std::memcpy(&address, &fn, sizeof(address));
    return address;
  }
};

TEST(gl_api, LoadFillsEveryPointer) {
  fake_lookup fake;
  gl_api gl;
  EXPECT_EQ(gl.load(&fake_lookup::lookup, &fake), "");
  size_t count = 0;
  const char* const* names = gl_api::names(count);
  ASSERT_EQ(fake.asked.size(), count);
  for (size_t i = 0; i < count; ++i) {
    EXPECT_EQ(fake.asked[i], names[i]);
    EXPECT_EQ(std::strncmp(names[i], "gl", 2), 0);
  }
  EXPECT_NE(gl.ReadPixels, nullptr);
  EXPECT_NE(gl.DrawElementsBaseVertex, nullptr);
}

TEST(gl_api, LoadNamesTheFirstMissingFunction) {
  fake_lookup fake;
  fake.missing = {"glReadPixels", "glDrawElementsBaseVertex"};
  gl_api gl;
  EXPECT_EQ(gl.load(&fake_lookup::lookup, &fake), "glReadPixels");
  EXPECT_EQ(gl.ReadPixels, nullptr);
  EXPECT_NE(gl.GetString, nullptr);
}

} // namespace
} // namespace bdg::nymph
