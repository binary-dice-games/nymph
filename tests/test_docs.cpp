// MIT License © 2026 Binary Dice Games
/// @file test_docs.cpp
/// @brief Plan step 14: the documentation matches the program, and every
///        example renders and round-trips.
#include <gtest/gtest.h>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>
#include "cli/cli.hpp"
#include "cli/schema_doc.hpp"
#include "elements/register_builtin.hpp"
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

// The committed pages are what `nymph schema --markdown --paged docs` writes
// (every build of nymph-cli refreshes them: NYMPH_UPDATE_DOCS).
TEST(docs, ElementsPagesAreTheSchemaMarkdown) {
  const std::vector<doc_page> pages = schema_markdown_pages(builtin_registry());
  ASSERT_GT(pages.size(), 10u);
  EXPECT_EQ(pages[0].path, "elements.md");
  std::set<fs::path> expected;
  for (const doc_page& page : pages) {
    SCOPED_TRACE(page.path);
    expected.insert(fs::path(page.path).lexically_normal());
    EXPECT_EQ(read_file(kSourceDir / "docs" / page.path), page.text)
        << "regenerate with: nymph schema --markdown --paged docs";
  }
  for (const fs::directory_entry& e : fs::directory_iterator(kSourceDir / "docs" / "elements")) {
    EXPECT_EQ(expected.count(fs::path("elements") / e.path().filename()), 1u)
        << e.path() << " is not generated: regenerate with nymph schema --markdown --paged docs";
  }
}

TEST(docs, ElementsPagesCoverEveryClassAndStaySmall) {
  const registry& reg = builtin_registry();
  const std::vector<doc_page> pages = schema_markdown_pages(reg);
  for (const element_class* c : reg.classes()) {
    SCOPED_TRACE(c->name());
    const std::string heading = "\n## " + std::string(c->name()) + "\n";
    bool found = false;
    for (const doc_page& page : pages) {
      found = found || ("\n" + page.text).find(heading) != std::string::npos;
    }
    EXPECT_TRUE(found);
  }
  // The point of the pages: a reader loads the root and one page, which must
  // be smaller than the whole reference (the widget catalog, on Window's
  // page, is the largest, about half of it).
  const size_t whole = schema_markdown(reg).size();
  for (const doc_page& page : pages) {
    SCOPED_TRACE(page.path);
    EXPECT_LT(page.text.size(), whole * 2 / 3);
  }
}

TEST(docs, ElementsRootLinksEveryPage) {
  const std::vector<doc_page> pages = schema_markdown_pages(builtin_registry());
  for (size_t i = 1; i < pages.size(); ++i) {
    SCOPED_TRACE(pages[i].path);
    EXPECT_NE(pages[0].text.find("](" + pages[i].path + ")"), std::string::npos);
    EXPECT_NE(pages[i].text.find("](../elements.md)"), std::string::npos);
  }
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
