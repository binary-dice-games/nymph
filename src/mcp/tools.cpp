// MIT License © 2026 Binary Dice Games
/// @file tools.cpp
/// @brief The built-in tools of `nymph mcp` and the registry that runs them.
#include "mcp/tools.hpp"
#include <algorithm>
#include <cctype>
#include <exception>
#include <filesystem>
#include <initializer_list>
#include <optional>
#include <sstream>
#include <string_view>
#include <utility>
#include "cli/cli_support.hpp"
#include "cli/schema_doc.hpp"
#include "core/error.hpp"
#include "core/suggest.hpp"
#include "elements/register_builtin.hpp"
#include "fonts/embedded_fonts.hpp"
#include "mcp/base64.hpp"
#include "mcp/resources.hpp"
#include "png/png_meta.hpp"

namespace bdg::nymph::mcp {

namespace {

namespace fs = std::filesystem;

// ── Arguments ───────────────────────────────────────────────────────────────

/// Refuses arguments a tool does not take, with the closest name.
void check_known(const json& args, std::initializer_list<std::string_view> names) {
  for (const auto& [key, value] : args.items()) {
    if (std::find(names.begin(), names.end(), key) == names.end()) {
      std::string list;
      for (std::string_view n : names) {
        list += (list.empty() ? "" : ", ") + std::string(n);
      }
      throw tool_error(
          "unknown argument '" + key + "'" + did_you_mean(key, names) +
          (list.empty() ? " (this tool takes no arguments)" : " (arguments: " + list + ")"));
    }
  }
}

std::optional<std::string> optional_string(const json& args, const char* name) {
  const auto it = args.find(name);
  if (it == args.end() || it->is_null()) {
    return std::nullopt;
  }
  if (!it->is_string()) {
    throw tool_error(std::string("argument '") + name + "' must be a string");
  }
  return it->get<std::string>();
}

/// Exactly one of two arguments.
void check_one_of(const json& args, const char* a, const char* b) {
  const bool has_a = optional_string(args, a).has_value();
  const bool has_b = optional_string(args, b).has_value();
  if (has_a == has_b) {
    throw tool_error(std::string("give exactly one of '") + a + "' and '" + b + "'");
  }
}

/// An input file named by the agent; never "-", which would be the
/// protocol's own standard input.
std::string checked_input_path(const std::string& path) {
  if (path.empty() || path == "-") {
    throw tool_error("input_path must name a file");
  }
  return path;
}

std::string lower(std::string text) {
  std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return char(std::tolower(c)); });
  return text;
}

/// An output file, which must end in one of `extensions` (DESIGN.md
/// section 2, goal 5: the server writes only charts and their sources).
std::optional<std::string> checked_output_path(const json& args, std::initializer_list<std::string_view> extensions) {
  const std::optional<std::string> path = optional_string(args, "output_path");
  if (!path) {
    return std::nullopt;
  }
  const std::string extension = lower(fs::path(*path).extension().string());
  if (std::find(extensions.begin(), extensions.end(), extension) == extensions.end()) {
    std::string list;
    for (std::string_view e : extensions) {
      list += (list.empty() ? "" : " or ") + std::string(e);
    }
    throw tool_error("output_path must end in " + list + ": " + *path);
  }
  return path;
}

std::string absolute_of(const std::string& path) {
  std::error_code ec;
  const fs::path p = fs::absolute(path, ec);
  return ec ? path : p.lexically_normal().string();
}

void write_file(const std::string& path, std::string_view data) {
  std::ostringstream unused; // write_output() writes here only for "-"
  write_output(path, data, unused);
}

// ── Charts ──────────────────────────────────────────────────────────────────

/// The YAML a tool works on and its name in messages.
struct chart_input {
  std::string source;
  std::string label; ///< the input path, or "source" for inline text
};

chart_input load_chart(const json& args) {
  check_one_of(args, "source", "input_path");
  if (const std::optional<std::string> source = optional_string(args, "source")) {
    return {*source, "source"};
  }
  const std::string path = checked_input_path(*optional_string(args, "input_path"));
  std::istringstream unused; // read_input() reads it only for "-"
  const bytes_t bytes = read_input(path, unused);
  return {source_of(bytes, path), path};
}

/// `<label>:<line>:<column>: <message>`, as the CLI prints it.
std::string describe(const error& e, const std::string& label) {
  std::string text = label;
  if (e.line() > 0) {
    text += ":" + std::to_string(e.line()) + ":" + std::to_string(e.column());
  }
  return text + ": " + e.what();
}

/// Width and height from a PNG's IHDR chunk.
std::pair<uint32_t, uint32_t> png_size(const std::vector<uint8_t>& png) {
  auto be32 = [&](size_t at) {
    return (uint32_t(png[at]) << 24) | (uint32_t(png[at + 1]) << 16) | (uint32_t(png[at + 2]) << 8) | png[at + 3];
  };
  return png.size() >= 24 ? std::pair{be32(16), be32(20)} : std::pair{0u, 0u};
}

rasterizer rasterizer_of(const json& args, const server_options& options, bool& explicit_choice) {
  const std::optional<std::string> name = optional_string(args, "rasterizer");
  explicit_choice = options.raster_explicit || name.has_value();
  if (!name) {
    return options.raster;
  }
  if (*name == "software") {
    return rasterizer::software;
  }
  if (*name == "opengl") {
    return rasterizer::opengl;
  }
  throw tool_error("unknown rasterizer '" + *name + "' (rasterizers: software, opengl)");
}

// ── Tools ───────────────────────────────────────────────────────────────────

json chart_input_properties() {
  return {
      {"source", {{"type", "string"}, {"description", "The chart's YAML source (nymph format)."}}},
      {"input_path",
       {{"type", "string"},
        {"description",
         "Instead of source: a .yaml file, or a nymph .png (its embedded source is used). Relative to the server's "
         "working directory."}}},
  };
}

json render_chart(tool_registry& registry, const json& args) {
  check_known(args, {"source", "input_path", "output_path", "rasterizer"});
  const std::optional<std::string> output = checked_output_path(args, {".png"});
  bool explicit_choice = false;
  const rasterizer wanted = rasterizer_of(args, registry.options(), explicit_choice);
  const chart_input input = load_chart(args);

  std::vector<uint8_t> png;
  json content = json::array();
  try {
    renderer& r = registry.renderer_for(wanted);
    png = r.render_png(input.source);
    if (explicit_choice && wanted == rasterizer::opengl && r.last_used() != rasterizer::opengl) {
      content.push_back(json{
          {"type", "text"},
          {"text", "warning: OpenGL unavailable (" + r.fallback_reason() + "); used the software rasterizer"}});
    }
  } catch (const error& e) {
    throw tool_error(describe(e, input.label));
  } catch (const std::exception& e) {
    throw tool_error(input.label + ": " + e.what()); // rendering or encoding
  }

  const auto [width, height] = png_size(png);
  json structured = {{"source", input.source}, {"width", width}, {"height", height}};
  std::string summary = "Rendered " + std::to_string(width) + "x" + std::to_string(height) + " PNG";
  if (output) {
    write_file(*output, as_text(png));
    structured["path"] = absolute_of(*output);
    summary += ", written to " + structured["path"].get<std::string>();
  }
  std::string image = base64_encode(png);
  if (image.size() > registry.options().max_image_base64) {
    summary += " (the image is too large to return here: " + std::to_string(png.size()) + " bytes" +
        (output ? "; open the file" : "; give output_path to write it to a file") + ")";
  } else {
    content.insert(content.begin(), json{{"type", "image"}, {"data", std::move(image)}, {"mimeType", "image/png"}});
  }
  content.push_back(json{{"type", "text"}, {"text", summary}});
  return {{"content", std::move(content)}, {"structuredContent", std::move(structured)}};
}

json check_chart(const json& args) {
  check_known(args, {"source", "input_path"});
  const chart_input input = load_chart(args);
  try {
    check_source(input.source);
  } catch (const error& e) {
    throw tool_error(describe(e, input.label));
  }
  return text_result("valid");
}

json extract_chart(const json& args) {
  check_known(args, {"input_path", "png_base64", "output_path"});
  check_one_of(args, "input_path", "png_base64");
  const std::optional<std::string> output = checked_output_path(args, {".yaml", ".yml"});
  bytes_t png;
  std::string label = "png_base64";
  if (const std::optional<std::string> text = optional_string(args, "png_base64")) {
    std::optional<std::vector<uint8_t>> decoded = base64_decode(*text);
    if (!decoded) {
      throw tool_error("png_base64 is not valid base64");
    }
    png = std::move(*decoded);
  } else {
    label = checked_input_path(*optional_string(args, "input_path"));
    std::istringstream unused;
    png = read_input(label, unused);
  }
  if (!is_png(png)) {
    throw tool_error(label + ": not a PNG image");
  }
  const std::string source = source_of(png, label);
  json result = text_result(source);
  if (output) {
    write_file(*output, source);
    result["content"].push_back(json{{"type", "text"}, {"text", "written to " + absolute_of(*output)}});
  }
  return result;
}

json chart_schema(const json& args) {
  check_known(args, {"type"});
  const std::optional<std::string> type = optional_string(args, "type");
  return text_result(type ? schema_type_text(*type) : schema_text(builtin_registry()));
}

json list_fonts(const json& args) {
  check_known(args, {});
  return text_result(fonts_text());
}

/// The App's fonts: the bytes of an embedded catalog font, by file name.
json viewer_font(const json& args) {
  check_known(args, {"name"});
  const std::optional<std::string> name = optional_string(args, "name");
  if (!name) {
    throw tool_error("viewer_font needs 'name'");
  }
  const font_entry* font = find_font(*name);
  if (font == nullptr) {
    throw tool_error(check_font_name(*name));
  }
  if (font->data == nullptr) {
    throw tool_error("font '" + *name + "' is not embedded in this nymph");
  }
  return {
      {"content",
       json::array({json{{"type", "text"}, {"text", *name + ", " + std::to_string(font->size) + " bytes"}}})},
      {"structuredContent", {{"name", *name}, {"base64", base64_encode(std::span(font->data, font->size))}}}};
}

json annotations(const char* title, bool read_only) {
  json a = {{"title", title}, {"readOnlyHint", read_only}, {"openWorldHint", false}};
  if (!read_only) {
    a["destructiveHint"] = false; // writes only output_path, a new chart or source
    a["idempotentHint"] = true;
  }
  return a;
}

} // namespace

// ── Registry ────────────────────────────────────────────────────────────────

json text_result(const std::string& text, bool error) {
  json result = {{"content", json::array({{{"type", "text"}, {"text", text}}})}};
  if (error) {
    result["isError"] = true;
  }
  return result;
}

tool_registry::tool_registry(const server_options& options) : options_(options) {}

tool_registry::~tool_registry() = default;

void tool_registry::add(tool t) {
  const json& name = t.definition.at("name");
  const auto same =
      std::find_if(tools_.begin(), tools_.end(), [&](const tool& x) { return x.definition.at("name") == name; });
  if (same != tools_.end()) {
    *same = std::move(t);
  } else {
    tools_.push_back(std::move(t));
  }
}

tool* tool_registry::find(const std::string& name) {
  const auto it =
      std::find_if(tools_.begin(), tools_.end(), [&](const tool& t) { return t.definition.at("name") == name; });
  return it == tools_.end() ? nullptr : &*it;
}

json tool_registry::list() const {
  json list = json::array();
  for (const tool& t : tools_) {
    list.push_back(t.definition);
  }
  return {{"tools", std::move(list)}};
}

json tool_registry::call(const json& params) {
  if (!params.is_object() || !params.contains("name") || !params["name"].is_string()) {
    throw rpc_error(rpc_code::invalid_params, "tools/call needs \"name\" (a string)");
  }
  const std::string name = params["name"].get<std::string>();
  const json arguments = params.value("arguments", json::object());
  if (!arguments.is_object()) {
    throw rpc_error(rpc_code::invalid_params, "\"arguments\" must be an object");
  }
  const auto it =
      std::find_if(tools_.begin(), tools_.end(), [&](const tool& t) { return t.definition.at("name") == name; });
  if (it == tools_.end()) {
    std::vector<std::string> names;
    for (const tool& t : tools_) {
      names.push_back(t.definition.at("name").get<std::string>());
    }
    throw rpc_error(rpc_code::invalid_params, "unknown tool '" + name + "'" + did_you_mean(name, names));
  }
  try {
    return it->handler(arguments);
  } catch (const tool_error& e) {
    return text_result(e.what(), true);
  } catch (const cli_failure& e) {
    return text_result(e.file + ": " + e.message, true);
  } catch (const error& e) {
    return text_result(describe(e, name), true);
  } catch (const std::exception& e) {
    return text_result(std::string("internal error: ") + e.what(), true);
  }
}

renderer& tool_registry::renderer_for(rasterizer r) {
  std::unique_ptr<renderer>& slot = renderers_[r];
  if (!slot) {
    slot = std::make_unique<renderer>(make_renderer(r, options_.gl_options));
  }
  return *slot;
}

void add_builtin_tools(tool_registry& registry) {
  json render_properties = chart_input_properties();
  render_properties["output_path"] = {
      {"type", "string"},
      {"description",
       "Also write the PNG to this file (must end in .png); relative to the server's working directory."}};
  render_properties["rasterizer"] = {
      {"type", "string"},
      {"enum", {"software", "opengl"}},
      {"description",
       "software: the same bytes on every machine for one nymph build. opengl: the GPU when there is one (falls back to "
       "software). "
       "Default: the server's."}};
  registry.add(
      {{{"name", "render_chart"},
        {"title", "Render chart"},
        {"description",
         "Render a nymph chart from its YAML source to a PNG, and return the image. Plots (lines, bars, scatter, "
         "heatmaps, 3D), UML diagrams, flowcharts, Gantt and Sankey charts, laid out with labels. A minimal "
         "source:\n"
         "root:\n  type: Plot\n  title: Revenue\n  children:\n    - {type: PlotLine, label: revenue, ys: "
         "$revenue}\ndata: {revenue: [12, 15, 11, 18]}\n"
         "On an error the result says <line>:<column> in the source: fix that spot and call again. Use "
         "chart_schema for an element's fields. The PNG carries its source (extract_chart reads it back)."},
        {"inputSchema",
         {{"type", "object"}, {"properties", std::move(render_properties)}, {"additionalProperties", false}}},
        {"outputSchema",
         {{"type", "object"},
          {"properties",
           {{"source", {{"type", "string"}}},
            {"width", {{"type", "integer"}}},
            {"height", {{"type", "integer"}}},
            {"path", {{"type", "string"}}}}},
          {"required", {"source", "width", "height"}}}},
        {"annotations", annotations("Render chart", false)}},
       [&registry](const json& args) { return render_chart(registry, args); }});

  registry.add(
      {{{"name", "check_chart"},
        {"title", "Check chart"},
        {"description",
         "Validate a nymph chart source without drawing it: everything render_chart checks, including whether the "
         "layout fits. Returns 'valid', or the first problem as <line>:<column>: <message>."},
        {"inputSchema",
         {{"type", "object"}, {"properties", chart_input_properties()}, {"additionalProperties", false}}},
        {"annotations", annotations("Check chart", true)}},
       check_chart});

  registry.add(
      {{{"name", "extract_chart"},
        {"title", "Extract chart source"},
        {"description",
         "Return the YAML source a nymph PNG carries, to change and render it again. If it has a 'source' key, it "
         "says how the data was made."},
        {"inputSchema",
         {{"type", "object"},
          {"properties",
           {{"input_path", {{"type", "string"}, {"description", "A nymph .png file."}}},
            {"png_base64", {{"type", "string"}, {"description", "Instead of input_path: the PNG's bytes in base64."}}},
            {"output_path",
             {{"type", "string"}, {"description", "Also write the YAML to this file (must end in .yaml or .yml)."}}}}},
          {"additionalProperties", false}}},
        {"annotations", annotations("Extract chart source", false)}},
       extract_chart});

  registry.add(
      {{{"name", "chart_schema"},
        {"title", "Chart element schema"},
        {"description",
         "The fields of one nymph element type (names, types, defaults, allowed values), or of every type when "
         "'type' is omitted (long)."},
        {"inputSchema",
         {{"type", "object"},
          {"properties", {{"type", {{"type", "string"}, {"description", "An element type, e.g. PlotLine."}}}}},
          {"additionalProperties", false}}},
        {"annotations", annotations("Chart element schema", true)}},
       chart_schema});

  registry.add(
      {{{"name", "list_fonts"},
        {"title", "List fonts"},
        {"description", "The font names a chart's 'font' field accepts, one per line."},
        {"inputSchema", {{"type", "object"}, {"properties", json::object()}, {"additionalProperties", false}}},
        {"annotations", annotations("List fonts", true)}},
       list_fonts});

  if (registry.options().viewer_html.empty()) {
    return;
  }
  // The App shows render_chart's result; it is also a plain tool for the model.
  registry.find("render_chart")->definition["_meta"] = {
      {"ui", {{"resourceUri", kViewerUri}, {"visibility", {"model", "app"}}}}};
  registry.add(
      {{{"name", "viewer_font"},
        {"title", "Viewer font"},
        {"description", "The bytes of a font the chart viewer needs (used by the viewer, not the model)."},
        {"inputSchema",
         {{"type", "object"},
          {"properties",
           {{"name", {{"type", "string"}, {"description", "A font file name, e.g. FiraSans-Regular.ttf."}}}}},
          {"required", {"name"}},
          {"additionalProperties", false}}},
        {"annotations", annotations("Viewer font", true)},
        {"_meta", {{"ui", {{"visibility", {"app"}}}}}}},
       viewer_font});
}

} // namespace bdg::nymph::mcp
