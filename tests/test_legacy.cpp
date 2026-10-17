// MIT License © 2026 Binary Dice Games
/// @file test_legacy.cpp
/// @brief Plan step 13: wish nymph sources (description, `--`, format YAML,
///        `--`, CSV) are read back from their PNGs as nymph YAML.
#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include "document/bind.hpp"
#include "document/legacy.hpp"
#include "document/table.hpp"
#include "document/yaml_doc.hpp"
#include "elements/register_builtin.hpp"
#include "nymph.hpp"
#include "png/png_meta.hpp"
#include "render/figure_renderer.hpp"
#include "schema/element_class.hpp"

namespace bdg::nymph {
namespace {

namespace fs = std::filesystem;

const fs::path kLegacyDir = fs::path(NYMPH_SOURCE_DIR) / "tests" / "data" / "legacy";

std::string read_file(const fs::path& p) {
  std::ifstream in(p, std::ios::binary);
  std::stringstream ss;
  ss << in.rdbuf();
  return ss.str();
}

/// A PNG carrying `source`, as wish nymph writes it (same chunk layout).
std::vector<uint8_t> wish_png(const std::string& source) {
  image img;
  img.width = 4;
  img.height = 4;
  img.rgba.assign(4 * 4 * 4, 255);
  return encode_png(img, source, "");
}

/// The three parts of a wish source, split by hand for comparison.
struct wish_parts {
  std::string description;
  std::string format;
  std::string data;
};

wish_parts split_by_hand(const std::string& text) {
  const size_t a = text.find("\n--\n");
  const size_t b = text.find("\n--\n", a + 4);
  return {text.substr(0, a), text.substr(a + 4, b - a - 3), text.substr(b + 4)};
}

/// Structural equality of two YAML trees (kinds, scalar texts, keys, order).
bool same_tree(const yaml_node& a, const yaml_node& b) {
  if (a.kind != b.kind || a.text != b.text || a.keys.size() != b.keys.size() ||
      a.children.size() != b.children.size()) {
    return false;
  }
  for (size_t i = 0; i < a.keys.size(); ++i) {
    if (a.keys[i].text != b.keys[i].text) {
      return false;
    }
  }
  for (size_t i = 0; i < a.children.size(); ++i) {
    if (!same_tree(a.children[i], b.children[i])) {
      return false;
    }
  }
  return true;
}

// ── The three wish test charts ───────────────────────────────────────────────

TEST(legacy, WishTestChartsConvertFaithfully) {
  for (const char* name : {"chart.nymph", "pie_heatmap.nymph", "surface3d.nymph"}) {
    SCOPED_TRACE(name);
    const std::string original = read_file(kLegacyDir / name);
    const wish_parts parts = split_by_hand(original);

    const std::optional<std::string> yaml = extract_source(wish_png(original));
    ASSERT_TRUE(yaml.has_value());
    const yaml_node converted = parse_yaml(*yaml);

    // Same description, same element tree and options, same data.
    ASSERT_NE(converted.find("description"), nullptr);
    EXPECT_EQ(converted.find("description")->text, parts.description);
    const yaml_node format = parse_yaml(parts.format);
    EXPECT_TRUE(same_tree(*converted.find("root"), *format.find("root")));
    EXPECT_TRUE(same_tree(*converted.find("image"), *format.find("image")));
    const table want = parse_csv(parts.data);
    const table got = parse_data(*converted.find("data"));
    ASSERT_EQ(got.headers.size(), want.headers.size());
    for (size_t c = 0; c < want.headers.size(); ++c) {
      EXPECT_EQ(got.headers[c].text, want.headers[c].text);
      ASSERT_EQ(got.columns[c].size(), want.columns[c].size());
      for (size_t r = 0; r < want.columns[c].size(); ++r) {
        EXPECT_EQ(got.columns[c][r].text, want.columns[c][r].text);
      }
    }
    // And it is a source nymph accepts and renders.
    EXPECT_NO_THROW(check_source(*yaml));
  }
}

TEST(legacy, ChartShowsALabelAboveTwoPlots) {
  const std::string yaml = *extract_source(wish_png(read_file(kLegacyDir / "chart.nymph")));
  const figure f = bind_source(yaml, builtin_registry());
  ASSERT_EQ(f.root.cls->name(), "VerticalLayout");
  ASSERT_EQ(f.root.children.size(), 3u);
  EXPECT_EQ(f.root.children[0].cls->name(), "Label");
  EXPECT_EQ(f.root.children[1].cls->name(), "Plot");
  EXPECT_EQ(f.root.children[2].cls->name(), "Plot");
  const auto rects = layout_figure(f);
  const rect label = rects.at(&f.root.children[0]);
  const rect first = rects.at(&f.root.children[1]);
  const rect second = rects.at(&f.root.children[2]);
  EXPECT_LE(label.max.y, first.min.y);
  EXPECT_LE(first.max.y, second.min.y);
  EXPECT_GT(first.height(), 100);
  // Rendering the converted source, and re-rendering a wish PNG directly.
  EXPECT_FALSE(render_png(yaml).empty());
}

// ── Detection ────────────────────────────────────────────────────────────────

TEST(legacy, NymphYamlWithDashLinesIsNotTakenForWish) {
  const std::string yaml =
      "description: |\n"
      "  first\n"
      "  --\n"
      "  second\n"
      "root: {type: Plot, children: [{type: PlotLine, ys: $v}]}\n"
      "data: |\n"
      "  v\n"
      "  --\n"
      "  1\n";
  EXPECT_FALSE(is_wish_source(yaml));
  EXPECT_EQ(extract_source(wish_png(yaml)).value_or(""), yaml); // returned verbatim
}

TEST(legacy, OrdinaryTextIsNotAWishSource) {
  EXPECT_FALSE(is_wish_source("root: {type: Plot}\n"));
  EXPECT_FALSE(is_wish_source("just one\n--\nseparator\n"));
  EXPECT_FALSE(convert_wish_source("root: {type: Plot}\n").has_value());
}

// ── Conversion details ───────────────────────────────────────────────────────

TEST(legacy, EmptyPartsAreLeftOut) {
  const std::string yaml = *convert_wish_source("--\nroot: {type: Plot}\n--\n");
  EXPECT_EQ(yaml, "nymph: 1\nroot: {type: Plot}\n");
}

TEST(legacy, CrlfAndLeadingSpacesSurvive) {
  const std::string wish = "  indented note\r\nsecond line\r\n--\r\nroot: {type: Plot}\r\n--\r\n  a, b\r\n1, 2\r\n";
  const std::string yaml = *convert_wish_source(wish);
  const yaml_node doc = parse_yaml(yaml);
  EXPECT_EQ(doc.find("description")->text, "  indented note\nsecond line");
  const table t = parse_data(*doc.find("data"));
  ASSERT_EQ(t.headers.size(), 2u);
  EXPECT_EQ(t.headers[0].text, "a");
  EXPECT_EQ(t.numbers(1), (std::vector<double>{2.0}));
}

TEST(legacy, OnlyTheFirstTwoSeparatorsSplit) {
  // As in wish: a `--` line in the CSV part is data, not a separator.
  const std::string yaml = *convert_wish_source("d\n--\nroot: {type: Plot}\n--\nv\n--\n1\n");
  EXPECT_EQ(parse_yaml(yaml).find("data")->text, "v\n--\n1\n");
}

} // namespace
} // namespace bdg::nymph
