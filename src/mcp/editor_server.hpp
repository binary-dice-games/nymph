// MIT License © 2026 Binary Dice Games
/// @file editor_server.hpp
/// @brief The running editor as an MCP server: a session of the editor tools
///        for every connection on its local socket, their work done on the
///        editor's UI thread (spec/14_editor_workspace/DESIGN.md sections 3
///        and 7).
#pragma once

#include <atomic>
#include <deque>
#include <filesystem>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>
#include "ipc/local_socket.hpp"
#include "mcp/editor_tools.hpp"

namespace bdg::nymph::mcp {

/// @brief Work handed from connection threads to the UI thread.
class ui_queue {
 public:
  /// @param wake Called (on the posting thread) after a job is queued: makes
  ///        the UI thread come round (native_window::wake()).
  explicit ui_queue(std::function<void()> wake) : wake_(std::move(wake)) {}

  /// @brief Queues `job` and waits for the UI thread to run it.
  /// @return Its result.
  /// @throws tool_error what the job threw, or "the editor was closed".
  json run(const editor_job& job);

  /// @brief Runs the queued jobs (UI thread).
  /// @return How many ran.
  size_t run_pending(editor_host& host);

  /// @brief Fails the queued jobs and every later one.
  void close();

 private:
  struct item {
    editor_job work;
    std::promise<json> result;
  };
  std::function<void()> wake_;
  std::mutex mtx_;
  std::deque<std::shared_ptr<item>> items_;
  bool closed_ = false;
};

/// @brief Serves the editor tools on a local socket until destroyed.
class editor_server {
 public:
  /// @brief Listens at `socket` and starts accepting.
  /// @param wake As ui_queue.
  /// @throws ipc::ipc_error when it cannot listen there.
  editor_server(std::filesystem::path socket, std::function<void()> wake);
  /// Stops accepting, fails waiting jobs, closes the connections and joins
  /// every thread; the socket file is removed.
  ~editor_server();
  editor_server(const editor_server&) = delete;
  editor_server& operator=(const editor_server&) = delete;

  /// @brief Runs the jobs the connections queued (UI thread).
  size_t run_pending(editor_host& host) {
    return queue_.run_pending(host);
  }

  /// @brief Connections open now (an agent is attached).
  int connections() const {
    return open_.load();
  }

  const std::filesystem::path& socket() const {
    return listener_.path();
  }

 private:
  struct connection {
    ipc::local_stream stream;
    std::thread thread;
  };
  void accept_loop();
  void serve(connection& c);

  ui_queue queue_;
  ipc::local_listener listener_;
  std::mutex mtx_;
  std::vector<std::unique_ptr<connection>> connections_;
  std::atomic<int> open_{0};
  std::thread accept_; ///< Last: started once everything else exists.
};

} // namespace bdg::nymph::mcp
