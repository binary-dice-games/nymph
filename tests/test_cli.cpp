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
#include "nymph.hpp"
#include "render/renderer_options.hpp"

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
    return run_with(renderer_test_options{}, args, stdin_text);
  }

  /// run() with the renderer's internal test options.
  run_result run_with(
      const renderer_test_options& gl_options,
      std::initializer_list<std::string> args,
      const std::string& stdin_text = "") const {
    std::vector<std::string> owned(args);
    std::vector<std::string_view> views(owned.begin(), owned.end());
    std::istringstream in(stdin_text);
    std::ostringstream out;
    std::ostringstream err;
    run_result r;
    r.code = run_cli(views, in, out, err, gl_options);
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

// ── source and named tables ──────────────────────────────────────────────────

TEST_F(cli, SourceKeyAndNamedTablesRoundTrip) {
  const std::string with_source =
      "source:\n"
      "  prompt: Chart weekly sales against costs.\n"
      "  code: |\n"
      "    rows = db.query(\"select week, sales from $table\")\n"
      "  language: python\n"
      "image: {width: 200, height: 150}\n"
      "root:\n"
      "  type: Plot\n"
      "  children:\n"
      "    - {type: PlotLine, label: sales, ys: $sales:v}\n"
      "    - {type: PlotLine, label: costs, ys: $costs:v}\n"
      "data:\n"
      "  sales: {v: [1, 3, 2]}\n"
      "  costs: |\n"
      "    v\n"
      "    2\n"
      "    1\n";
  write("chart.yaml", with_source);
  ASSERT_EQ(run({path("chart.yaml")}).code, 0);
  const run_result extract = run({path("chart.png")});
  EXPECT_EQ(extract.code, 0);
  EXPECT_EQ(extract.out, with_source);

  // The same chart without source draws the same pixels: only the
  // embedded text differs.
  std::string without = with_source.substr(with_source.find("image:"));
  write("plain.yaml", without);
  ASSERT_EQ(run({path("plain.yaml")}).code, 0);
  const std::string a = read("chart.png");
  const std::string b = read("plain.png");
  EXPECT_NE(a, b);
  EXPECT_EQ(a.substr(a.find("IDAT")), b.substr(b.find("IDAT")));
}

// ── Output targets ───────────────────────────────────────────────────────────

TEST_F(cli, OutputThroughASymlinkReplacesTheFileItPointsTo) {
  write("chart.yaml", kSource);
  write("real.png", "old");
  std::error_code ec;
  fs::create_symlink(dir_ / "real.png", dir_ / "link.png", ec);
  if (ec) {
    GTEST_SKIP() << "symbolic links unavailable: " << ec.message();
  }
  ASSERT_EQ(run({path("chart.yaml"), "-o", path("link.png")}).code, 0);
  EXPECT_TRUE(fs::is_symlink(dir_ / "link.png"));
  EXPECT_EQ(read("real.png").substr(1, 3), "PNG");
}

TEST_F(cli, OutputThatIsNotARegularFileIsNotReplaced) {
  write("chart.yaml", kSource);
  fs::create_directories(dir_ / "out.png");
  const run_result r = run({path("chart.yaml"), "-o", path("out.png")});
  EXPECT_EQ(r.code, 1);
  EXPECT_NE(r.err.find("cannot be written"), std::string::npos) << r.err;
  EXPECT_TRUE(fs::is_directory(dir_ / "out.png"));
}

// ── fonts ────────────────────────────────────────────────────────────────────

TEST_F(cli, FontsListsTheNamesAndPrintsTheLicences) {
  const run_result names = run({"fonts"});
  EXPECT_EQ(names.code, 0);
  EXPECT_EQ(names.out.rfind("default\n", 0), 0u);
  EXPECT_NE(names.out.find("\nFiraSans-Regular.ttf\n"), std::string::npos);
  const run_result licenses = run({"fonts", "--licenses"});
  EXPECT_EQ(licenses.code, 0);
  EXPECT_NE(licenses.out.find("== FiraSans-OFL.txt =="), std::string::npos);
  EXPECT_NE(licenses.out.find("SIL OPEN FONT LICENSE"), std::string::npos);
  EXPECT_EQ(run({"fonts", "x.ttf"}).code, 2);
}

// ── Rasterizers (spec/02_opengl_rasterizer, plan step 6) ─────────────────────

#if defined(_WIN32)
const char* const kMissingGL = "nymph-no-such-opengl32.dll";
#else
const char* const kMissingGL = "nymph-no-such-libEGL.so";
#endif

/// The forced missing library, or, in a build without OpenGL, that.
bool names_missing_gl(const std::string& text) {
  return text.find(kMissingGL) != std::string::npos || text.find("built without OpenGL support") != std::string::npos;
}

std::string png_text(const std::vector<uint8_t>& png) {
  return std::string(png.begin(), png.end());
}

TEST_F(cli, SoftwareRasterizerGivesTheLibraryBytes) {
  write("chart.yaml", kSource);
  const run_result r = run({"render", path("chart.yaml"), "--rasterizer", "software"});
  EXPECT_EQ(r.code, 0) << r.err;
  EXPECT_EQ(r.err, "");
  EXPECT_EQ(read("chart.png"), png_text(render_png(kSource)));
  // The implicit render takes it too, before or after the file.
  EXPECT_EQ(run({"--rasterizer", "software", path("chart.yaml"), "-o", path("b.png")}).code, 0);
  EXPECT_EQ(read("b.png"), read("chart.png"));
}

TEST_F(cli, BadRasterizerArgumentsExitTwo) {
  write("chart.yaml", kSource);
  for (std::initializer_list<std::string> args : {
           std::initializer_list<std::string>{path("chart.yaml"), "--rasterizer", "vulkan"},
           {path("chart.yaml"), "--rasterizer"},
           {"check", path("chart.yaml"), "--rasterizer", "software"},
           {"schema", "--rasterizer", "opengl"},
           {"rasterizers", "foo"},
       }) {
    const run_result r = run(args);
    EXPECT_EQ(r.code, 2) << r.err;
    EXPECT_NE(r.err.find("usage:"), std::string::npos);
  }
  ASSERT_EQ(run({path("chart.yaml")}).code, 0);
  // `nymph chart.png --rasterizer ...` is an extract: the option does not apply.
  EXPECT_EQ(run({path("chart.png"), "--rasterizer", "opengl"}).code, 2);
  EXPECT_FALSE(exists("chart.yaml.png"));
}

TEST_F(cli, DefaultRenderIsSilentWhateverTheRasterizer) {
  write("chart.yaml", kSource);
  const run_result r = run({path("chart.yaml")});
  EXPECT_EQ(r.code, 0);
  EXPECT_EQ(r.err, "");
  ASSERT_TRUE(exists("chart.png"));
  EXPECT_EQ(run({"extract", path("chart.png")}).out, kSource);
}

TEST_F(cli, FallbackIsSilentByDefaultAndWarnsWhenOpenGLWasAskedFor) {
  write("chart.yaml", kSource);
  renderer_test_options no_gl;
  no_gl.library_names = {kMissingGL};

  const run_result quiet = run_with(no_gl, {path("chart.yaml"), "-o", path("quiet.png")});
  EXPECT_EQ(quiet.code, 0);
  EXPECT_EQ(quiet.err, "");
  EXPECT_EQ(read("quiet.png"), png_text(render_png(kSource)));

  const run_result loud = run_with(no_gl, {path("chart.yaml"), "--rasterizer", "opengl", "-o", path("loud.png")});
  EXPECT_EQ(loud.code, 0);
  EXPECT_EQ(lines(loud.err), 1u) << loud.err;
  EXPECT_EQ(loud.err.rfind("nymph: warning: OpenGL unavailable (", 0), 0u) << loud.err;
  EXPECT_TRUE(names_missing_gl(loud.err)) << loud.err;
  EXPECT_EQ(read("loud.png"), png_text(render_png(kSource)));
}

TEST_F(cli, RasterizersReportsWhatIsAvailable) {
  const run_result r = run({"rasterizers"});
  EXPECT_EQ(r.code, 0) << r.err;
  const auto [available, text] = probe_opengl();
  EXPECT_EQ(
      r.out, "software: available\nopengl: " + std::string(available ? "available (" : "unavailable (") + text + ")\n");

  renderer_test_options no_gl;
  no_gl.library_names = {kMissingGL};
  const run_result missing = run_with(no_gl, {"rasterizers"});
  EXPECT_EQ(missing.code, 0);
  EXPECT_NE(missing.out.find("opengl: unavailable ("), std::string::npos) << missing.out;
  EXPECT_TRUE(names_missing_gl(missing.out)) << missing.out;
}

TEST_F(cli, ExplicitOpenGLWithAGPUWritesAnImageWithoutWarning) {
  renderer_test_options gl;
  gl.allow_software_gl = true;
  const renderer probe = make_renderer(rasterizer::opengl, gl);
  if (probe.active() != rasterizer::opengl) {
    GTEST_SKIP() << "no OpenGL device: " << probe.fallback_reason();
  }
  write("chart.yaml", kSource);
  const run_result r = run_with(gl, {path("chart.yaml"), "--rasterizer", "opengl"});
  EXPECT_EQ(r.code, 0);
  EXPECT_EQ(r.err, "");
  EXPECT_EQ(run({"extract", path("chart.png")}).out, kSource);
}

} // namespace
} // namespace bdg::nymph
