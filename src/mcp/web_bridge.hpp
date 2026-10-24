// MIT License © 2026 Binary Dice Games
/// @file web_bridge.hpp
/// @brief The web app in a browser tab as one more editor of `nymph mcp`:
///        a WebSocket on 127.0.0.1, opened when an agent asks to pair, a
///        one-time code the user types into the app, and the app's tools
///        forwarded (spec/17_agent_connectors/DESIGN.md section 7.2).
#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include "ipc/loopback.hpp"
#include "mcp/json_rpc.hpp"
#include "mcp/websocket.hpp"

namespace bdg::nymph::mcp {

class tool_registry;

/// @brief How the bridge is set up; tests shorten the times.
struct web_bridge_options {
  /// The app's address, for pairing links.
  std::string app_url = "https://binary-dice-games.github.io/nymph/";
  /// The page origins accepted; empty for the app's own origin plus those in
  /// `NYMPH_WEB_APP_ORIGINS` (separated by commas or spaces). An entry
  /// ending in `:*` accepts any port.
  std::vector<std::string> origins;
  std::chrono::milliseconds code_life{std::chrono::minutes(5)};
  /// How long a call waits for the page (a confirmation may be open there).
  std::chrono::milliseconds call_timeout{std::chrono::minutes(5)};
};

/// @brief What web_bridge::pair() returns.
struct web_pairing {
  std::string code; ///< `<port>-XXXX-XXXX`
  std::string link; ///< the app's address with `#pair=<code>`
  uint16_t port = 0;
  int64_t expires_in = 0; ///< seconds
};

/// @brief Paired web app pages. Thread-safe: the session thread calls in,
///        one reader thread per page answers.
class web_bridge {
 public:
  /// @param changed Called (on a reader thread) when the pages' tools
  ///        change: a page paired, went away, or listed new tools.
  web_bridge(web_bridge_options options, std::function<void()> changed);
  /// Closes the listener and every page, failing waiting calls, and joins
  /// every thread.
  ~web_bridge();
  web_bridge(const web_bridge&) = delete;
  web_bridge& operator=(const web_bridge&) = delete;

  /// @brief Opens the listener if needed (or again after three wrong codes)
  ///        and makes a new code; the previous code stops working.
  /// @throws tool_error when no port can be opened.
  web_pairing pair();

  /// @brief The MCP client's name (`clientInfo.name`), sent with each call
  ///        so the page attributes changes to it.
  void set_client(std::string name);

  /// @brief Whether a page is paired and ready.
  bool has_pages() const;

  /// @brief When the most recently focused page took the focus (seconds
  ///        since the epoch); 0 without pages.
  double latest_focus() const;

  /// @brief The names pages go by (`web:<document name>`).
  std::vector<std::string> page_names() const;

  /// @brief The tools of the pages, without the `editor_*` ones (nymph mcp
  ///        has those already), each once, as `tools/list` entries.
  json page_tools() const;

  /// @brief Calls `tool` on the page `want` names (`web:<name>` or
  ///        `<name>`), or the one focused last for "".
  /// @return The page's CallToolResult.
  /// @throws tool_error without a page, when the page refuses, does not
  ///         answer in time, or closes.
  json call(const std::string& want, const std::string& tool, json arguments);

 private:
  struct page;
  void accept_loop(ipc::loopback_listener* listener);
  void serve(const std::shared_ptr<page>& p);
  bool admit(page& p);
  int request(page& p, const std::string& method, json params, std::function<void(const json&)> reply);
  void reap();
  bool allowed_origin(const std::string& origin) const;
  std::shared_ptr<page> pick(const std::string& want) const;

  web_bridge_options options_;
  std::function<void()> changed_;
  mutable std::mutex mtx_;
  std::unique_ptr<ipc::loopback_listener> listener_;
  std::thread accept_;
  std::vector<std::shared_ptr<page>> pages_;
  std::string code_; ///< "" when none is valid
  std::chrono::steady_clock::time_point code_expires_;
  int wrong_ = 0;
  std::vector<std::string> resume_tokens_;
  std::string client_ = "agent";
  std::atomic<bool> stopping_{false};
};

/// @brief Adds `web_app_pair` (pairing code and link) and `web_app_call`
///        (any tool of a paired tab, for clients that do not re-read the
///        tool list) to `registry`, and lists the paired tabs' own tools in
///        it while they are paired.
void add_web_app_tools(tool_registry& registry, std::shared_ptr<web_bridge> bridge);

} // namespace bdg::nymph::mcp
