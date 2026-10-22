// MIT License © 2026 Binary Dice Games
/// @file editor_registry.hpp
/// @brief Discovery files of running editors: each `nymph view --edit`
///        writes one next to its socket, and `nymph mcp` reads them to find
///        the editor an agent should talk to
///        (spec/14_editor_workspace/DESIGN.md section 3).
#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace bdg::nymph::ipc {

/// @brief One running editor, as its discovery file says.
struct editor_entry {
  int64_t pid = 0;
  std::string socket; ///< The socket path.
  std::string file; ///< The file it edits (absolute).
  std::string version; ///< nymph's version.
  double focused = 0.0; ///< When its window last took the focus (seconds since the epoch).
  std::filesystem::path record; ///< The discovery file.
};

/// @brief The directory of the editors' sockets and discovery files:
///        `$NYMPH_EDITORS_DIR` if set (tests), else `$XDG_RUNTIME_DIR/nymph`,
///        else `<temp>/nymph-<uid>` on POSIX and `%LOCALAPPDATA%\nymph\editors`
///        on Windows. Created on first use, readable by the user only.
/// @throws ipc_error when it exists but belongs to another user or others
///         can reach it (POSIX).
std::filesystem::path editors_dir();

/// @brief Where editor `pid` puts its socket: `<dir>/<pid>.sock`, or when
///        that is longer than kMaxSocketPath, the same name in a short
///        private folder (`/tmp/nymph-<uid>`, or the system's temporary
///        folder on Windows).
/// @throws ipc_error when no folder gives a short enough path.
std::filesystem::path socket_path(const std::filesystem::path& dir, int64_t pid);

/// @brief Whether process `pid` is running.
bool process_alive(int64_t pid);

/// @brief This process's id.
int64_t current_pid();

/// @brief Seconds since the epoch, for `focused`.
double now_epoch();

/// @brief A running editor's discovery file: written on construction,
///        rewritten by set_focused(), removed on destruction.
class editor_record {
 public:
  /// @throws ipc_error when the file cannot be written.
  editor_record(std::filesystem::path dir, editor_entry entry);
  ~editor_record();
  editor_record(const editor_record&) = delete;
  editor_record& operator=(const editor_record&) = delete;

  /// @brief Notes that the window took the focus now.
  void set_focused(double when);

  const editor_entry& entry() const {
    return entry_;
  }

 private:
  void write() const;
  editor_entry entry_;
};

/// @brief The editors whose discovery files are in `dir` (unreadable files
///        skipped), the most recently focused first.
std::vector<editor_entry> list_editors(const std::filesystem::path& dir);

/// @brief Removes an editor's discovery file and socket (one that is gone).
void forget_editor(const editor_entry& e);

/// @brief The editor `want` names: a process id, or the file it edits (its
///        full path or its name); the most recently focused one for "".
std::optional<editor_entry> pick_editor(const std::vector<editor_entry>& editors, std::string_view want);

} // namespace bdg::nymph::ipc
