// MIT License © 2026 Binary Dice Games
/// @file test_docs.cpp
/// @brief Plan step 14: the documentation matches the program, and every
///        example renders and round-trips.
#include <gtest/gtest.h>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>
#include "cli/cli.hpp"
#include "nymph.hpp"

namespace bdg::nymph {
namespace {

namespace fs = std::filesystem;

const fs::path kSourceDir(NYMPH_SOURCE_DIR);

std::string read_file(const fs::path& p) {
  std::ifstream in(p, std::ios::binary);
  std::stringstream ss;
  ss << in.rdbuf();
  return ss.str();
}

std::vector<fs::path> example_sources() {
  std::vector<fs::path> out;
  for (const fs::directory_entry& e : fs::directory_iterator(kSourceDir / "examples")) {
    if (e.path().extension() == ".yaml") {
      out.push_back(e.path());
    }
  }
  std::sort(out.begin(), out.end());
  return out;
}

// ── docs/elements.md ─────────────────────────────────────────────────────────

TEST(docs, ElementsMdIsTheSchemaMarkdown) {
  const std::vector<std::string_view> args{"schema", "--markdown"};
  std::istringstream in;
  std::ostringstream out;
  std::ostringstream err;
  ASSERT_EQ(run_cli(args, in, out, err), 0) << err.str();
  EXPECT_EQ(read_file(kSourceDir / "docs" / "elements.md"), out.str())
      << "regenerate with: nymph schema --markdown > docs/elements.md";
}

// ── examples/ ────────────────────────────────────────────────────────────────

TEST(docs, ExamplesRenderAndRoundTrip) {
  const std::vector<fs::path> sources = example_sources();
  ASSERT_GE(sources.size(), 8u);
  for (const fs::path& path : sources) {
    SCOPED_TRACE(path.filename().string());
    const std::string source = read_file(path);
    const std::vector<uint8_t> png = render_png(source);
    ASSERT_FALSE(png.empty());
    EXPECT_EQ(extract_source(png).value_or(""), source);
    EXPECT_EQ(render_png(source), png); // deterministic
  }
}

TEST(docs, ExamplesMdQuotesEveryExampleVerbatim) {
  const std::string page = read_file(kSourceDir / "docs" / "examples.md");
  for (const fs::path& path : example_sources()) {
    SCOPED_TRACE(path.filename().string());
    const std::string name = path.stem().string();
    EXPECT_NE(page.find("```yaml\n" + read_file(path) + "```\n"), std::string::npos);
    EXPECT_NE(page.find("](../examples/" + name + ".png)"), std::string::npos);
    // The committed image carries the current source (pixels may differ by
    // compiler, so only the source is compared).
    const std::string png = read_file(kSourceDir / "examples" / (name + ".png"));
    const std::vector<uint8_t> bytes(png.begin(), png.end());
    EXPECT_EQ(extract_source(bytes).value_or(""), read_file(path))
        << "re-render with: nymph examples/" << name << ".yaml";
  }
}

} // namespace
} // namespace bdg::nymph
