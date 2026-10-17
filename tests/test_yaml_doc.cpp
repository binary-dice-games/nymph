// MIT License © 2026 Binary Dice Games
/// @file test_yaml_doc.cpp
/// @brief Plan step 5: parse_yaml() builds a positioned node tree and
///        reports every problem with its line and column.
#include <gtest/gtest.h>
#include <string>
#include "core/error.hpp"
#include "document/yaml_doc.hpp"

namespace bdg::nymph {
namespace {

/// Parses `text`, expecting an error; returns it.
error parse_error(const std::string& text) {
  try {
    parse_yaml(text);
  } catch (const error& e) {
    return e;
  }
  ADD_FAILURE() << "no error for:\n" << text;
  return error(0, 0, "");
}

// ── Positions ────────────────────────────────────────────────────────────────

const char* const kSource =
    "root:\n" // 1
    "  type: Plot\n" // 2
    "  children:\n" // 3
    "    - type: PlotLine\n" // 4
    "      xs: [1, 2, 3]\n" // 5
    "data: |\n" // 6
    "  a, b\n" // 7
    "  1, 2\n"; // 8

TEST(yaml_doc, NodesKnowTheirLineAndColumn) {
  const yaml_node doc = parse_yaml(kSource);
  ASSERT_TRUE(doc.is_mapping());
  ASSERT_EQ(doc.keys.size(), 2u);
  EXPECT_EQ(doc.keys[0].text, "root");
  EXPECT_EQ(doc.keys[0].line, 1);
  EXPECT_EQ(doc.keys[0].column, 1);

  const yaml_node* root = doc.find("root");
  ASSERT_NE(root, nullptr);
  ASSERT_TRUE(root->is_mapping());
  EXPECT_EQ(root->line, 2);
  EXPECT_EQ(root->column, 3);
  const yaml_node* type_key = root->find_key("type");
  ASSERT_NE(type_key, nullptr);
  EXPECT_EQ(type_key->line, 2);
  EXPECT_EQ(type_key->column, 3);
  EXPECT_EQ(root->find("type")->column, 9);

  const yaml_node* children = root->find("children");
  ASSERT_TRUE(children != nullptr && children->is_sequence());
  ASSERT_EQ(children->children.size(), 1u);
  const yaml_node& item = children->children[0];
  EXPECT_EQ(item.line, 4);
  EXPECT_EQ(item.column, 7);

  const yaml_node* xs = item.find("xs");
  ASSERT_TRUE(xs != nullptr && xs->is_sequence());
  EXPECT_EQ(xs->line, 5);
  EXPECT_EQ(xs->column, 11);
  ASSERT_EQ(xs->children.size(), 3u);
  EXPECT_EQ(xs->children[1].text, "2");
  EXPECT_EQ(xs->children[1].line, 5);
  EXPECT_EQ(xs->children[1].column, 15);
}

TEST(yaml_doc, BlockScalarKnowsWhereItsTextStarts) {
  const yaml_node doc = parse_yaml(kSource);
  const yaml_node* data = doc.find("data");
  ASSERT_NE(data, nullptr);
  EXPECT_EQ(data->style, scalar_style::literal);
  EXPECT_EQ(data->text, "a, b\n1, 2\n");
  EXPECT_EQ(data->line, 6);
  EXPECT_EQ(data->column, 7); // the '|' indicator
  EXPECT_EQ(data->content_line, 7);
  EXPECT_EQ(data->content_column, 3);
}

TEST(yaml_doc, BlockScalarWithLeadingBlankLineAndComment) {
  const yaml_node doc = parse_yaml("data: | # csv below\n\n    x\n    1\nnext: 2\n");
  const yaml_node* data = doc.find("data");
  ASSERT_NE(data, nullptr);
  EXPECT_EQ(data->text, "\nx\n1\n");
  EXPECT_EQ(data->content_line, 2); // text line 0 is the blank line 2
  EXPECT_EQ(data->content_column, 5);
}

TEST(yaml_doc, ColumnsCountCharactersNotBytes) {
  const yaml_node doc = parse_yaml("\xc3\xb1\xc3\xb1: v\n"); // "ññ: v"
  ASSERT_EQ(doc.keys.size(), 1u);
  EXPECT_EQ(doc.children[0].column, 5);
}

TEST(yaml_doc, CrlfLineEndingsKeepPositions) {
  const yaml_node doc = parse_yaml("a: 1\r\nb:\r\n  c: 2\r\n");
  const yaml_node* b = doc.find("b");
  ASSERT_NE(b, nullptr);
  EXPECT_EQ(b->find("c")->line, 3);
  EXPECT_EQ(b->find("c")->column, 6);
}

// ── Scalars ──────────────────────────────────────────────────────────────────

TEST(yaml_doc, QuotedAndPlainScalarsAreToldApart) {
  const yaml_node doc = parse_yaml("a: 2025\nb: \"2025\"\nc: '2025'\nd: >\n  folded\n  text\n");
  EXPECT_EQ(doc.find("a")->style, scalar_style::plain);
  EXPECT_FALSE(doc.find("a")->quoted());
  EXPECT_EQ(doc.find("b")->style, scalar_style::double_quoted);
  EXPECT_TRUE(doc.find("b")->quoted());
  EXPECT_EQ(doc.find("c")->style, scalar_style::single_quoted);
  EXPECT_TRUE(doc.find("c")->quoted());
  EXPECT_EQ(doc.find("a")->text, "2025");
  EXPECT_EQ(doc.find("b")->text, "2025");
  EXPECT_EQ(doc.find("d")->style, scalar_style::folded);
  EXPECT_EQ(doc.find("d")->text, "folded text\n");
}

TEST(yaml_doc, EmptyValueAndCommentAreAnEmptyPlainScalar) {
  // `color: #C0392B` is a comment: the value is empty, which bind() reports.
  const yaml_node doc = parse_yaml("color: #C0392B\n");
  const yaml_node* color = doc.find("color");
  ASSERT_NE(color, nullptr);
  EXPECT_TRUE(color->is_scalar());
  EXPECT_EQ(color->text, "");
  EXPECT_EQ(color->style, scalar_style::plain);
}

TEST(yaml_doc, AliasesAreExpandedIntoCopies) {
  const yaml_node doc = parse_yaml("a: &style {k: 1}\nb: *style\n");
  const yaml_node* b = doc.find("b");
  ASSERT_TRUE(b != nullptr && b->is_mapping());
  EXPECT_EQ(b->find("k")->text, "1");
}

TEST(yaml_doc, FindOnANonMappingIsNull) {
  const yaml_node doc = parse_yaml("[1, 2]\n");
  EXPECT_TRUE(doc.is_sequence());
  EXPECT_EQ(doc.find("1"), nullptr);
}

// ── Errors ───────────────────────────────────────────────────────────────────

TEST(yaml_doc, SyntaxErrorHasAPosition) {
  const error e = parse_error("root:\n  xs: [1, 2\n  type: Plot\n");
  EXPECT_GE(e.line(), 2);
  EXPECT_GT(e.column(), 0);
  // The flow sequence's start is named, since the error is noticed later.
  EXPECT_NE(std::string(e.what()).find("started at line 2, column 7"), std::string::npos) << e.what();
}

TEST(yaml_doc, UnterminatedQuoteNamesWhereTheQuoteStarted) {
  const error e = parse_error("a: \"unterminated\n");
  EXPECT_NE(std::string(e.what()).find("started at line 1, column 4"), std::string::npos) << e.what();
}

TEST(yaml_doc, DuplicateKeyIsReportedAtTheSecond) {
  const error e = parse_error("root:\n  type: Plot\n  title: a\n  title: b\n");
  EXPECT_EQ(e.line(), 4);
  EXPECT_EQ(e.column(), 3);
  EXPECT_NE(std::string(e.what()).find("duplicate key 'title'"), std::string::npos);
  EXPECT_NE(std::string(e.what()).find("line 3"), std::string::npos);
}

TEST(yaml_doc, NonScalarKeyIsAnError) {
  const error e = parse_error("? [a, b]\n: 1\n");
  EXPECT_EQ(e.line(), 1);
  EXPECT_NE(std::string(e.what()).find("mapping key"), std::string::npos);
}

TEST(yaml_doc, SecondDocumentIsAnError) {
  const error e = parse_error("a: 1\n---\nb: 2\n");
  EXPECT_EQ(e.line(), 2);
  EXPECT_NE(std::string(e.what()).find("second YAML document"), std::string::npos);
}

TEST(yaml_doc, EmptyDocumentIsAnError) {
  for (const char* text : {"", "\n\n", "# only a comment\n"}) {
    const error e = parse_error(text);
    EXPECT_EQ(e.line(), 1);
    EXPECT_NE(std::string(e.what()).find("empty"), std::string::npos) << text;
  }
}

TEST(yaml_doc, TabIndentationGetsItsOwnMessage) {
  const error e = parse_error("root:\n\ttype: Plot\n");
  EXPECT_EQ(e.line(), 2);
  EXPECT_EQ(e.column(), 1);
  EXPECT_NE(std::string(e.what()).find("tab"), std::string::npos) << e.what();
}

TEST(yaml_doc, DeepNestingIsAnError) {
  const std::string deep = std::string(300, '[') + std::string(300, ']') + "\n";
  const error e = parse_error(deep);
  EXPECT_NE(std::string(e.what()).find("too deep"), std::string::npos);
}

TEST(yaml_doc, AliasExpansionIsBounded) {
  // Each level refers to the previous one ten times: 10^7 nodes in total.
  std::string text = "l0: &l0 [x, x, x, x, x, x, x, x, x, x]\n";
  for (int level = 1; level <= 6; ++level) {
    const std::string prev = "*l" + std::to_string(level - 1);
    text += "l" + std::to_string(level) + ": &l" + std::to_string(level) + " [";
    for (int i = 0; i < 10; ++i) {
      text += (i ? ", " : "") + prev;
    }
    text += "]\n";
  }
  const error e = parse_error(text);
  EXPECT_NE(std::string(e.what()).find("too large"), std::string::npos);
}

} // namespace
} // namespace bdg::nymph
