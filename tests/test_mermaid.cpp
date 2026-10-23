// MIT License © 2026 Binary Dice Games
/// @file test_mermaid.cpp
/// @brief The mermaid import (spec/16_mermaid_import/PLAN.md steps 1, 2,
///        7-12): detection, positions, and golden files.
///
/// Every `tests/data/mermaid/<kind>/<case>.mmd` is converted and compared
/// with `<case>.yaml` (the exact source; the image size within a few pixels,
/// as layout floats may differ between compilers) and `<case>.warnings`
/// (one `line:column: message` per line, empty or missing for none), or,
/// when `<case>.error` exists, the import must fail with that
/// `line:column: message`. NYMPH_UPDATE_GOLDEN=1 rewrites the expected files.
#include <gtest/gtest.h>
#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <regex>
#include <sstream>
#include "core/error.hpp"
#include "nymph.hpp"

namespace {

namespace fs = std::filesystem;
using namespace bdg::nymph;

const fs::path kData = fs::path(NYMPH_SOURCE_DIR) / "tests" / "data" / "mermaid";

std::string read_file(const fs::path& p) {
  std::ifstream in(p, std::ios::binary);
  std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  text.erase(std::remove(text.begin(), text.end(), '\r'), text.end()); // checked out with CRLF
  return text;
}

void write_file(const fs::path& p, const std::string& text) {
  std::ofstream(p, std::ios::binary) << text;
}

bool updating() {
  const char* v = std::getenv("NYMPH_UPDATE_GOLDEN");
  return v != nullptr && std::string(v) == "1";
}

std::string warnings_text(const std::vector<import_warning>& warnings) {
  std::string text;
  for (const import_warning& w : warnings) {
    text += std::to_string(w.line) + ":" + std::to_string(w.column) + ": " + w.message + "\n";
  }
  return text;
}

const std::regex kImage(R"(image: \{width: (\d+), height: (\d+)\})");

/// The source with its image size taken out, and the size.
std::string without_size(const std::string& source, int& width, int& height) {
  std::smatch m;
  width = height = 0;
  if (std::regex_search(source, m, kImage)) {
    width = std::stoi(m[1]);
    height = std::stoi(m[2]);
  }
  return std::regex_replace(source, kImage, "image: {width: W, height: H}");
}

std::vector<fs::path> cases() {
  std::vector<fs::path> found;
  for (const auto& entry : fs::recursive_directory_iterator(kData)) {
    if (entry.path().extension() == ".mmd") {
      found.push_back(entry.path());
    }
  }
  std::sort(found.begin(), found.end());
  return found;
}

// ── Golden files ─────────────────────────────────────────────────────────────

TEST(mermaid_golden, every_case_converts_as_recorded) {
  const std::vector<fs::path> all = cases();
  ASSERT_GE(all.size(), 20u) << "the mermaid corpus is missing: " << kData;
  for (const fs::path& mmd : all) {
    SCOPED_TRACE(mmd.string());
    const fs::path yaml = fs::path(mmd).replace_extension(".yaml");
    const fs::path warn = fs::path(mmd).replace_extension(".warnings");
    const fs::path err = fs::path(mmd).replace_extension(".error");
    const std::string text = read_file(mmd);
    if (fs::exists(err)) {
      try {
        (void)import_mermaid(text);
        ADD_FAILURE() << "converted, but " << err.filename() << " expects an error";
      } catch (const error& e) {
        const std::string got = std::to_string(e.line()) + ":" + std::to_string(e.column()) + ": " + e.what() + "\n";
        if (updating()) {
          write_file(err, got);
        } else {
          EXPECT_EQ(got, read_file(err));
        }
      }
      continue;
    }
    EXPECT_TRUE(is_mermaid(text));
    mermaid_import m;
    try {
      m = import_mermaid(text);
    } catch (const error& e) {
      ADD_FAILURE() << e.line() << ":" << e.column() << ": " << e.what();
      continue;
    }
    if (updating()) {
      write_file(yaml, m.source);
      if (m.warnings.empty()) {
        fs::remove(warn);
      } else {
        write_file(warn, warnings_text(m.warnings));
      }
      continue;
    }
    ASSERT_TRUE(fs::exists(yaml)) << "no " << yaml.filename() << " (run with NYMPH_UPDATE_GOLDEN=1)";
    int w = 0, h = 0, ew = 0, eh = 0;
    EXPECT_EQ(without_size(m.source, w, h), without_size(read_file(yaml), ew, eh));
    EXPECT_NEAR(w, ew, 4);
    EXPECT_NEAR(h, eh, 4);
    EXPECT_EQ(warnings_text(m.warnings), fs::exists(warn) ? read_file(warn) : std::string());
    // The result is a valid source and converting again gives the same bytes.
    EXPECT_NO_THROW(check_source(m.source));
    EXPECT_EQ(import_mermaid(text).source, m.source);
  }
}

TEST(mermaid_golden, every_supported_kind_has_cases) {
  for (const char* kind : {"reader", "pie", "flowchart", "sequence", "class", "state", "er", "gantt", "mindmap",
                           "sankey", "xychart"}) {
    EXPECT_TRUE(fs::is_directory(kData / kind)) << kind;
  }
}

// ── Detection ────────────────────────────────────────────────────────────────

TEST(mermaid_detect, every_kind_keyword_is_mermaid) {
  for (const char* k : {"flowchart TD", "graph LR", "sequenceDiagram", "classDiagram", "classDiagram-v2",
                        "stateDiagram", "stateDiagram-v2", "erDiagram", "gantt", "pie", "mindmap", "sankey-beta",
                        "xychart-beta", "journey", "timeline", "quadrantChart", "gitGraph", "C4Context",
                        "requirementDiagram", "block-beta", "architecture-beta", "packet-beta", "kanban",
                        "radar-beta", "treemap-beta", "zenuml"}) {
    EXPECT_TRUE(is_mermaid(std::string(k) + "\n  A")) << k;
  }
}

TEST(mermaid_detect, wrappers_before_the_keyword) {
  EXPECT_TRUE(is_mermaid("\n\n  flowchart LR\nA-->B\n"));
  EXPECT_TRUE(is_mermaid("%% a comment\nflowchart LR\n"));
  EXPECT_TRUE(is_mermaid("%%{init: {'theme': 'dark'}}%%\nsequenceDiagram\n"));
  EXPECT_TRUE(is_mermaid("---\ntitle: Hi\n---\npie\n\"a\": 1\n"));
  EXPECT_TRUE(is_mermaid("```mermaid\ngraph TD\nA-->B\n```\n"));
  EXPECT_TRUE(is_mermaid("flowchart LR\r\nA-->B\r\n"));
  EXPECT_TRUE(is_mermaid("\xEF\xBB\xBFgantt\n"));
}

TEST(mermaid_detect, nymph_sources_and_other_text_are_not_mermaid) {
  EXPECT_FALSE(is_mermaid(""));
  EXPECT_FALSE(is_mermaid("graph: 1\n"));
  EXPECT_FALSE(is_mermaid("pie:\n  a: 1\n"));
  EXPECT_FALSE(is_mermaid("---\nroot: {type: Label, text: hi}\n"));
  EXPECT_FALSE(is_mermaid("```yaml\nroot: {}\n```\n"));
  EXPECT_FALSE(is_mermaid("flowcharts are nice"));
  for (const auto& entry : fs::directory_iterator(fs::path(NYMPH_SOURCE_DIR) / "examples")) {
    if (entry.path().extension() == ".yaml") {
      EXPECT_FALSE(is_mermaid(read_file(entry.path()))) << entry.path();
    }
  }
}

// ── Positions and errors ─────────────────────────────────────────────────────

TEST(mermaid_errors, positions_count_characters_and_lines_of_the_whole_text) {
  // Line 4 (after a fence and a comment); column 9 counts the two-byte é once.
  try {
    (void)import_mermaid("```mermaid\n%% c\nflowchart LR\n  é --> F@{shape: blob}\n```\n");
    FAIL() << "expected an error";
  } catch (const error& e) {
    EXPECT_EQ(e.line(), 4);
    EXPECT_EQ(e.column(), 10); // the '@' of @{
    EXPECT_NE(std::string(e.what()).find("unknown mermaid shape 'blob'"), std::string::npos);
  }
}

TEST(mermaid_errors, problems_found_by_nymph_point_at_the_mermaid_line) {
  // A sankey cycle is found only when nymph checks the converted source;
  // it names the diagram, written from the mermaid header (line 2 here).
  try {
    (void)import_mermaid("%% flows\nsankey-beta\na,b,1\nb,a,1\n");
    FAIL() << "expected an error";
  } catch (const error& e) {
    EXPECT_EQ(e.line(), 2);
    EXPECT_NE(std::string(e.what()).find("(nymph YAML line"), std::string::npos) << e.what();
  }
}

TEST(mermaid_errors, unsupported_kinds_list_the_supported_ones) {
  try {
    (void)import_mermaid("gitGraph\n  commit\n");
    FAIL() << "expected an error";
  } catch (const error& e) {
    EXPECT_EQ(e.line(), 1);
    EXPECT_EQ(e.column(), 1);
    EXPECT_NE(std::string(e.what()).find("flowchart"), std::string::npos);
  }
}

TEST(mermaid_errors, subgraphs_nested_too_deep_fail_before_recursing) {
  std::string text = "flowchart TD\n";
  for (int i = 0; i < 9; ++i) {
    text += "subgraph g" + std::to_string(i) + "\n";
  }
  text += "A\n";
  for (int i = 0; i < 9; ++i) {
    text += "end\n";
  }
  try {
    (void)import_mermaid(text);
    FAIL() << "expected an error";
  } catch (const error& e) {
    EXPECT_EQ(e.line(), 10);
  }
}

TEST(mermaid_errors, the_node_limit_is_reported_at_a_mermaid_line) {
  std::string text = "flowchart LR\n";
  for (int i = 0; i < 600; ++i) {
    text += "n" + std::to_string(i) + " --> n" + std::to_string(i + 1) + "\n";
  }
  try {
    (void)import_mermaid(text);
    FAIL() << "expected an error";
  } catch (const error& e) {
    EXPECT_EQ(e.line(), 501); // where node 501 is first named
    EXPECT_NE(std::string(e.what()).find("more than 500 nodes"), std::string::npos) << e.what();
  }
}

// ── Output ───────────────────────────────────────────────────────────────────

TEST(mermaid_output, the_image_fits_small_and_large_diagrams) {
  for (int n : {3, 120}) {
    std::string text = "flowchart TD\n";
    for (int i = 0; i < n; ++i) {
      text += "r --> c" + std::to_string(i) + "\n";
    }
    const mermaid_import m = import_mermaid(text);
    EXPECT_NO_THROW(render_png(m.source)) << n;
  }
}

TEST(mermaid_output, a_leading_dollar_stays_text) {
  const mermaid_import m = import_mermaid("flowchart LR\nA[$5 per unit] --> B\n");
  EXPECT_NE(m.source.find("text: $$5 per unit"), std::string::npos) << m.source;
}

} // namespace
