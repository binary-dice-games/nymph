// MIT License © 2026 Binary Dice Games
/// @file test_cli.cpp
/// @brief Plan step 12: the `nymph` command line, run in-process through
///        run_cli() with files in a temporary directory per test.
#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>
#include "cli/cli.hpp"

namespace bdg::nymph {
namespace {

namespace fs = std::filesystem;

const char* const kSource =
    "description: A small line chart.\n"
    "image: {width: 200, height: 150}\n"
    "root:\n"
    "  type: Plot\n"
    "  children:\n"
    "    - {type: PlotLine, label: v, ys: $v}\n"
    "data: {v: [1, 3, 2]}\n";

/// The result of one run_cli() call.
struct run_result {
  int code = 0;
  std::string out;
  std::string err;
};

/// A fresh directory named after the running test, removed afterwards.
class cli : public ::testing::Test {
 protected:
  void SetUp() override {
    dir_ = fs::temp_directory_path() /
        (std::string("nymph_cli_") + ::testing::UnitTest::GetInstance()->current_test_info()->name());
    fs::remove_all(dir_);
    fs::create_directories(dir_);
  }
  void TearDown() override {
    std::error_code ignored;
    fs::remove_all(dir_, ignored);
  }

  std::string path(const std::string& name) const {
    return (dir_ / name).string();
  }

  void write(const std::string& name, std::string_view content) const {
    std::ofstream(dir_ / name, std::ios::binary).write(content.data(), static_cast<std::streamsize>(content.size()));
  }

  std::string read(const std::string& name) const {
    std::ifstream f(dir_ / name, std::ios::binary);
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
  }

  bool exists(const std::string& name) const {
    return fs::exists(dir_ / name);
  }

  run_result run(std::initializer_list<std::string> args, const std::string& stdin_text = "") const {
    std::vector<std::string> owned(args);
    std::vector<std::string_view> views(owned.begin(), owned.end());
    std::istringstream in(stdin_text);
    std::ostringstream out;
    std::ostringstream err;
    run_result r;
    r.code = run_cli(views, in, out, err);
    r.out = out.str();
    r.err = err.str();
    return r;
  }

  /// The number of lines in `text`.
  static size_t lines(const std::string& text) {
    size_t n = 0;
    for (char c : text) {
      n += c == '\n' ? 1 : 0;
    }
    return n;
  }

  fs::path dir_;
};

// ── Round trip ───────────────────────────────────────────────────────────────

TEST_F(cli, RenderThenExtractRoundTrips) {
  write("chart.yaml", kSource);
  const run_result render = run({path("chart.yaml")});
  EXPECT_EQ(render.code, 0) << render.err;
  EXPECT_EQ(render.err, "");
  ASSERT_TRUE(exists("chart.png"));
  EXPECT_EQ(read("chart.png").substr(1, 3), "PNG");

  const run_result extract = run({path("chart.png")});
  EXPECT_EQ(extract.code, 0) << extract.err;
  EXPECT_EQ(extract.out, kSource);

  // Explicit commands, and -o for extract.
  EXPECT_EQ(run({"extract", path("chart.png"), "-o", path("back.yaml")}).code, 0);
  EXPECT_EQ(read("back.yaml"), kSource);
  EXPECT_EQ(run({"render", path("back.yaml"), "-o", path("again.png")}).code, 0);
  EXPECT_EQ(read("again.png"), read("chart.png")); // deterministic
  EXPECT_FALSE(exists("chart.png.nymph-tmp"));
}

TEST_F(cli, RenderingANymphPngRerendersItsSourceInPlace) {
  write("chart.yaml", kSource);
  ASSERT_EQ(run({path("chart.yaml")}).code, 0);
  const std::string first = read("chart.png");
  write("chart.png", first + "trailing junk is dropped on re-render");
  ASSERT_EQ(run({"render", path("chart.png")}).code, 0);
  EXPECT_EQ(read("chart.png"), first);
}

TEST_F(cli, StdinAndStdout) {
  const run_result to_file = run({"render", "-", "-o", path("out.png")}, kSource);
  EXPECT_EQ(to_file.code, 0) << to_file.err;
  EXPECT_TRUE(exists("out.png"));

  const run_result to_stdout = run({"render", "-", "-o", "-"}, kSource);
  EXPECT_EQ(to_stdout.code, 0);
  EXPECT_EQ(to_stdout.out, read("out.png"));

  const run_result needs_output = run({"render", "-"}, kSource);
  EXPECT_EQ(needs_output.code, 2);
  EXPECT_NE(needs_output.err.find("needs -o"), std::string::npos);
  EXPECT_NE(needs_output.err.find("usage:"), std::string::npos);

  // A PNG on stdin is extracted to stdout without -o.
  const run_result piped = run({"-"}, read("out.png"));
  EXPECT_EQ(piped.code, 0);
  EXPECT_EQ(piped.out, kSource);
}

// ── Failures ─────────────────────────────────────────────────────────────────

TEST_F(cli, BadSourceIsOneLineWithPositionAndWritesNothing) {
  write("bad.yaml", std::string(kSource).replace(std::string(kSource).find("ys: $v"), 6, "ys: $w"));
  const run_result r = run({path("bad.yaml")});
  EXPECT_EQ(r.code, 1);
  EXPECT_EQ(lines(r.err), 1u) << r.err;
  EXPECT_EQ(r.err.rfind("nymph: " + path("bad.yaml") + ":6:38: unknown column 'w'", 0), 0u) << r.err;
  EXPECT_FALSE(exists("bad.png"));
  EXPECT_FALSE(exists("bad.png.nymph-tmp"));
}

TEST_F(cli, ExistingOutputIsLeftUnchangedOnFailure) {
  write("bad.yaml", "root: {type: Nope}\n");
  write("bad.png", "previous content");
  EXPECT_EQ(run({path("bad.yaml")}).code, 1);
  EXPECT_EQ(read("bad.png"), "previous content");
}

TEST_F(cli, LayoutOverflowIsReportedLikeAnyOtherError) {
  write("tall.yaml", "image: {width: 200, height: 100}\nroot: {type: Plot, height: 500}\n");
  const run_result r = run({"check", path("tall.yaml")});
  EXPECT_EQ(r.code, 1);
  EXPECT_NE(r.err.find(":2:7: Plot needs 500 px of height, has 84"), std::string::npos) << r.err;
}

TEST_F(cli, NonNymphAndMissingInputs) {
  write("chart.yaml", kSource);
  ASSERT_EQ(run({path("chart.yaml")}).code, 0);
  // Strip the nymph chunk (right after IHDR) to get a plain PNG.
  std::string png = read("chart.png");
  const size_t at = 33;
  const size_t length = (static_cast<uint8_t>(png[at]) << 24) | (static_cast<uint8_t>(png[at + 1]) << 16) |
      (static_cast<uint8_t>(png[at + 2]) << 8) | static_cast<uint8_t>(png[at + 3]);
  write("plain.png", png.erase(at, 12 + length));

  const run_result plain = run({path("plain.png")});
  EXPECT_EQ(plain.code, 1);
  EXPECT_EQ(plain.err, "nymph: " + path("plain.png") + ": not a nymph image (the PNG carries no nymph source)\n");

  const run_result not_png = run({"extract", path("chart.yaml")});
  EXPECT_EQ(not_png.code, 1);
  EXPECT_NE(not_png.err.find("not a PNG image"), std::string::npos);

  const run_result missing = run({path("missing.yaml")});
  EXPECT_EQ(missing.code, 1);
  EXPECT_EQ(missing.err, "nymph: " + path("missing.yaml") + ": no such file\n");

  write("corrupt.png", png.substr(0, 40));
  EXPECT_NE(run({path("corrupt.png")}).err.find("truncated PNG"), std::string::npos);
}

TEST_F(cli, BadArgumentsExitTwoWithUsage) {
  for (std::initializer_list<std::string> args : {
           std::initializer_list<std::string>{},
           {"--frobnicate"},
           {"a.yaml", "b.yaml"},
           {"render", "a.yaml", "-o"},
           {"check", "a.yaml", "-o", "x"},
       }) {
    const run_result r = run(args);
    EXPECT_EQ(r.code, 2);
    EXPECT_NE(r.err.find("usage:"), std::string::npos);
  }
}

// ── check, schema, version ───────────────────────────────────────────────────

TEST_F(cli, CheckIsSilentOnAValidSource) {
  write("chart.yaml", kSource);
  const run_result r = run({"check", path("chart.yaml")});
  EXPECT_EQ(r.code, 0);
  EXPECT_EQ(r.out, "");
  EXPECT_EQ(r.err, "");
  EXPECT_FALSE(exists("chart.png"));
}

TEST_F(cli, SchemaListsTypesAndFields) {
  const run_result line = run({"schema", "PlotLine"});
  EXPECT_EQ(line.code, 0);
  for (const char* field : {"xs:", "ys:", "color:", "goes in: Plot"}) {
    EXPECT_NE(line.out.find(field), std::string::npos) << field;
  }
  const run_result all = run({"schema"});
  for (const char* type : {"Plot\n", "Plot3DSurface\n", "VerticalLayout\n", "Label\n"}) {
    EXPECT_NE(all.out.find(type), std::string::npos) << type;
  }
  const run_result md = run({"schema", "--markdown"});
  EXPECT_EQ(md.out.rfind("# nymph element reference", 0), 0u);
  EXPECT_NE(md.out.find("| `xs` | list of numbers |"), std::string::npos);

  const run_result nope = run({"schema", "Nope"});
  EXPECT_EQ(nope.code, 1);
  EXPECT_NE(nope.err.find("unknown element type 'Nope'"), std::string::npos);
}

TEST_F(cli, VersionAndHelp) {
  EXPECT_EQ(run({"--version"}).out.rfind("nymph ", 0), 0u);
  const run_result help = run({"--help"});
  EXPECT_EQ(help.code, 0);
  EXPECT_NE(help.out.find("usage:"), std::string::npos);
}

} // namespace
} // namespace bdg::nymph
