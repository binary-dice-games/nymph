// MIT License © 2026 Binary Dice Games
/// @file view_command.cpp
/// @brief `nymph view`: argument parsing, input checks and error reporting
///        around the window.
#include "window/view_command.hpp"
#include <algorithm>
#include <filesystem>
#include <iostream>
#include <sstream>
#include "cli/cli_support.hpp"
#include "core/error.hpp"
#include "render/image_options.hpp"
#include "themes/theme_catalog.hpp"
#include "window/viewer_window.hpp"

namespace bdg::nymph {

namespace {

constexpr const char* kViewUsage =
    "usage: nymph view <in> [--theme <theme>] [--fit source|width|fill] [--edit [--no-mcp]]\n"
    "  opens <in> (a YAML file or a nymph PNG) in a window: drag to pan, wheel to zoom,\n"
    "  hover for values, double-click to fit. Keys: R resets the view, S saves the image on\n"
    "  screen as <in>.view.png, Y saves its YAML as <in>.view.yaml, Esc or Q closes.\n"
    "  --edit (or the E key) opens the editor panel: click a text or an element in the\n"
    "  figure, change its text, font, size, colour or any other field; Undo and Redo.\n"
    "  Right-click an element (in the figure or the tree) to add, move or delete elements;\n"
    "  the tabs over the figure edit the data tables. While editing, AI agents using\n"
    "  `nymph mcp` can read and change the chart (editor_* tools); --no-mcp turns that off.\n";

constexpr int kMinWindowWidth = 240;
constexpr int kMinWindowHeight = 160;

} // namespace

const char* view_usage() {
  return kViewUsage;
}

view_args parse_view_args(std::span<const std::string_view> args) {
  view_args o;
  bool have_input = false;
  for (size_t i = 0; i < args.size(); ++i) {
    const std::string_view a = args[i];
    if (a == "--theme") {
      if (i + 1 >= args.size()) {
        throw view_usage_error{"--theme needs a theme (" + chart_theme_list() + ")"};
      }
      const std::string_view value = args[++i];
      if (!is_known_theme(value)) {
        throw view_usage_error{"unknown theme '" + std::string(value) + "' (themes: " + chart_theme_list() + ")"};
      }
      o.theme = std::string(value);
    } else if (a == "--edit") {
      o.edit = true;
    } else if (a == "--no-mcp") {
      o.no_mcp = true;
    } else if (a == "--fit") {
      if (i + 1 >= args.size()) {
        throw view_usage_error{"--fit needs source, width or fill"};
      }
      const std::string_view value = args[++i];
      if (value == "source") {
        o.fit = view_fit::source;
      } else if (value == "width") {
        o.fit = view_fit::width;
      } else if (value == "fill") {
        o.fit = view_fit::fill;
      } else {
        throw view_usage_error{"unknown fit '" + std::string(value) + "' (fits: source, width, fill)"};
      }
    } else if (a == "-") {
      throw view_usage_error{"view needs a file, not standard input"};
    } else if (a.size() > 1 && a[0] == '-') {
      throw view_usage_error{"unknown option '" + std::string(a) + "'"};
    } else if (have_input) {
      throw view_usage_error{"too many inputs"};
    } else {
      o.input = std::string(a);
      have_input = true;
    }
  }
  if (!have_input) {
    throw view_usage_error{"no input"};
  }
  return o;
}

view_request prepare_view(const view_args& args) {
  std::istringstream no_stdin;
  const bytes_t input = read_input(args.input, no_stdin);
  view_request r;
  r.args = args;
  loaded_source loaded = load_source(input, args.input);
  r.source = std::move(loaded.source);
  r.mermaid_kind = std::move(loaded.mermaid_kind);
  for (const import_warning& w : loaded.warnings) {
    std::cerr << "nymph: " << warning_text(label_of(args.input), w) << "\n";
  }
  // Bind and lay the source out the way the window will, so that every
  // problem is reported before a window opens; the same view says how big
  // the layout is.
  live_view probe;
  probe.set_source(r.source);
  probe.set_theme(args.theme);
  probe.resize(view_size{0.0f, 0.0f, 1.0f});
  const view_geometry g = probe.geometry();
  r.width = static_cast<int>(g.layout_width);
  r.height = static_cast<int>(g.layout_height);
  return r;
}

window_size initial_window_size(int width, int height, int available_width, int available_height) {
  window_size s{width, height};
  if (available_width > 0) {
    s.width = std::min(s.width, available_width);
  }
  if (available_height > 0) {
    s.height = std::min(s.height, available_height);
  }
  s.width = std::max(s.width, std::min(kMinWindowWidth, available_width > 0 ? available_width : kMinWindowWidth));
  s.height = std::max(s.height, std::min(kMinWindowHeight, available_height > 0 ? available_height : kMinWindowHeight));
  return s;
}

std::string view_save_path(const std::string& input, std::string_view extension) {
  namespace fs = std::filesystem;
  const fs::path in(input);
  fs::path out = in;
  out.replace_filename(in.stem().string() + ".view." + std::string(extension));
  return out.string();
}

std::string edit_save_path(const std::string& input, bool from_mermaid) {
  if (!from_mermaid) {
    return input;
  }
  namespace fs = std::filesystem;
  fs::path out(input);
  out.replace_extension(".yaml");
  if (out == fs::path(input)) {
    out.replace_extension(".nymph.yaml"); // mermaid in a .yaml file: never over it
  }
  return out.string();
}

int run_view_cli(std::span<const std::string_view> args, std::ostream& err) {
  for (const std::string_view a : args) {
    if (a == "--help" || a == "-h") {
      std::cout << kViewUsage;
      return 0;
    }
  }
  std::string label = "nymph view";
  try {
    const view_args parsed = parse_view_args(args);
    label = label_of(parsed.input);
    show_window(prepare_view(parsed));
    return 0;
  } catch (const view_usage_error& e) {
    err << "nymph: " << e.message << "\n" << kViewUsage;
    return 2;
  } catch (const cli_failure& e) {
    err << "nymph: " << e.file << ": " << e.message << "\n";
  } catch (const error& e) {
    err << "nymph: " << label;
    if (e.line() > 0) {
      err << ":" << e.line() << ":" << e.column();
    }
    err << ": " << e.what() << "\n";
  } catch (const window_error& e) {
    err << "nymph: " << e.what() << " (nymph render draws without a window)\n";
  } catch (const std::exception& e) {
    err << "nymph: " << label << ": " << e.what() << "\n";
  }
  return 1;
}

} // namespace bdg::nymph
