// MIT License © 2026 Binary Dice Games
/// @file editor_registry.cpp
/// @brief Discovery files of running editors (see editor_registry.hpp).
#include "ipc/editor_registry.hpp"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <system_error>
#include "ipc/local_socket.hpp"

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <cerrno>
#endif

namespace bdg::nymph::ipc {

namespace {

using json = nlohmann::json;

std::string env(const char* name) {
  const char* v = std::getenv(name);
  return v != nullptr ? std::string(v) : std::string();
}

/// Creates `dir` readable by the user only, or checks that an existing one is.
void make_private(const std::filesystem::path& dir) {
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  if (!std::filesystem::is_directory(dir, ec)) {
    throw ipc_error("cannot make the folder " + dir.string());
  }
#if !defined(_WIN32)
  struct stat st {};
  if (::stat(dir.c_str(), &st) != 0) {
    throw ipc_error("cannot read the folder " + dir.string());
  }
  if (st.st_uid != ::getuid()) {
    throw ipc_error(dir.string() + " belongs to another user; set NYMPH_EDITORS_DIR to a folder of yours");
  }
  if ((st.st_mode & 077) != 0 && ::chmod(dir.c_str(), 0700) != 0) {
    throw ipc_error("cannot make " + dir.string() + " private to you");
  }
#endif
}

} // namespace

std::filesystem::path editors_dir() {
  std::filesystem::path dir;
  if (const std::string forced = env("NYMPH_EDITORS_DIR"); !forced.empty()) {
    dir = forced;
  } else {
#if defined(_WIN32)
    const std::string local = env("LOCALAPPDATA");
    dir = std::filesystem::path(local.empty() ? std::filesystem::temp_directory_path() : std::filesystem::path(local)) /
        "nymph" / "editors";
#else
    if (const std::string runtime = env("XDG_RUNTIME_DIR"); !runtime.empty()) {
      dir = std::filesystem::path(runtime) / "nymph";
    } else {
      const std::string tmp = env("TMPDIR");
      dir = std::filesystem::path(tmp.empty() ? std::string("/tmp") : tmp) / ("nymph-" + std::to_string(::getuid()));
    }
#endif
  }
  make_private(dir);
  return dir;
}

std::filesystem::path socket_path(const std::filesystem::path& dir, int64_t pid) {
  const std::string name = std::to_string(pid) + ".sock";
  std::filesystem::path path = dir / name;
  if (path.string().size() <= kMaxSocketPath) {
    return path;
  }
#if defined(_WIN32)
  const std::filesystem::path short_dir = std::filesystem::temp_directory_path() / "nymph";
#else
  const std::filesystem::path short_dir = std::filesystem::path("/tmp") / ("nymph-" + std::to_string(::getuid()));
#endif
  make_private(short_dir);
  path = short_dir / name;
  if (path.string().size() > kMaxSocketPath) {
    throw ipc_error("no folder gives a socket path short enough (" + path.string() + ")");
  }
  return path;
}

bool process_alive(int64_t pid) {
  if (pid <= 0) {
    return false;
  }
#if defined(_WIN32)
  HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, static_cast<DWORD>(pid));
  if (h == nullptr) {
    return false;
  }
  DWORD code = 0;
  const bool alive = GetExitCodeProcess(h, &code) && code == STILL_ACTIVE;
  CloseHandle(h);
  return alive;
#else
  return ::kill(static_cast<pid_t>(pid), 0) == 0 || errno == EPERM;
#endif
}

int64_t current_pid() {
#if defined(_WIN32)
  return static_cast<int64_t>(GetCurrentProcessId());
#else
  return static_cast<int64_t>(::getpid());
#endif
}

double now_epoch() {
  using namespace std::chrono;
  return duration<double>(system_clock::now().time_since_epoch()).count();
}

// ── editor_record ────────────────────────────────────────────────────────────

editor_record::editor_record(std::filesystem::path dir, editor_entry entry) : entry_(std::move(entry)) {
  entry_.record = dir / (std::to_string(entry_.pid) + ".json");
  write();
}

editor_record::~editor_record() {
  std::error_code ec;
  std::filesystem::remove(entry_.record, ec);
}

void editor_record::set_focused(double when) {
  entry_.focused = when;
  try {
    write();
  } catch (const ipc_error&) {
    // The editor keeps working; the choice between editors is a little off.
  }
}

void editor_record::write() const {
  const json j{
      {"pid", entry_.pid},
      {"socket", entry_.socket},
      {"file", entry_.file},
      {"version", entry_.version},
      {"focused", entry_.focused},
  };
  // Written whole, then renamed: a reader never sees half a file.
  std::filesystem::path temp = entry_.record;
  temp += ".tmp";
  {
    std::ofstream out(temp, std::ios::binary | std::ios::trunc);
    out << j.dump() << "\n";
    if (!out) {
      throw ipc_error("cannot write " + temp.string());
    }
  }
  std::error_code ec;
  std::filesystem::rename(temp, entry_.record, ec);
  if (ec) {
    throw ipc_error("cannot write " + entry_.record.string());
  }
}

// ── Finding editors ──────────────────────────────────────────────────────────

std::vector<editor_entry> list_editors(const std::filesystem::path& dir) {
  std::vector<editor_entry> out;
  std::error_code ec;
  for (std::filesystem::directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec)) {
    if (it->path().extension() != ".json") {
      continue;
    }
    std::ifstream in(it->path(), std::ios::binary);
    std::stringstream ss;
    ss << in.rdbuf();
    try {
      const json j = json::parse(ss.str());
      editor_entry e;
      e.pid = j.at("pid").get<int64_t>();
      e.socket = j.at("socket").get<std::string>();
      e.file = j.value("file", std::string());
      e.version = j.value("version", std::string());
      e.focused = j.value("focused", 0.0);
      e.record = it->path();
      out.push_back(std::move(e));
    } catch (const std::exception&) {
      // Being written, or not ours: skip it.
    }
  }
  std::sort(out.begin(), out.end(), [](const editor_entry& a, const editor_entry& b) {
    return a.focused != b.focused ? a.focused > b.focused : a.pid > b.pid;
  });
  return out;
}

void forget_editor(const editor_entry& e) {
  std::error_code ec;
  std::filesystem::remove(e.record, ec);
  if (!e.socket.empty()) {
    std::filesystem::remove(e.socket, ec);
  }
}

std::optional<editor_entry> pick_editor(const std::vector<editor_entry>& editors, std::string_view want) {
  if (editors.empty()) {
    return std::nullopt;
  }
  if (want.empty()) {
    return editors.front();
  }
  for (const editor_entry& e : editors) {
    const std::filesystem::path file(e.file);
    if (std::to_string(e.pid) == want || e.file == want || file.filename().string() == want) {
      return e;
    }
  }
  // A path written another way (relative, other separators): compare as paths.
  std::error_code ec;
  const std::filesystem::path wanted = std::filesystem::weakly_canonical(std::filesystem::path(want), ec);
  for (const editor_entry& e : editors) {
    if (!ec && std::filesystem::weakly_canonical(std::filesystem::path(e.file), ec) == wanted) {
      return e;
    }
  }
  return std::nullopt;
}

} // namespace bdg::nymph::ipc
