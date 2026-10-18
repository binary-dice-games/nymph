// MIT License © 2026 Binary Dice Games
/// @file test_mcp_tools.cpp
/// @brief spec/06_claude_integration/PLAN.md step 3: the tools of
///        `nymph mcp`, called through the session, with files in a
///        temporary directory per test.
#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#include "cli/cli.hpp"
#include "mcp/base64.hpp"
#include "mcp/mcp_session.hpp"
#include "nymph.hpp"

namespace bdg::nymph::mcp {
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

/// Line 6, column 41: the column 'w' does not exist.
const char* const kBadSource =
    "description: A small line chart.\n"
    "image: {width: 200, height: 150}\n"
    "root:\n"
    "  type: Plot\n"
    "  children:\n"
    "    - {type: PlotLine, label: v, ys: $w}\n"
    "data: {v: [1, 3, 2]}\n";

/// A 1x1 PNG without a nymph source.
const char* const kPlainPngBase64 =
    "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mNkYPhfDwAChwGA60e6kgAAAABJRU5ErkJggg==";

std::string read_file(const fs::path& path) {
  std::ifstream f(path, std::ios::binary);
  return std::string(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
}

std::string as_string(const std::vector<uint8_t>& bytes) {
  return std::string(bytes.begin(), bytes.end());
}

/// The CLI's standard error for `args`, without "nymph: " and the newline.
std::string cli_error(std::vector<std::string_view> args) {
  std::istringstream in;
  std::ostringstream out;
  std::ostringstream err;
  EXPECT_EQ(run_cli(args, in, out, err), 1);
  std::string text = err.str();
  EXPECT_EQ(text.rfind("nymph: ", 0), 0u) << text;
  return text.substr(7, text.size() - 8);
}

std::string cli_output(std::vector<std::string_view> args) {
  std::istringstream in;
  std::ostringstream out;
  std::ostringstream err;
  EXPECT_EQ(run_cli(args, in, out, err), 0) << err.str();
  return out.str();
}

/// A session after `initialize`, in a fresh directory named after the test.
class mcp_tools : public ::testing::Test {
 protected:
  void SetUp() override {
    dir_ = fs::temp_directory_path() /
        (std::string("nymph_mcp_tools_") + ::testing::UnitTest::GetInstance()->current_test_info()->name());
    fs::remove_all(dir_);
    fs::create_directories(dir_);
    start({});
  }
  void TearDown() override {
    std::error_code ignored;
    fs::remove_all(dir_, ignored);
  }

  /// Replaces the session with one started with `options`.
  void start(server_options options) {
    session_ = std::make_unique<session>(options);
    rpc_message init;
    init.id = 0;
    init.method = "initialize";
    init.params = {{"protocolVersion", "2025-11-25"}};
    ASSERT_TRUE(session_->handle(init)->contains("result"));
  }

  /// The JSON-RPC response to `tools/call`.
  json call_raw(const json& params) {
    rpc_message m;
    m.id = ++next_id_;
    m.method = "tools/call";
    m.params = params;
    return *session_->handle(m);
  }

  /// The CallToolResult of a tool call.
  json call(const std::string& name, const json& arguments = json::object()) {
    const json response = call_raw({{"name", name}, {"arguments", arguments}});
    EXPECT_TRUE(response.contains("result")) << response.dump();
    return response.value("result", json::object());
  }

  /// The text of a result that must be an error.
  std::string error_text(const json& result) {
    EXPECT_TRUE(result.value("isError", false)) << result.dump().substr(0, 300);
    return result["content"][0]["text"].get<std::string>();
  }

  /// The text blocks of a successful result, joined by newlines.
  std::string text_of(const json& result) {
    EXPECT_FALSE(result.value("isError", false)) << result.dump().substr(0, 300);
    std::string text;
    for (const json& block : result["content"]) {
      if (block["type"] == "text") {
        text += (text.empty() ? "" : "\n") + block["text"].get<std::string>();
      }
    }
    return text;
  }

  std::string path(const std::string& name) const {
    return (dir_ / name).string();
  }

  void write(const std::string& name, std::string_view content) const {
    std::ofstream(dir_ / name, std::ios::binary).write(content.data(), static_cast<std::streamsize>(content.size()));
  }

  fs::path dir_;
  std::unique_ptr<session> session_;
  int next_id_ = 0;
};

server_options software() {
  server_options o;
  o.raster = rasterizer::software;
  return o;
}

// ── tools/list and tools/call ───────────────────────────────────────────────

TEST_F(mcp_tools, list_names_every_tool_with_its_schemas) {
  rpc_message m;
  m.id = 1;
  m.method = "tools/list";
  const json tools = (*session_->handle(m))["result"]["tools"];
  std::vector<std::string> names;
  for (const json& t : tools) {
    names.push_back(t["name"]);
    EXPECT_FALSE(t["description"].get<std::string>().empty());
    EXPECT_EQ(t["inputSchema"]["type"], "object");
    EXPECT_EQ(t["inputSchema"]["additionalProperties"], false);
    EXPECT_TRUE(t["annotations"].contains("readOnlyHint"));
  }
  EXPECT_EQ(
      names, (std::vector<std::string>{"render_chart", "check_chart", "extract_chart", "chart_schema", "list_fonts"}));
  const json& render = tools[0];
  for (const char* p : {"source", "input_path", "output_path", "rasterizer"}) {
    EXPECT_TRUE(render["inputSchema"]["properties"].contains(p)) << p;
  }
  EXPECT_EQ(render["outputSchema"]["required"], json({"source", "width", "height"}));
  EXPECT_EQ(render["annotations"]["readOnlyHint"], false);
  EXPECT_EQ(tools[1]["annotations"]["readOnlyHint"], true);
}

TEST_F(mcp_tools, unknown_tools_and_bad_params_are_protocol_errors) {
  json r = call_raw({{"name", "render_chrt"}, {"arguments", json::object()}});
  EXPECT_EQ(r["error"]["code"], -32602);
  EXPECT_NE(r["error"]["message"].get<std::string>().find("did you mean 'render_chart'"), std::string::npos);
  EXPECT_EQ(call_raw({{"arguments", json::object()}})["error"]["code"], -32602);
  EXPECT_EQ(call_raw({{"name", "list_fonts"}, {"arguments", json::array()}})["error"]["code"], -32602);
  // Without arguments: an empty object.
  EXPECT_FALSE(call_raw({{"name", "list_fonts"}})["result"].value("isError", false));
}

TEST_F(mcp_tools, a_throwing_tool_is_an_error_result_and_the_next_call_works) {
  session_->tools().add({{{"name", "boom"}, {"inputSchema", {{"type", "object"}}}}, [](const json&) -> json {
                           throw std::logic_error("kaput");
                         }});
  EXPECT_EQ(error_text(call("boom")), "internal error: kaput");
  EXPECT_EQ(text_of(call("check_chart", {{"source", kSource}})), "valid");
}

// ── render_chart ────────────────────────────────────────────────────────────

TEST_F(mcp_tools, render_returns_the_same_png_as_the_library) {
  start(software());
  const std::string source = read_file(fs::path(NYMPH_SOURCE_DIR) / "examples" / "bars.yaml");
  const json result = call("render_chart", {{"source", source}});
  ASSERT_FALSE(result.value("isError", false)) << result.dump().substr(0, 300);
  ASSERT_EQ(result["content"][0]["type"], "image");
  EXPECT_EQ(result["content"][0]["mimeType"], "image/png");
  const auto png = base64_decode(result["content"][0]["data"].get<std::string>());
  ASSERT_TRUE(png.has_value());
  EXPECT_EQ(*png, render_png(source));
  EXPECT_EQ(result["structuredContent"]["source"], source);
  EXPECT_GT(result["structuredContent"]["width"].get<int>(), 0);
  EXPECT_GT(result["structuredContent"]["height"].get<int>(), 0);
  EXPECT_FALSE(result["structuredContent"].contains("path"));
  EXPECT_NE(text_of(result).find("Rendered "), std::string::npos);
}

TEST_F(mcp_tools, render_reports_the_image_size) {
  const json result = call("render_chart", {{"source", kSource}});
  EXPECT_EQ(result["structuredContent"]["width"], 200);
  EXPECT_EQ(result["structuredContent"]["height"], 150);
  EXPECT_NE(text_of(result).find("Rendered 200x150 PNG"), std::string::npos);
}

TEST_F(mcp_tools, render_of_a_nymph_png_renders_its_source_and_writes_output) {
  start(software());
  const std::vector<uint8_t> png = render_png(kSource);
  write("in.png", as_string(png));
  const json result = call("render_chart", {{"input_path", path("in.png")}, {"output_path", path("out.png")}});
  EXPECT_EQ(result["structuredContent"]["source"], kSource);
  EXPECT_EQ(read_file(dir_ / "out.png"), as_string(png));
  const std::string written = result["structuredContent"]["path"];
  EXPECT_TRUE(fs::path(written).is_absolute());
  EXPECT_TRUE(fs::equivalent(written, dir_ / "out.png"));
  EXPECT_NE(text_of(result).find("written to " + written), std::string::npos);
}

TEST_F(mcp_tools, render_errors_match_the_cli_and_leave_the_output_untouched) {
  write("bad.yaml", kBadSource);
  write("out.png", "old");
  const std::string from_file =
      error_text(call("render_chart", {{"input_path", path("bad.yaml")}, {"output_path", path("out.png")}}));
  EXPECT_EQ(from_file, cli_error({"render", path("bad.yaml"), "-o", path("out.png")}));
  EXPECT_EQ(from_file.rfind(path("bad.yaml") + ":6:", 0), 0u) << from_file;
  EXPECT_EQ(read_file(dir_ / "out.png"), "old");

  const std::string inline_text = error_text(call("render_chart", {{"source", kBadSource}}));
  EXPECT_EQ(inline_text, "source" + from_file.substr(path("bad.yaml").size()));
}

TEST_F(mcp_tools, render_refuses_bad_arguments) {
  EXPECT_NE(error_text(call("render_chart")).find("exactly one of 'source' and 'input_path'"), std::string::npos);
  EXPECT_NE(
      error_text(call("render_chart", {{"source", kSource}, {"input_path", "x.yaml"}})).find("exactly one"),
      std::string::npos);
  EXPECT_NE(
      error_text(call("render_chart", {{"source", kSource}, {"output_path", path("out.jpg")}}))
          .find("output_path must end in .png"),
      std::string::npos);
  EXPECT_FALSE(fs::exists(dir_ / "out.jpg"));
  EXPECT_EQ(
      error_text(call("render_chart", {{"input_path", path("missing.yaml")}})),
      path("missing.yaml") + ": no such file");
  EXPECT_EQ(error_text(call("render_chart", {{"input_path", "-"}})), "input_path must name a file");
  EXPECT_NE(
      error_text(call("render_chart", {{"sorce", kSource}})).find("unknown argument 'sorce' (did you mean 'source'?)"),
      std::string::npos);
  EXPECT_EQ(error_text(call("render_chart", {{"source", 5}})), "argument 'source' must be a string");
  EXPECT_NE(
      error_text(call("render_chart", {{"source", kSource}, {"rasterizer", "vulkan"}})).find("unknown rasterizer"),
      std::string::npos);
}

TEST_F(mcp_tools, render_warns_when_an_explicit_opengl_falls_back) {
  server_options o;
  o.gl_options.library_names = {"nymph-no-such-gl-library"};
  start(o);
  // The server's default: silent.
  EXPECT_EQ(call("render_chart", {{"source", kSource}})["content"].size(), 2u);
  const json result = call("render_chart", {{"source", kSource}, {"rasterizer", "opengl"}});
  EXPECT_EQ(result["content"][0]["type"], "image");
  EXPECT_NE(text_of(result).find("warning: OpenGL unavailable ("), std::string::npos);
}

TEST_F(mcp_tools, render_leaves_out_an_image_too_large_to_return) {
  server_options o = software();
  o.max_image_base64 = 100;
  start(o);
  const json result = call("render_chart", {{"source", kSource}, {"output_path", path("out.png")}});
  ASSERT_EQ(result["content"].size(), 1u);
  EXPECT_NE(text_of(result).find("too large to return here"), std::string::npos);
  EXPECT_EQ(read_file(dir_ / "out.png"), as_string(render_png(kSource)));
}

// ── check_chart ─────────────────────────────────────────────────────────────

TEST_F(mcp_tools, check_validates_text_and_files) {
  write("bad.yaml", kBadSource);
  EXPECT_EQ(text_of(call("check_chart", {{"source", kSource}})), "valid");
  EXPECT_EQ(
      error_text(call("check_chart", {{"input_path", path("bad.yaml")}})), cli_error({"check", path("bad.yaml")}));
  EXPECT_EQ(
      error_text(call("check_chart", {{"source", kSource}, {"output_path", "x.png"}})).rfind("unknown argument", 0),
      0u);
}

// ── extract_chart ───────────────────────────────────────────────────────────

TEST_F(mcp_tools, extract_round_trips_from_a_file_and_from_base64) {
  const std::vector<uint8_t> png = render_png(kSource);
  write("chart.png", as_string(png));
  EXPECT_EQ(text_of(call("extract_chart", {{"input_path", path("chart.png")}})), kSource);
  EXPECT_EQ(text_of(call("extract_chart", {{"png_base64", base64_encode(png)}})), kSource);
  const json written = call("extract_chart", {{"input_path", path("chart.png")}, {"output_path", path("chart.yaml")}});
  EXPECT_EQ(read_file(dir_ / "chart.yaml"), kSource);
  EXPECT_NE(text_of(written).find("written to "), std::string::npos);
}

TEST_F(mcp_tools, extract_refuses_what_carries_no_source) {
  EXPECT_EQ(
      error_text(call("extract_chart", {{"png_base64", kPlainPngBase64}})),
      "png_base64: not a nymph image (the PNG carries no nymph source)");
  EXPECT_EQ(error_text(call("extract_chart", {{"png_base64", "aGVsbG8="}})), "png_base64: not a PNG image");
  EXPECT_EQ(error_text(call("extract_chart", {{"png_base64", "not base64!"}})), "png_base64 is not valid base64");
  write("chart.yaml", kSource);
  EXPECT_EQ(
      error_text(call("extract_chart", {{"input_path", path("chart.yaml")}})),
      path("chart.yaml") + ": not a PNG image");
  EXPECT_NE(
      error_text(call("extract_chart", {{"png_base64", kPlainPngBase64}, {"output_path", path("x.png")}}))
          .find("must end in .yaml or .yml"),
      std::string::npos);
}

// ── chart_schema and list_fonts ─────────────────────────────────────────────

TEST_F(mcp_tools, schema_matches_the_cli) {
  EXPECT_EQ(text_of(call("chart_schema", {{"type", "PlotLine"}})), cli_output({"schema", "PlotLine"}));
  EXPECT_EQ(text_of(call("chart_schema")), cli_output({"schema"}));
  const std::string unknown = error_text(call("chart_schema", {{"type", "PlotLyne"}}));
  EXPECT_EQ(unknown.rfind("schema: unknown element type 'PlotLyne' (did you mean 'PlotLine'?) (types: ", 0), 0u)
      << unknown;
}

TEST_F(mcp_tools, fonts_match_the_cli) {
  EXPECT_EQ(text_of(call("list_fonts")), cli_output({"fonts"}));
  EXPECT_NE(error_text(call("list_fonts", {{"all", true}})).find("takes no arguments"), std::string::npos);
}

// ── base64 ──────────────────────────────────────────────────────────────────

TEST(mcp_base64, round_trips_every_length) {
  std::vector<uint8_t> bytes;
  for (int n = 0; n < 40; ++n) {
    const std::string text = base64_encode(bytes);
    EXPECT_EQ(text.size() % 4, 0u);
    EXPECT_EQ(base64_decode(text), bytes);
    bytes.push_back(uint8_t(n * 37 + 11));
  }
  EXPECT_EQ(base64_encode(std::vector<uint8_t>{'M', 'a', 'n'}), "TWFu");
  EXPECT_EQ(base64_decode("TW F\nu"), (std::vector<uint8_t>{'M', 'a', 'n'}));
  EXPECT_EQ(base64_decode("TWE"), (std::vector<uint8_t>{'M', 'a'}));
}

TEST(mcp_base64, rejects_malformed_text) {
  EXPECT_FALSE(base64_decode("TWF*").has_value());
  EXPECT_FALSE(base64_decode("T").has_value());
  EXPECT_FALSE(base64_decode("TW=E").has_value());
  EXPECT_FALSE(base64_decode("TWE==").has_value());
  EXPECT_FALSE(base64_decode("TQ===").has_value());
}

} // namespace
} // namespace bdg::nymph::mcp
