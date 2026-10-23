// MIT License © 2026 Binary Dice Games
/// @file test_web_templates.cpp
/// @brief spec/15_web_app plan step 10: every template of the web app's New
///        chart (web/app/templates/) loads and renders, and is listed.
#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include "nymph.hpp"

namespace bdg::nymph {
namespace {

std::string read_text(const std::filesystem::path& p) {
  std::ifstream in(p, std::ios::binary);
  std::stringstream ss;
  ss << in.rdbuf();
  return ss.str();
}

TEST(WebTemplates, EveryTemplateRendersAndIsListed) {
  const std::filesystem::path dir = std::filesystem::path(NYMPH_SOURCE_DIR) / "web" / "app" / "templates";
  const std::string index = read_text(dir / "index.json");
  int count = 0;
  for (const auto& entry : std::filesystem::directory_iterator(dir)) {
    if (entry.path().extension() != ".yaml") {
      continue;
    }
    ++count;
    const std::string name = entry.path().stem().string();
    SCOPED_TRACE(name);
    EXPECT_NE(index.find("\"" + name + "\""), std::string::npos) << "not in index.json";
    EXPECT_NO_THROW((void)render_png(read_text(entry.path())));
    EXPECT_TRUE(std::filesystem::exists(dir / (name + ".png"))) << "no thumbnail";
  }
  EXPECT_GE(count, 8);
}

} // namespace
} // namespace bdg::nymph
