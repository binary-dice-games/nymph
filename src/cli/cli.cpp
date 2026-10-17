// MIT License © 2026 Binary Dice Games
/// @file cli.cpp
/// @brief run_cli(): argument parsing, file I/O and error reporting around
///        the library API.
#include "cli/cli.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>
#include "cli/schema_doc.hpp"
#include "core/error.hpp"
#include "core/limits.hpp"
#include "elements/register_builtin.hpp"
#include "nymph.hpp"
#include "png/png_meta.hpp"

namespace bdg::nymph {

namespace {

namespace fs = std::filesystem;
using bytes_t = std::vector<uint8_t>;

constexpr const char* kUsage =
    "usage: nymph <in> [-o <out>]                 render YAML to PNG, or print the YAML a nymph PNG carries\n"
    "       nymph render <in> [-o <out>]          render; <in> may be a nymph PNG, or - for stdin (needs -o)\n"
    "       nymph extract <in.png> [-o <out>]     print (or write) the YAML a nymph PNG carries\n"
    "       nymph check <in>                      validate without rendering; silent when valid\n"
    "       nymph schema [<Type>] [--markdown]    element reference\n"
    "       nymph --version | --help\n"
    "render writes <in> with its extension replaced by .png unless -o is given; -o - writes to stdout.\n";

/// A failure to report as `nymph: <file>: <message>`.
struct cli_failure {
  std::string file;
  std::string message;
};

/// Bad arguments: usage and exit 2.
struct usage_error {
  std::string message;
};

struct options {
  std::string command; ///< "", render, extract, check, schema
  std::vector<std::string> positional;
  std::optional<std::string> output;
  bool markdown = false;
};

options parse(std::span<const std::string_view> args) {
  options o;
  size_t i = 0;
  if (!args.empty() && (args[0] == "render" || args[0] == "extract" || args[0] == "check" || args[0] == "schema")) {
    o.command = std::string(args[0]);
    i = 1;
  }
  for (; i < args.size(); ++i) {
    const std::string_view a = args[i];
    if (a == "-o" || a == "--output") {
      if (i + 1 >= args.size()) {
        throw usage_error{"-o needs a file name"};
      }
      o.output = std::string(args[++i]);
    } else if (a == "--markdown" && o.command == "schema") {
      o.markdown = true;
    } else if (a.size() > 1 && a[0] == '-') {
      throw usage_error{"unknown option '" + std::string(a) + "'"};
    } else {
      o.positional.emplace_back(a);
    }
  }
  return o;
}

std::string label_of(const std::string& path) {
  return path == "-" ? "<stdin>" : path;
}

/// Reads `path` ("-" for `in`), refusing more than the source limit plus
/// room for a PNG's pixels (a PNG can be far larger than its source).
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

/// Writes `data` to `path` ("-" for `out`) through a temporary file and a
/// rename, so the target is either the new content or untouched. A
/// symbolic link is followed (the file it points to is replaced, not the
/// link); a device or pipe is written directly.
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

/// The YAML a command works on: the input itself, or the source a nymph PNG
/// carries.
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

std::string png_name_for(const std::string& in) {
  fs::path p(in);
  p.replace_extension(".png");
  return p.string();
}

/// `render`: `input` (YAML, or a nymph PNG whose source is re-rendered) to
/// a PNG at `-o`, or next to the input.
int do_render(const options& o, const std::string& path, const bytes_t& input, std::ostream& out) {
  const std::string source = source_of(input, path);
  std::vector<uint8_t> png;
  try {
    png = render_png(source);
  } catch (const error&) {
    throw; // a problem in the source: reported with its position
  } catch (const std::exception& e) {
    throw cli_failure{label_of(path), e.what()}; // rendering or encoding
  }
  write_output(o.output.value_or(png_name_for(path)), as_text(png), out);
  return 0;
}

/// `extract`: the YAML a nymph PNG carries, to `-o` or stdout.
int do_extract(const options& o, const std::string& path, const bytes_t& input, std::ostream& out) {
  if (!is_png(input)) {
    throw cli_failure{label_of(path), "not a PNG image"};
  }
  write_output(o.output.value_or("-"), source_of(input, path), out);
  return 0;
}

/// `check`: bind and lay out, print nothing.
int do_check(const std::string& path, const bytes_t& input) {
  check_source(source_of(input, path));
  return 0;
}

int do_schema(const options& o, std::ostream& out) {
  if (o.positional.size() > 1 || o.output || (o.markdown && !o.positional.empty())) {
    throw usage_error{"schema takes at most one type name, or --markdown alone"};
  }
  const registry& reg = builtin_registry();
  if (o.markdown) {
    out << schema_markdown(reg);
    return 0;
  }
  if (o.positional.empty()) {
    out << schema_text(reg);
    return 0;
  }
  const element_class* cls = reg.find(o.positional[0]);
  if (cls == nullptr) {
    std::string names;
    for (const element_class* c : classes_in_doc_order(reg)) {
      names += (names.empty() ? "" : ", ") + std::string(c->name());
    }
    throw cli_failure{"schema", "unknown element type '" + o.positional[0] + "' (types: " + names + ")"};
  }
  out << schema_text(*cls, reg);
  return 0;
}

/// The input file of the command, for error messages.
std::string input_label(const options& o) {
  return o.positional.empty() ? "nymph" : label_of(o.positional[0]);
}

} // namespace

int run_cli(std::span<const std::string_view> args, std::istream& in, std::ostream& out, std::ostream& err) {
  for (std::string_view a : args) {
    if (a == "--help" || a == "-h") {
      out << kUsage;
      return 0;
    }
    if (a == "--version") {
      out << "nymph " << version() << "\n";
      return 0;
    }
  }
  options o;
  try {
    o = parse(args);
    if (o.command == "schema") {
      return do_schema(o, out);
    }
    if (o.positional.size() != 1) {
      throw usage_error{o.positional.empty() ? "no input" : "too many inputs"};
    }
    const std::string& path = o.positional[0];
    // Every argument problem is reported before any file is touched.
    if (o.command == "render" && path == "-" && !o.output) {
      throw usage_error{"reading from stdin needs -o <out.png> (or -o - for stdout)"};
    }
    if (o.command == "check" && o.output) {
      throw usage_error{"check takes no -o"};
    }
    const bytes_t input = read_input(path, in);
    if (o.command.empty()) {
      // `nymph <in>`: extract from a PNG, render anything else.
      if (!is_png(input) && path == "-" && !o.output) {
        throw usage_error{"rendering from stdin needs -o <out.png> (or -o - for stdout)"};
      }
      o.command = is_png(input) ? "extract" : "render";
    }
    if (o.command == "render") {
      return do_render(o, path, input, out);
    }
    if (o.command == "extract") {
      return do_extract(o, path, input, out);
    }
    return do_check(path, input);
  } catch (const usage_error& e) {
    err << "nymph: " << e.message << "\n" << kUsage;
    return 2;
  } catch (const cli_failure& e) {
    err << "nymph: " << e.file << ": " << e.message << "\n";
  } catch (const error& e) {
    err << "nymph: " << input_label(o);
    if (e.line() > 0) {
      err << ":" << e.line() << ":" << e.column();
    }
    err << ": " << e.what() << "\n";
  } catch (const std::exception& e) {
    err << "nymph: " << input_label(o) << ": " << e.what() << "\n";
  }
  return 1;
}

} // namespace bdg::nymph
