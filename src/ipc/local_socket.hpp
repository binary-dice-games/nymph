// MIT License © 2026 Binary Dice Games
/// @file local_socket.hpp
/// @brief A stream socket on a file path (`AF_UNIX`): how `nymph mcp`
///        reaches a running editor (spec/14_editor_workspace/DESIGN.md
///        sections 3 and 9). Linux, macOS, MSYS2, and Windows 10 1803 or
///        later (`afunix.h`). Lines of text in both directions.
#pragma once

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

namespace bdg::nymph::ipc {

/// @brief A socket that could not be created, bound or connected.
class ipc_error : public std::runtime_error {
 public:
  using std::runtime_error::runtime_error;
};

/// @brief The longest socket path accepted: within every platform's limit
///        (macOS: 104 bytes with the terminator).
inline constexpr size_t kMaxSocketPath = 100;

/// @brief One connection. Reads and writes may run on different threads;
///        close() from any thread ends a read blocked on another.
class local_stream {
 public:
  local_stream() = default;
  explicit local_stream(intptr_t handle) : handle_(handle) {}
  ~local_stream();
  local_stream(local_stream&& other) noexcept;
  local_stream& operator=(local_stream&& other) noexcept;
  local_stream(const local_stream&) = delete;
  local_stream& operator=(const local_stream&) = delete;

  /// @brief Connects to the listener at `path`.
  /// @throws ipc_error when nothing listens there.
  static local_stream connect(const std::filesystem::path& path);

  /// @brief Reads up to the next "\n" (dropped, as is a "\r" before it).
  /// @return false at the end of the stream or on an error.
  bool read_line(std::string& line);

  /// @brief Writes all of `data`. @return false on an error.
  bool write(std::string_view data);

  /// @brief Ends both directions: a read blocked on another thread returns.
  void shutdown();

  /// @brief Ends reading only: a read blocked on another thread returns, and
  ///        a reply being written still goes out.
  void shutdown_reads();

  bool valid() const {
    return handle_ != kInvalid;
  }

  static constexpr intptr_t kInvalid = -1;

 private:
  void close();

  intptr_t handle_ = kInvalid;
  std::string buffer_; ///< Bytes read past the last line.
};

/// @brief A listening socket at a path. accept() blocks; stop() from another
///        thread makes it return nullopt.
class local_listener {
 public:
  /// @brief Listens at `path`, replacing a file left there by a process that
  ///        is gone (nothing answers on it).
  /// @throws ipc_error for a path that is too long, one where something
  ///         listens already, or a socket that cannot be made.
  explicit local_listener(std::filesystem::path path);
  /// Stops and removes the socket file.
  ~local_listener();
  local_listener(const local_listener&) = delete;
  local_listener& operator=(const local_listener&) = delete;

  /// @brief The next connection; nullopt once stop() was called.
  std::optional<local_stream> accept();

  /// @brief Makes accept() return nullopt, now and from then on (any thread).
  void stop();

  const std::filesystem::path& path() const {
    return path_;
  }

 private:
  std::filesystem::path path_;
  intptr_t handle_ = local_stream::kInvalid;
  std::atomic<bool> stopped_{false};
};

} // namespace bdg::nymph::ipc
