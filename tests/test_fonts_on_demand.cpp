// MIT License © 2026 Binary Dice Games
/// @file test_fonts_on_demand.cpp
/// @brief spec/04_web_viewer plan step 2: the font catalog, fonts
///        registered at run time, and the fonts a figure uses.
#include <gtest/gtest.h>
#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>
#include "core/error.hpp"
#include "core/sha256.hpp"
#include "document/bind.hpp"
#include "elements/register_builtin.hpp"
#include "fonts/embedded_fonts.hpp"
#include "fonts/font_testing.hpp"
#include "fonts/fonts_used.hpp"
#include "nymph.hpp"
#include "png/png_meta.hpp"
#include "raster_compare.hpp"

// Decodes pixels with its own copy of stb_image (nymph keeps its copy
// internal).
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_STATIC
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#include <stb_image.h>

namespace bdg::nymph {
namespace {

namespace fs = std::filesystem;

const fs::path kSourceDir(NYMPH_SOURCE_DIR);
const fs::path kFontsDir(NYMPH_FONTS_DIR);

/// The tests below name fonts of nymph's own `fonts/` folder.
#define NYMPH_REQUIRE_STANDARD_FONTS()                                                                 \
  if (find_font("FiraSans-Regular.ttf") == nullptr || find_font("PT_Serif-Web-Bold.ttf") == nullptr) { \
    GTEST_SKIP() << "built with another NYMPH_FONTS_DIR";                                              \
  }

std::vector<uint8_t> read_bytes(const fs::path& p) {
  std::ifstream in(p, std::ios::binary);
  return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

/// Decodes a PNG's pixels as RGBA.
image decode(const std::vector<uint8_t>& png) {
  int w = 0;
  int h = 0;
  int n = 0;
  stbi_uc* data = stbi_load_from_memory(png.data(), static_cast<int>(png.size()), &w, &h, &n, 4);
  image img;
  if (data != nullptr) {
    img.width = w;
    img.height = h;
    img.rgba.assign(data, data + static_cast<size_t>(w) * h * 4);
    stbi_image_free(data);
  }
  return img;
}

/// `png` shows the committed example picture: the same embedded source and
/// the same pixels within the rasterizer tolerance. The committed PNGs are
/// drawn with one toolchain; another's floating point rounds a texel
/// differently, so the bytes are not compared.
void expect_committed_picture(const std::vector<uint8_t>& mine, const fs::path& committed, const std::string& label) {
  const std::vector<uint8_t> theirs = read_bytes(committed);
  EXPECT_EQ(extract_source(mine).value_or("a"), extract_source(theirs).value_or("b")) << label;
  expect_close(decode(theirs), decode(mine), label);
}

std::string read_text(const fs::path& p) {
  const std::vector<uint8_t> b = read_bytes(p);
  return {b.begin(), b.end()};
}

/// Makes the build behave like `NYMPH_EMBED_FONTS=none` for one test.
class no_embedded_fonts : public ::testing::Test {
 protected:
  void SetUp() override {
    hide_embedded_fonts_for_testing(true);
  }
  void TearDown() override {
    hide_embedded_fonts_for_testing(false);
  }
};

// ── SHA-256 ──────────────────────────────────────────────────────────────────

TEST(sha256, MatchesTheStandardTestVectors) {
  const auto hex = [](const std::string& s) {
    return sha256_hex({reinterpret_cast<const uint8_t*>(s.data()), s.size()});
  };
  EXPECT_EQ(hex(""), "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
  EXPECT_EQ(hex("abc"), "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
  // 56 bytes: the padding needs a second block.
  EXPECT_EQ(
      hex("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"),
      "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
  EXPECT_EQ(hex(std::string(1000, 'a')), "41edece42d63e8d9bf515a9ba6932e1c20cbc9f5a5d134645adb5db1b9737ea3");
}

// ── Catalog ──────────────────────────────────────────────────────────────────

TEST(font_catalog, ListsEveryFontFileWithItsSizeAndHash) {
  std::vector<std::string> files;
  for (const fs::directory_entry& e : fs::directory_iterator(kFontsDir)) {
    const std::string ext = e.path().extension().string();
    if (ext == ".ttf" || ext == ".otf") {
      files.push_back(e.path().filename().string());
    }
  }
  std::sort(files.begin(), files.end());
  std::vector<std::string> names;
  for (const font_entry& f : font_catalog()) {
    SCOPED_TRACE(f.name);
    names.emplace_back(f.name);
    const std::vector<uint8_t> bytes = read_bytes(kFontsDir / f.name);
    EXPECT_EQ(f.size, bytes.size());
    EXPECT_EQ(std::string(f.sha256), sha256_hex(bytes));
    ASSERT_NE(f.data, nullptr); // the default build embeds every font
    EXPECT_TRUE(std::equal(bytes.begin(), bytes.end(), f.data));
    EXPECT_EQ(find_font(f.name), &f);
    EXPECT_EQ(font_data(f.name).size(), f.size);
  }
  EXPECT_EQ(names, files);
  EXPECT_EQ(find_font("nope.ttf"), nullptr);
  EXPECT_TRUE(font_data("nope.ttf").empty());
}

TEST(font_catalog, ItalicSiblingNamesOnlyCatalogFonts) {
  NYMPH_REQUIRE_STANDARD_FONTS();
  EXPECT_EQ(italic_sibling_name("FiraSans-Regular.ttf"), "FiraSans-Italic.ttf");
  EXPECT_EQ(italic_sibling_name("FiraSans-Bold.ttf"), "FiraSans-Italic.ttf");
  EXPECT_EQ(italic_sibling_name("FiraMono-Regular.ttf"), ""); // no FiraMono-Italic.ttf
  EXPECT_EQ(italic_sibling_name("default"), "");
}

// ── Registration ─────────────────────────────────────────────────────────────

TEST_F(no_embedded_fonts, RegisterFontChecksNameSizeAndHash) {
  NYMPH_REQUIRE_STANDARD_FONTS();
  const std::string name = "FiraSans-Bold.ttf";
  std::vector<uint8_t> bytes = read_bytes(kFontsDir / name);
  EXPECT_TRUE(font_data(name).empty());

  EXPECT_THROW(register_font("Nope-Regular.ttf", bytes), std::invalid_argument);
  std::vector<uint8_t> shorter(bytes.begin(), bytes.end() - 1);
  EXPECT_THROW(register_font(name, shorter), std::invalid_argument);
  std::vector<uint8_t> changed = bytes;
  changed[changed.size() / 2] ^= 0x01;
  EXPECT_THROW(register_font(name, changed), std::invalid_argument);
  EXPECT_TRUE(font_data(name).empty());

  register_font(name, bytes);
  ASSERT_EQ(font_data(name).size(), bytes.size());
  EXPECT_TRUE(std::equal(bytes.begin(), bytes.end(), font_data(name).begin()));
  // The bytes were copied: changing the caller's buffer changes nothing.
  bytes[0] ^= 0xFF;
  EXPECT_NE(font_data(name)[0], bytes[0]);
  // Registering again is harmless.
  bytes[0] ^= 0xFF;
  EXPECT_NO_THROW(register_font(name, bytes));
}

// ── Fonts a figure uses ──────────────────────────────────────────────────────

TEST(fonts_used, ListsImageAndElementFontsSortedOnce) {
  NYMPH_REQUIRE_STANDARD_FONTS();
  const figure f = bind_source(read_text(kSourceDir / "examples/fonts.yaml"), builtin_registry());
  const std::vector<std::string> used = fonts_used(f);
  EXPECT_TRUE(std::is_sorted(used.begin(), used.end()));
  for (const char* name :
       {"FiraSans-Regular.ttf", "PT_Serif-Web-Bold.ttf", "FiraSans-Italic.ttf", "FiraSans-Bold.ttf"}) {
    EXPECT_NE(std::find(used.begin(), used.end(), name), used.end()) << name;
  }
  EXPECT_EQ(std::adjacent_find(used.begin(), used.end()), used.end()); // unique
  EXPECT_TRUE(fonts_missing(f).empty());
}

TEST(fonts_used, DefaultFontOnlyUsesNoCatalogFont) {
  const figure f = bind_source("root: {type: Label, text: hi}\n", builtin_registry());
  EXPECT_TRUE(fonts_used(f).empty());
}

TEST(fonts_used, DiagramsImplyTheItalicSiblingOfTheirFont) {
  NYMPH_REQUIRE_STANDARD_FONTS();
  const figure f = bind_source(
      "image: {font: FiraSans-Regular.ttf}\n"
      "root:\n"
      "  type: ClassDiagram\n"
      "  children:\n"
      "    - {type: Class, id: A}\n",
      builtin_registry());
  EXPECT_EQ(fonts_used(f), (std::vector<std::string>{"FiraSans-Italic.ttf", "FiraSans-Regular.ttf"}));
  // A label in the same font implies nothing.
  const figure label =
      bind_source("image: {font: FiraSans-Regular.ttf}\nroot: {type: Label, text: hi}\n", builtin_registry());
  EXPECT_EQ(fonts_used(label), (std::vector<std::string>{"FiraSans-Regular.ttf"}));
}

// ── Rendering without embedded fonts ─────────────────────────────────────────

TEST_F(no_embedded_fonts, ANotEmbeddedFontBindsButDoesNotRenderUntilRegistered) {
  NYMPH_REQUIRE_STANDARD_FONTS();
  const std::string source = read_text(kSourceDir / "examples/fonts.yaml");
  const figure f = bind_source(source, builtin_registry()); // the name is valid
  EXPECT_FALSE(fonts_missing(f).empty());
  try {
    (void)render_png(source);
    FAIL() << "expected nymph::error";
  } catch (const error& e) {
    EXPECT_EQ(e.line(), 5); // image.font, the first font written
    EXPECT_NE(std::string(e.what()).find("FiraSans-Regular.ttf"), std::string::npos) << e.what();
    EXPECT_NE(std::string(e.what()).find("register_font"), std::string::npos) << e.what();
  }
  EXPECT_THROW(check_source(source), error);

  for (const std::string& name : fonts_missing(f)) {
    register_font(name, read_bytes(kFontsDir / name));
  }
  EXPECT_TRUE(fonts_missing(f).empty());
  // Registered fonts draw exactly as embedded ones.
  expect_committed_picture(render_png(source), kSourceDir / "examples/fonts.png", "fonts");
}

TEST_F(no_embedded_fonts, AnElementFontErrorPointsAtItsField) {
  NYMPH_REQUIRE_STANDARD_FONTS();
  try {
    (void)render_png("root:\n  type: Label\n  text: hi\n  font: FiraMono-Bold.ttf\n");
    FAIL() << "expected nymph::error";
  } catch (const error& e) {
    EXPECT_EQ(e.line(), 4);
    EXPECT_EQ(e.column(), 9);
  }
}

TEST_F(no_embedded_fonts, TheBuiltInFontNeedsNothing) {
  const std::string source = read_text(kSourceDir / "examples/line.yaml");
  expect_committed_picture(render_png(source), kSourceDir / "examples/line.png", "line");
}

TEST_F(no_embedded_fonts, AMissingImpliedFontIsNotAnError) {
  NYMPH_REQUIRE_STANDARD_FONTS();
  // PT Serif: no other test registers it (they may share a process).
  register_font("PT_Serif-Web-Regular.ttf", read_bytes(kFontsDir / "PT_Serif-Web-Regular.ttf"));
  const std::string source =
      "image: {font: PT_Serif-Web-Regular.ttf}\nroot:\n  type: ClassDiagram\n  children:\n    - {type: Class, id: A}\n";
  const figure f = bind_source(source, builtin_registry());
  EXPECT_EQ(fonts_missing(f), (std::vector<std::string>{"PT_Serif-Web-Italic.ttf"}));
  EXPECT_NO_THROW((void)render_png(source)); // falls back to the regular font
}

} // namespace
} // namespace bdg::nymph
