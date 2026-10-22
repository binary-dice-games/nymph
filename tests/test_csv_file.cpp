// MIT License © 2026 Binary Dice Games
/// @file test_csv_file.cpp
/// @brief spec/14_editor_workspace plan step 6: CSV import and export, and
///        the rows of a pasted block.
#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <string>
#include "core/error.hpp"
#include "edit/csv_file.hpp"

namespace bdg::nymph {
namespace {

/// A directory of this test's own (ctest runs tests in parallel).
std::filesystem::path test_dir() {
  const auto* info = ::testing::UnitTest::GetInstance()->current_test_info();
  const std::filesystem::path dir =
      std::filesystem::temp_directory_path() / (std::string("nymph_csv_") + info->test_suite_name() + "_" + info->name());
  std::filesystem::remove_all(dir);
  std::filesystem::create_directories(dir);
  return dir;
}

std::string read(const std::filesystem::path& p) {
  std::ifstream in(p, std::ios::binary);
  return std::string(std::istreambuf_iterator<char>(in), {});
}

TEST(csv_file, ImportThenExportGivesTheSameFile) {
  const std::filesystem::path dir = test_dir();
  const std::string text = "name, note\n\"a, b\", \"said \"\"hi\"\"\"\nüber, 3\n";
  std::ofstream(dir / "in.csv", std::ios::binary) << "\xEF\xBB\xBF" << text; // with a BOM
  const table_text t = read_csv_file(dir / "in.csv");
  EXPECT_EQ(t.headers, (std::vector<std::string>{"name", "note"}));
  EXPECT_EQ(t.rows[1][0], "über");
  write_csv_file(dir / "out.csv", t);
  EXPECT_EQ(read(dir / "out.csv"), text); // the BOM is not written back
  std::filesystem::remove_all(dir);
}

TEST(csv_file, AnUnwritableTargetLeavesNoPartialFile) {
  const std::filesystem::path dir = test_dir();
  const std::filesystem::path target = dir / "missing" / "out.csv";
  EXPECT_THROW(write_csv_file(target, table_text{{"a"}, {{"1"}}}), error);
  EXPECT_FALSE(std::filesystem::exists(target));
  EXPECT_FALSE(std::filesystem::exists(dir / "missing"));
  std::filesystem::remove_all(dir);
}

TEST(csv_file, ExportReplacesAnExistingFile) {
  const std::filesystem::path dir = test_dir();
  std::ofstream(dir / "out.csv") << "old\n";
  write_csv_file(dir / "out.csv", table_text{{"a"}, {{"1"}}});
  EXPECT_EQ(read(dir / "out.csv"), "a\n1\n");
  EXPECT_EQ(std::distance(std::filesystem::directory_iterator(dir), {}), 1); // no temporary left
  std::filesystem::remove_all(dir);
}

TEST(csv_file, AFileThatDoesNotParseNamesItself) {
  const std::filesystem::path dir = test_dir();
  std::ofstream(dir / "bad.csv") << "a, b\n1\n";
  try {
    (void)read_csv_file(dir / "bad.csv");
    FAIL() << "a ragged row was read";
  } catch (const error& e) {
    EXPECT_NE(std::string(e.what()).find("bad.csv"), std::string::npos) << e.what();
  }
  EXPECT_THROW((void)read_csv_file(dir / "none.csv"), error);
  std::filesystem::remove_all(dir);
}

TEST(csv_file, PastedRowsAreCsvOrTabSeparated) {
  EXPECT_EQ(parse_rows("1, 2\n3, \"a, b\"\n"), (std::vector<std::vector<std::string>>{{"1", "2"}, {"3", "a, b"}}));
  EXPECT_EQ(parse_rows("1\t 2\r\n3\t\n"), (std::vector<std::vector<std::string>>{{"1", " 2"}, {"3", ""}}));
  EXPECT_EQ(parse_rows("x"), (std::vector<std::vector<std::string>>{{"x"}}));
  EXPECT_TRUE(parse_rows("").empty());
}

} // namespace
} // namespace bdg::nymph
