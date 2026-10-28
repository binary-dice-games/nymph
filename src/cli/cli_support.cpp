// MIT License © 2026 Binary Dice Games
/// @file cli_support.cpp
/// @brief File input and output, and the schema and font texts, shared by
///        the command line and the MCP server.
#include "cli/cli_support.hpp"
#include <algorithm>
#include <cctype>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <stdexcept>
#include "cli/schema_doc.hpp"
#include "core/suggest.hpp"
#include "elements/register_builtin.hpp"
#include "fonts/embedded_fonts.hpp"
#include "nymph.hpp"
#include "png/png_meta.hpp"
#include "shapes/shape_catalog.hpp"
#include "edit/source_edit.hpp"
#include "themes/theme_catalog.hpp"

namespace bdg::nymph::embedded {
extern const unsigned char cli_examples[];
extern const std::size_t cli_examples_size;
} // namespace bdg::nymph::embedded

namespace bdg::nymph {

namespace fs = std::filesystem;

namespace {

// ── Examples ────────────────────────────────────────────────────────────────

struct example {
  std::string name;
  std::string source;
  std::string description;        ///< The `description:` value, or empty.
  std::vector<std::string> types; ///< Element types used, in order, once each.
};

bool is_word_char(char c) {
  return std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '_';
}

/// The identifiers that follow `type:` in `source`.
std::vector<std::string> types_of(const std::string& source) {
  std::vector<std::string> types;
  for (std::size_t at = source.find("type:"); at != std::string::npos; at = source.find("type:", at + 5)) {
    if (at > 0 && is_word_char(source[at - 1])) {
      continue; // "image_type:", not "type:"
    }
    std::size_t begin = at + 5;
    while (begin < source.size() && source[begin] == ' ') {
      ++begin;
    }
    std::size_t end = begin;
    while (end < source.size() && is_word_char(source[end])) {
      ++end;
    }
    std::string type = source.substr(begin, end - begin);
    if (!type.empty() && std::find(types.begin(), types.end(), type) == types.end()) {
      types.push_back(std::move(type));
    }
  }
  return types;
}

std::string description_of(const std::string& source) {
  const std::string key = "description:";
  const std::size_t at = source.compare(0, key.size(), key) == 0 ? 0 : source.find("\n" + key);
  if (at == std::string::npos) {
    return {};
  }
  std::size_t begin = source.find(':', at) + 1;
  while (begin < source.size() && source[begin] == ' ') {
    ++begin;
  }
  std::size_t end = source.find('\n', begin);
  std::string value = source.substr(begin, end - begin);
  if (!value.empty() && (value[0] == '>' || value[0] == '|')) {
    // A block scalar: its indented lines, joined by spaces.
    value.clear();
    for (std::size_t line = end + 1; line < source.size() && source[line] == ' ';) {
      end = source.find('\n', line);
      const std::size_t first = source.find_first_not_of(' ', line);
      value += (value.empty() ? "" : " ") + source.substr(first, end - first);
      line = end + 1;
    }
  }
  return value;
}

/// The compiled-in examples: the bundle's "#@@ <name>" sections.
const std::vector<example>& examples() {
  static const std::vector<example> all = [] {
    std::string bundle(reinterpret_cast<const char*>(embedded::cli_examples), embedded::cli_examples_size);
    bundle.erase(std::remove(bundle.begin(), bundle.end(), '\r'), bundle.end()); // checked out with CRLF
    const std::string marker = "#@@ ";
    std::vector<example> found;
    for (std::size_t at = bundle.find(marker); at != std::string::npos;) {
      const std::size_t name_end = bundle.find('\n', at);
      const std::size_t next = bundle.find("\n" + marker, name_end);
      example e;
      e.name = bundle.substr(at + marker.size(), name_end - at - marker.size());
      e.source = bundle.substr(name_end + 1, next == std::string::npos ? next : next + 1 - (name_end + 1));
      e.description = description_of(e.source);
      e.types = types_of(e.source);
      found.push_back(std::move(e));
      at = next == std::string::npos ? next : next + 1;
    }
    return found;
  }();
  return all;
}

/// Writes `data` straight to an existing file that is not a regular file
/// (a device such as /dev/null, a pipe): a rename would replace it.
void write_in_place(const fs::path& target, const std::string& path, const std::string_view data) {
  std::ofstream f(target, std::ios::binary);
  f.write(data.data(), static_cast<std::streamsize>(data.size()));
  f.close();
  if (!f) {
    throw cli_failure{path, "cannot be written"};
  }
}

} // namespace

std::string label_of(const std::string& path) {
  return path == "-" ? "<stdin>" : path;
}

bytes_t read_input(const std::string& path, std::istream& in) {
  constexpr size_t kMaxInputBytes = 512u * 1024u * 1024u;
  std::istream* stream = &in;
  std::ifstream file;
  if (path != "-") {
    std::error_code ec;
    if (!fs::is_regular_file(path, ec)) {
      throw cli_failure{path, fs::exists(path, ec) ? "not a file" : "no such file"};
    }
    file.open(path, std::ios::binary);
    if (!file) {
      throw cli_failure{path, "cannot be read"};
    }
    stream = &file;
  }
  bytes_t data;
  char buf[65536];
  while (stream->read(buf, sizeof(buf)) || stream->gcount() > 0) {
    data.insert(data.end(), buf, buf + stream->gcount());
    if (data.size() > kMaxInputBytes) {
      throw cli_failure{label_of(path), "input is larger than 512 MiB"};
    }
  }
  return data;
}

void write_output(const std::string& path, const std::string_view data, std::ostream& out) {
  if (path == "-") {
    out.write(data.data(), static_cast<std::streamsize>(data.size()));
    out.flush();
    return;
  }
  fs::path target(path);
  std::error_code ec;
  const fs::file_status status = fs::status(target, ec); // follows links
  if (fs::exists(status)) {
    if (!fs::is_regular_file(status)) {
      write_in_place(target, path, data);
      return;
    }
    if (fs::is_symlink(fs::symlink_status(target, ec))) {
      target = fs::canonical(target, ec);
      if (ec) {
        throw cli_failure{path, "cannot be resolved"};
      }
    }
  }
  fs::path temp = target;
  temp += ".nymph-tmp";
  {
    std::ofstream f(temp, std::ios::binary | std::ios::trunc);
    if (!f) {
      throw cli_failure{path, "cannot be written"};
    }
    f.write(data.data(), static_cast<std::streamsize>(data.size()));
    f.close();
    if (!f) {
      std::error_code ignored;
      fs::remove(temp, ignored);
      throw cli_failure{path, "could not be written completely"};
    }
  }
  fs::rename(temp, target, ec);
  if (ec) {
    fs::remove(temp, ec);
    throw cli_failure{path, "cannot be replaced"};
  }
}

std::string_view as_text(const bytes_t& b) {
  return std::string_view(reinterpret_cast<const char*>(b.data()), b.size());
}

std::string source_of(const bytes_t& input, const std::string& path) {
  if (!is_png(input)) {
    return std::string(as_text(input));
  }
  std::optional<std::string> source;
  try {
    source = extract_source(input);
  } catch (const std::runtime_error& e) {
    throw cli_failure{label_of(path), e.what()};
  }
  if (!source) {
    throw cli_failure{label_of(path), "not a nymph image (the PNG carries no nymph source)"};
  }
  return *source;
}

loaded_source load_source_text(std::string_view text) {
  loaded_source loaded;
  if (is_mermaid(text)) {
    mermaid_import imported = import_mermaid(text);
    loaded.source = std::move(imported.source);
    loaded.mermaid_kind = std::move(imported.kind);
    loaded.warnings = std::move(imported.warnings);
  } else {
    loaded.source = std::string(text);
  }
  return loaded;
}

loaded_source load_source(const bytes_t& input, const std::string& path) {
  if (is_png(input)) {
    return {source_of(input, path), {}, {}};
  }
  return load_source_text(as_text(input));
}

std::string warning_text(const std::string& label, const import_warning& w) {
  std::string text = label;
  if (w.line > 0) {
    text += ":" + std::to_string(w.line) + ":" + std::to_string(w.column);
  }
  return text + ": warning: " + w.message;
}

std::string fonts_text() {
  std::string text = std::string(kDefaultFontName) + "\n";
  for (const font_entry& f : font_catalog()) {
    text += std::string(f.name) + (f.data == nullptr ? " (not embedded)" : "") + "\n";
  }
  return text;
}

std::string shapes_listing(const std::string& name) {
  try {
    return shapes_text(name);
  } catch (const std::invalid_argument& e) {
    throw cli_failure{"shapes", e.what()};
  }
}

namespace {

[[noreturn]] void unknown_theme(const std::string& command, const std::string& name) {
  std::vector<std::string> names;
  for (const chart_theme& t : chart_themes()) {
    names.push_back(t.id);
  }
  throw cli_failure{command, "unknown theme '" + name + "'" + did_you_mean(name, names) + " (themes: " +
                                 chart_theme_list() + ")"};
}

} // namespace

std::string themes_listing(const std::string& name) {
  if (!name.empty()) {
    for (const embedded_file& f : embedded_themes()) {
      if (f.name == name) {
        return std::string(f.text());
      }
    }
    unknown_theme("themes", name);
  }
  size_t width = 0;
  for (const chart_theme& t : chart_themes()) {
    width = std::max(width, t.id.size());
  }
  std::string text;
  for (const chart_theme& t : chart_themes()) {
    text += t.id + std::string(width + 2 - t.id.size(), ' ') + t.name + " (" + (t.dark ? "dark" : "light") +
        "): " + t.description + "\n";
  }
  return text;
}

std::string source_with_theme(std::string_view source, const std::string& theme) {
  if (!is_known_theme(theme)) {
    unknown_theme("--theme", theme);
  }
  return set_field(source, "image", "theme", theme);
}

std::string schema_type_text(const std::string& type) {
  const registry& reg = builtin_registry();
  const element_class* cls = reg.find(type);
  if (cls == nullptr) {
    std::vector<std::string> names;
    std::string list;
    for (const element_class* c : classes_in_doc_order(reg)) {
      names.emplace_back(c->name());
      list += (list.empty() ? "" : ", ") + names.back();
    }
    throw cli_failure{
        "schema", "unknown element type '" + type + "'" + did_you_mean(type, names) + " (types: " + list + ")"};
  }
  return schema_text(*cls, reg);
}

std::string example_text(const std::string& query) {
  const std::vector<example>& all = examples();
  if (query.empty()) {
    std::string text;
    for (const example& e : all) {
      text += e.name + ": " + e.description + " [";
      for (std::size_t i = 0; i < e.types.size(); ++i) {
        text += (i == 0 ? "" : ", ") + e.types[i];
      }
      text += "]\n";
    }
    return text;
  }
  for (const example& e : all) {
    if (e.name == query) {
      return e.source;
    }
  }
  // An element type: the smallest example that uses it.
  const example* best = nullptr;
  std::string others;
  for (const example& e : all) {
    if (std::find(e.types.begin(), e.types.end(), query) == e.types.end()) {
      continue;
    }
    if (best == nullptr || e.source.size() < best->source.size()) {
      if (best != nullptr) {
        others += (others.empty() ? "" : ", ") + best->name;
      }
      best = &e;
    } else {
      others += (others.empty() ? "" : ", ") + e.name;
    }
  }
  if (best != nullptr) {
    std::string text = "# example \"" + best->name + "\"" +
                       (others.empty() ? "" : " (others using " + query + ": " + others + ")") + "\n" + best->source;
    return text;
  }
  std::vector<std::string> names;
  std::string list;
  for (const example& e : all) {
    names.push_back(e.name);
    list += (list.empty() ? "" : ", ") + e.name;
    for (const std::string& t : e.types) {
      if (std::find(names.begin(), names.end(), t) == names.end()) {
        names.push_back(t);
      }
    }
  }
  if (builtin_registry().find(query) != nullptr) {
    throw cli_failure{
        "example",
        "no example uses '" + query + "'; its fields are in the schema, and its container's example shows where it "
        "goes (examples: " + list + ")"};
  }
  throw cli_failure{
      "example",
      "no example named or using '" + query + "'" + did_you_mean(query, names) + " (examples: " + list + ")"};
}

} // namespace bdg::nymph
