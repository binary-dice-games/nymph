// MIT License © 2026 Binary Dice Games
/// @file test_view_command.cpp
/// @brief Plan step 14: `nymph view` up to the point a window would open:
///        arguments, input checks and errors, window size, save paths. The
///        window itself needs a display and is checked by hand.
#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>
#include "nymph.hpp"
#include "window/view_command.hpp"

namespace bdg::nymph {
namespace {

namespace fs = std::filesystem;

const char* const kSource =
    "description: A small line chart.\n"
    "image: {width: 320, height: 200}\n"
    "root:\n"
    "  type: Plot\n"
    "  children:\n"
    "    - {type: PlotLine, label: v, ys: $v}\n"
    "data: {v: [1, 3, 2]}\n";

std::vector<std::string_view> views(std::initializer_list<std::string_view> args) {
  return std::vector<std::string_view>(args);
}

/// A fresh directory named after the running test, removed afterwards.
class view_command : public ::testing::Test {
 protected:
  void SetUp() override {
    dir_ = fs::temp_directory_path() /
        (std::string("nymph_view_") + ::testing::UnitTest::GetInstance()->current_test_info()->name());
    fs::remove_all(dir_);
    fs::create_directories(dir_);
  }
  void TearDown() override {
    std::error_code ignored;
    fs::remove_all(dir_, ignored);
  }

  std::string write(const std::string& name, std::string_view content) const {
    const fs::path p = dir_ / name;
    std::ofstream(p, std::ios::binary).write(content.data(), static_cast<std::streamsize>(content.size()));
    return p.string();
  }

  /// run_view_cli() on `args`; the code, with the error output in `err`.
  static int run(std::initializer_list<std::string_view> args, std::string& err) {
    std::ostringstream e;
    const std::vector<std::string_view> v = views(args);
    const int code = run_view_cli(v, e);
    err = e.str();
    return code;
  }

  fs::path dir_;
};

// ── Arguments ────────────────────────────────────────────────────────────────

TEST_F(view_command, ParsesTheInputAndTheOptions) {
  const view_args plain = parse_view_args(views({"chart.yaml"}));
  EXPECT_EQ(plain.input, "chart.yaml");
  EXPECT_EQ(plain.theme, "");
  EXPECT_EQ(plain.fit, view_fit::fill);

  const view_args o = parse_view_args(views({"--theme", "dark", "chart.png", "--fit", "source"}));
  EXPECT_EQ(o.input, "chart.png");
  EXPECT_EQ(o.theme, "dark");
  EXPECT_EQ(o.fit, view_fit::source);
  EXPECT_EQ(parse_view_args(views({"c.yaml", "--fit", "width"})).fit, view_fit::width);
}

TEST_F(view_command, RejectsBadArguments) {
  EXPECT_THROW(parse_view_args(views({})), view_usage_error);
  EXPECT_THROW(parse_view_args(views({"a.yaml", "b.yaml"})), view_usage_error);
  EXPECT_THROW(parse_view_args(views({"-"})), view_usage_error);
  EXPECT_THROW(parse_view_args(views({"a.yaml", "--bogus"})), view_usage_error);
  EXPECT_THROW(parse_view_args(views({"a.yaml", "--theme"})), view_usage_error);
  EXPECT_THROW(parse_view_args(views({"a.yaml", "--theme", "neon"})), view_usage_error);
  EXPECT_THROW(parse_view_args(views({"a.yaml", "--fit", "huge"})), view_usage_error);
}

TEST_F(view_command, BadArgumentsAreUsageErrorsWithExitCode2) {
  std::string err;
  EXPECT_EQ(run({}, err), 2);
  EXPECT_NE(err.find("no input"), std::string::npos);
  EXPECT_NE(err.find("usage: nymph view"), std::string::npos);
  EXPECT_EQ(run({"a.yaml", "--theme", "neon"}, err), 2);
  EXPECT_NE(err.find("unknown theme 'neon'"), std::string::npos);
}

TEST_F(view_command, HelpSucceeds) {
  std::string err;
  EXPECT_EQ(run({"--help"}, err), 0);
  EXPECT_TRUE(err.empty());
}

// ── Checks before a window opens ─────────────────────────────────────────────

TEST_F(view_command, AMissingFileFailsWithExitCode1) {
  std::string err;
  EXPECT_EQ(run({(dir_ / "missing.yaml").string()}, err), 1);
  EXPECT_NE(err.find("no such file"), std::string::npos) << err;
}

TEST_F(view_command, ASourceErrorIsReportedWithItsPosition) {
  std::string source = kSource;
  source.replace(source.find("$v"), 2, "$nope");
  const std::string bad = write("bad.yaml", source);
  std::string err;
  EXPECT_EQ(run({bad}, err), 1);
  EXPECT_NE(err.find("bad.yaml:6:"), std::string::npos) << err;
  EXPECT_NE(err.find("nope"), std::string::npos) << err;
}

TEST_F(view_command, APngThatIsNotANymphImageIsRefused) {
  // The PNG signature and nothing else: no nymph source can be read from it.
  const std::string path = write("plain.png", std::string_view("\x89PNG\r\n\x1a\n", 8));
  std::string err;
  EXPECT_EQ(run({path}, err), 1);
  EXPECT_NE(err.find("plain.png"), std::string::npos) << err;
}

TEST_F(view_command, PreparesTheLayoutSizeOfASource) {
  const std::string path = write("chart.yaml", kSource);
  const view_request r = prepare_view(parse_view_args(views({path, "--theme", "dark"})));
  EXPECT_EQ(r.width, 320);
  EXPECT_EQ(r.height, 200);
  EXPECT_NE(r.source.find("PlotLine"), std::string::npos);
}

TEST_F(view_command, PreparesANymphPngLikeItsSource) {
  const std::vector<uint8_t> png = render_png(kSource);
  const std::string path = write("chart.png", std::string_view(reinterpret_cast<const char*>(png.data()), png.size()));
  const view_request r = prepare_view(parse_view_args(views({path})));
  EXPECT_EQ(r.width, 320);
  EXPECT_EQ(r.height, 200);
  EXPECT_EQ(r.source, kSource);
}

// ── Window size and saves ────────────────────────────────────────────────────

TEST_F(view_command, TheWindowIsTheLayoutSizeWithinTheScreen) {
  const window_size roomy = initial_window_size(800, 600, 1920, 1000);
  EXPECT_EQ(roomy.width, 800);
  EXPECT_EQ(roomy.height, 600);
  const window_size tight = initial_window_size(1600, 1200, 1000, 700);
  EXPECT_EQ(tight.width, 1000);
  EXPECT_EQ(tight.height, 700);
  const window_size unknown = initial_window_size(800, 600, 0, 0);
  EXPECT_EQ(unknown.width, 800);
  EXPECT_EQ(unknown.height, 600);
  const window_size tiny = initial_window_size(40, 30, 1920, 1080);
  EXPECT_EQ(tiny.width, 240);
  EXPECT_EQ(tiny.height, 160);
}

TEST_F(view_command, SavesGoNextToTheInputAndNeverOverIt) {
  const fs::path in = fs::path("dir") / "chart.yaml";
  EXPECT_EQ(fs::path(view_save_path(in.string(), "png")), fs::path("dir") / "chart.view.png");
  EXPECT_EQ(fs::path(view_save_path(in.string(), "yaml")), fs::path("dir") / "chart.view.yaml");
  EXPECT_NE(view_save_path("chart.view.png", "png"), "chart.view.png");
  EXPECT_EQ(fs::path(view_save_path("chart.png", "png")).filename(), "chart.view.png");
}

} // namespace
} // namespace bdg::nymph
