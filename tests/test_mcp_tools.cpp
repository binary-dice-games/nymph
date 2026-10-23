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
#include "fonts/embedded_fonts.hpp"
#include "mcp/base64.hpp"
#include "mcp/editor_tools.hpp"
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
  // The chart tools, then the tools of a running editor (spec/14_editor_workspace).
  std::vector<std::string> expected{
      "render_chart", "check_chart", "convert_chart", "extract_chart", "chart_schema", "example_chart", "list_fonts",
      "list_shapes"};
  for (const json& t : editor_tool_definitions()) {
    expected.push_back(t["name"]);
  }
  EXPECT_EQ(names, expected);
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

TEST_F(mcp_tools, render_names_the_path_it_cannot_write) {
  const std::string absolute = (dir_ / "no_such_dir" / "out.png").string();
  const std::string absolute_error = error_text(call("render_chart", {{"source", kSource}, {"output_path", absolute}}));
  EXPECT_EQ(absolute_error, absolute + ": cannot be written");
  const std::string relative_error =
      error_text(call("render_chart", {{"source", kSource}, {"output_path", "no_such_dir/out.png"}}));
  EXPECT_NE(relative_error.find("out.png: cannot be written"), std::string::npos) << relative_error;
  EXPECT_NE(relative_error.find("give an absolute path"), std::string::npos) << relative_error;
  EXPECT_TRUE(fs::path(relative_error.substr(0, relative_error.find(": cannot"))).is_absolute()) << relative_error;
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

TEST_F(mcp_tools, examples_match_the_cli_and_render) {
  EXPECT_EQ(text_of(call("example_chart")), cli_output({"example"}));
  EXPECT_EQ(text_of(call("example_chart", {{"type", "PlotHeatmap"}})), cli_output({"example", "PlotHeatmap"}));
  // By element type or by name, the source is complete: it renders.
  for (const char* query : {"PlotHeatmap", "ClassDiagram", "Window", "GanttChart", "uml_state"}) {
    const std::string text = text_of(call("example_chart", {{"type", query}}));
    EXPECT_FALSE(call("render_chart", {{"source", text}}).value("isError", false)) << query;
  }
  const std::string unknown = error_text(call("example_chart", {{"type", "PlotHeatmapp"}}));
  EXPECT_NE(unknown.find("(did you mean 'PlotHeatmap'?)"), std::string::npos) << unknown;
}

TEST_F(mcp_tools, fonts_match_the_cli) {
  EXPECT_EQ(text_of(call("list_fonts")), cli_output({"fonts"}));
  EXPECT_NE(error_text(call("list_fonts", {{"all", true}})).find("takes no arguments"), std::string::npos);
}

TEST_F(mcp_tools, shapes_match_the_cli) {
  EXPECT_EQ(text_of(call("list_shapes")), cli_output({"shapes"}));
  EXPECT_EQ(text_of(call("list_shapes", {{"name", "cloud"}})), cli_output({"shapes", "cloud"}));
  const std::string unknown = error_text(call("list_shapes", {{"name", "clod"}}));
  EXPECT_NE(unknown.find("unknown shape 'clod' (did you mean 'cloud'?)"), std::string::npos) << unknown;
  EXPECT_NE(error_text(call("list_shapes", {{"all", true}})).find("unknown"), std::string::npos);
}

// ── The MCP App (step 5) ────────────────────────────────────────────────────

/// A stand-in App page, with a NUL like the real one (its wasm as text).
const std::string kViewerHtml = std::string("<!doctype html><p>viewer\0</p>", 30);

json rpc(session& s, const std::string& method, const json& params) {
  rpc_message m;
  m.id = 99;
  m.method = method;
  m.params = params;
  return *s.handle(m);
}

TEST_F(mcp_tools, without_the_app_there_is_no_ui) {
  const json tools = rpc(*session_, "tools/list", json::object())["result"]["tools"];
  for (const json& t : tools) {
    EXPECT_FALSE(t.contains("_meta")) << t["name"];
    EXPECT_NE(t["name"], "viewer_font");
  }
  const json resources = rpc(*session_, "resources/list", json::object())["result"]["resources"];
  for (const json& r : resources) {
    EXPECT_NE(r["uri"], "ui://nymph/viewer");
  }
  EXPECT_EQ(rpc(*session_, "resources/read", {{"uri", "ui://nymph/viewer"}})["error"]["code"], -32002);
}

TEST_F(mcp_tools, the_app_is_a_resource_linked_from_render_chart) {
  server_options o;
  o.viewer_html = kViewerHtml;
  start(o);
  const json tools = rpc(*session_, "tools/list", json::object())["result"]["tools"];
  ASSERT_EQ(tools[0]["name"], "render_chart");
  EXPECT_EQ(tools[0]["_meta"]["ui"]["resourceUri"], "ui://nymph/viewer");
  EXPECT_EQ(tools[0]["_meta"]["ui"]["visibility"], json({"model", "app"}));
  // The chart tools end with the app-only viewer_font; the editor's follow.
  const json& font = tools[tools.size() - 1 - editor_tool_definitions().size()];
  ASSERT_EQ(font["name"], "viewer_font");
  EXPECT_EQ(font["_meta"]["ui"]["visibility"], json({"app"}));

  const json resources = rpc(*session_, "resources/list", json::object())["result"]["resources"];
  ASSERT_EQ(resources.size(), 3u);
  EXPECT_EQ(resources[0]["uri"], "ui://nymph/viewer");
  EXPECT_EQ(resources[0]["mimeType"], "text/html;profile=mcp-app");

  const json content = rpc(*session_, "resources/read", {{"uri", "ui://nymph/viewer"}})["result"]["contents"][0];
  EXPECT_EQ(content["mimeType"], "text/html;profile=mcp-app");
  EXPECT_EQ(content["text"].get<std::string>(), kViewerHtml);
  EXPECT_EQ(content["_meta"]["ui"]["prefersBorder"], true);
  EXPECT_EQ(content["_meta"]["ui"]["csp"], json::object());
}

TEST_F(mcp_tools, viewer_font_returns_an_embedded_font) {
  server_options o;
  o.viewer_html = kViewerHtml;
  start(o);
  const font_entry* font = nullptr;
  for (const font_entry& f : font_catalog()) {
    if (f.data != nullptr) {
      font = &f;
      break;
    }
  }
  ASSERT_NE(font, nullptr) << "the native build embeds the fonts";
  const json result = call("viewer_font", {{"name", font->name}});
  ASSERT_FALSE(result.value("isError", false)) << result.dump().substr(0, 300);
  EXPECT_EQ(result["structuredContent"]["name"], font->name);
  const auto bytes = base64_decode(result["structuredContent"]["base64"].get<std::string>());
  ASSERT_TRUE(bytes.has_value());
  EXPECT_EQ(*bytes, std::vector<uint8_t>(font->data, font->data + font->size));

  EXPECT_NE(error_text(call("viewer_font", {{"name", "FiraSans-Regular.tt"}})).find("did you mean"), std::string::npos);
  EXPECT_EQ(error_text(call("viewer_font")), "viewer_font needs 'name'");
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


// ── Mermaid (spec/16_mermaid_import/PLAN.md step 4) ─────────────────────────

const char* const kMermaid =
    "flowchart LR\n"
    "  A[Start] --> B{Ok?}\n"
    "  B -->|yes| C\n"
    "  click A callback\n";

TEST_F(mcp_tools, render_takes_mermaid_and_returns_the_converted_yaml) {
  start(software());
  const json result = call("render_chart", {{"source", kMermaid}});
  const std::string text = text_of(result);
  EXPECT_EQ(result["content"][0]["type"], "image");
  EXPECT_NE(text.find("Converted from mermaid (flowchart); convert_chart gives the nymph YAML."), std::string::npos)
      << text;
  EXPECT_NE(text.find("source:4:3: warning: click is ignored"), std::string::npos) << text;
  // The App draws structuredContent.source: the YAML, never the mermaid.
  const std::string source = result["structuredContent"]["source"];
  EXPECT_EQ(source, import_mermaid(kMermaid).source);
  EXPECT_EQ(text_of(call("convert_chart", {{"source", kMermaid}})).rfind(source, 0), 0u);
}

TEST_F(mcp_tools, mermaid_errors_point_at_the_mermaid) {
  write("bad.mmd", "flowchart LR\n  A --> B@{ shape: blob }\n");
  EXPECT_EQ(error_text(call("check_chart", {{"input_path", path("bad.mmd")}})), cli_error({"check", path("bad.mmd")}));
  EXPECT_EQ(
      error_text(call("render_chart", {{"source", "flowchart LR\n  A --> B@{ shape: blob }\n"}})).rfind("source:2:10: ", 0),
      0u);
}

TEST_F(mcp_tools, check_reports_the_mermaid_kind) {
  const std::string text = text_of(call("check_chart", {{"source", "sequenceDiagram\n  A->>B: hi\n"}}));
  EXPECT_EQ(text, "valid\nConverted from mermaid (sequenceDiagram).");
}

TEST_F(mcp_tools, convert_writes_only_yaml_and_reads_every_input) {
  write("flow.mmd", kMermaid);
  const json written = call("convert_chart", {{"input_path", path("flow.mmd")}, {"output_path", path("flow.yaml")}});
  EXPECT_EQ(read_file(dir_ / "flow.yaml"), cli_output({"convert", path("flow.mmd")}));
  EXPECT_NE(text_of(written).find("written to "), std::string::npos);
  for (const char* bad : {"flow.png", "flow.mmd"}) {
    EXPECT_EQ(
        error_text(call("convert_chart", {{"source", kMermaid}, {"output_path", path(bad)}})).rfind("output_path must end in", 0),
        0u)
        << bad;
  }
  // A YAML source is checked and returned; a nymph PNG gives its source.
  EXPECT_EQ(text_of(call("convert_chart", {{"source", kSource}})), kSource);
  write("chart.png", as_string(render_png(kSource)));
  EXPECT_EQ(text_of(call("convert_chart", {{"input_path", path("chart.png")}})), kSource);
  EXPECT_FALSE(error_text(call("convert_chart", {{"source", kBadSource}})).empty());
}

} // namespace
} // namespace bdg::nymph::mcp
