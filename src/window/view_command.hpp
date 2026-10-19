// MIT License © 2026 Binary Dice Games
/// @file view_command.hpp
/// @brief `nymph view`: arguments, input checks and the command itself
///        (spec/04_web_viewer/DESIGN.md section 10).
#pragma once

#include <iosfwd>
#include <span>
#include <string>
#include <string_view>
#include "view/live_view.hpp"

namespace bdg::nymph {

/// @brief Bad arguments of `nymph view`: reported with the usage, exit 2.
struct view_usage_error {
  std::string message;
};

/// @brief The parsed command line of `nymph view`.
struct view_args {
  std::string input; ///< A YAML file or a nymph PNG.
  std::string theme; ///< "light", "dark", "classic", or "" for the source's own.
  bool edit = false; ///< `--edit`: open with the editor panel (the E key shows it too).
  view_fit fit = view_fit::fill; ///< How the layout follows the window.
};

/// @brief What `nymph view` shows, checked: ready for a window.
struct view_request {
  view_args args;
  std::string source; ///< The YAML (the one a nymph PNG carries, for a PNG).
  int width = 0; ///< The layout size of the source (`image`), CSS pixels.
  int height = 0;
};

/// @brief A window size in screen coordinates.
struct window_size {
  int width = 0;
  int height = 0;
};

/// @brief Parses the arguments after `view`: `<in> [--theme T] [--fit F]`.
/// @throws view_usage_error for a missing or second input, `-` (the window
///         needs a file to name its saves after), an unknown option or an
///         unknown theme or fit.
view_args parse_view_args(std::span<const std::string_view> args);

/// @brief Reads and binds the input, without opening a window, and measures
///        its layout.
/// @throws cli_failure for a missing or unreadable file, a corrupt PNG or
///         one without a nymph source; nymph::error (with its position) for
///         a problem in the source; std::runtime_error for fonts this build
///         lacks.
view_request prepare_view(const view_args& args);

/// @brief The window size to open for a layout of `width` x `height`: the
///        same size, but within `available_width` x `available_height`
///        (the screen's usable area; 0 for unknown) and never below
///        240 x 160.
window_size initial_window_size(int width, int height, int available_width, int available_height);

/// @brief Where a save key writes: next to `input`, `<name>.view.<extension>`
///        (never `input` itself).
std::string view_save_path(const std::string& input, std::string_view extension);

/// @brief `nymph view <in> ...`: checks the input, then runs the window until
///        it is closed.
/// @param args The arguments after `view`.
/// @return 0 when the window closed; 1 for a failure (one line on `err`,
///         the source's line and column included); 2 for bad arguments
///         (message and usage on `err`).
int run_view_cli(std::span<const std::string_view> args, std::ostream& err);

/// @brief Who gets a key in `nymph view`.
enum class key_route {
  panel, ///< The editor panel's text field or dialog.
  figure, ///< A text field of the figure (UI mockups, spec/09_ui_mockups).
  viewer, ///< The viewer's shortcuts (Esc, Q, R, S, Y, E).
};

/// @brief The panel first while it has the keyboard, then the figure while
///        one of its text fields does, else the viewer's shortcuts: typing
///        "q" in a field never closes the window.
key_route route_key(bool panel_has_keyboard, bool figure_has_keyboard);

/// @brief The usage text of `nymph view`.
const char* view_usage();

} // namespace bdg::nymph
