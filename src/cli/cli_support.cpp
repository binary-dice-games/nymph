// MIT License © 2026 Binary Dice Games
/// @file cli_support.cpp
/// @brief File input and output, and the schema and font texts, shared by
///        the command line and the MCP server.
#include "cli/cli_support.hpp"
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

namespace bdg::nymph {

namespace fs = std::filesystem;

namespace {

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

} // namespace bdg::nymph
