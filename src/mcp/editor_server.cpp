// MIT License © 2026 Binary Dice Games
/// @file editor_server.cpp
/// @brief The editor's MCP server (see editor_server.hpp).
#include "mcp/editor_server.hpp"
#include <utility>
#include "mcp/json_rpc.hpp"
#include "mcp/mcp_session.hpp"

namespace bdg::nymph::mcp {

// ── ui_queue ─────────────────────────────────────────────────────────────────

json ui_queue::run(const editor_job& job) {
  auto it = std::make_shared<item>();
  it->work = job;
  std::future<json> result = it->result.get_future();
  {
    const std::lock_guard<std::mutex> lock(mtx_);
    if (closed_) {
      throw tool_error("the editor was closed");
    }
    items_.push_back(it);
  }
  if (wake_) {
    wake_();
  }
  return result.get(); // rethrows what the job threw
}

size_t ui_queue::run_pending(editor_host& host) {
  std::deque<std::shared_ptr<item>> ready;
  {
    const std::lock_guard<std::mutex> lock(mtx_);
    ready.swap(items_);
  }
  for (const std::shared_ptr<item>& it : ready) {
    try {
      it->result.set_value(it->work(host));
    } catch (const tool_error&) {
      it->result.set_exception(std::current_exception());
    } catch (const std::exception& e) {
      it->result.set_exception(std::make_exception_ptr(tool_error(e.what())));
    }
  }
  return ready.size();
}

void ui_queue::close() {
  std::deque<std::shared_ptr<item>> waiting;
  {
    const std::lock_guard<std::mutex> lock(mtx_);
    closed_ = true;
    waiting.swap(items_);
  }
  for (const std::shared_ptr<item>& it : waiting) {
    it->result.set_exception(std::make_exception_ptr(tool_error("the editor was closed")));
  }
}

// ── editor_server ────────────────────────────────────────────────────────────

editor_server::editor_server(std::filesystem::path socket, std::function<void()> wake)
    : queue_(std::move(wake)), listener_(std::move(socket)) {
  accept_ = std::thread([this] { accept_loop(); });
}

editor_server::~editor_server() {
  queue_.close(); // a connection waiting on the UI thread gets its error
  listener_.stop();
  if (accept_.joinable()) {
    accept_.join();
  }
  std::vector<std::unique_ptr<connection>> all;
  {
    const std::lock_guard<std::mutex> lock(mtx_);
    all.swap(connections_);
  }
  for (const std::unique_ptr<connection>& c : all) {
    c->stream.shutdown_reads(); // its read returns; a reply on its way still goes out
  }
  for (const std::unique_ptr<connection>& c : all) {
    if (c->thread.joinable()) {
      c->thread.join();
    }
  }
}

void editor_server::accept_loop() {
  while (std::optional<ipc::local_stream> s = listener_.accept()) {
    auto c = std::make_unique<connection>();
    c->stream = std::move(*s);
    connection& ref = *c;
    const std::lock_guard<std::mutex> lock(mtx_);
    connections_.push_back(std::move(c));
    ref.thread = std::thread([this, &ref] { serve(ref); });
  }
}

void editor_server::serve(connection& c) {
  ++open_;
  server_options options;
  options.builtin_tools = false;
  options.server_name = "nymph-editor";
  options.instructions = editor_instructions();
  options.extra_tools = [this](tool_registry& registry) {
    add_editor_tools(registry, [this](const editor_job& job) { return queue_.run(job); });
  };
  session s(options);
  std::string line;
  while (c.stream.read_line(line)) {
    if (line.find_first_not_of(" \t") == std::string::npos) {
      continue;
    }
    std::optional<json> response;
    json id;
    try {
      response = s.handle(parse_message(line, id));
    } catch (const rpc_error& e) {
      response = make_error(id, e.code(), e.what());
    } catch (const std::exception& e) {
      response = make_error(id, rpc_code::internal_error, std::string("internal error: ") + e.what());
    }
    if (response && !c.stream.write(to_line(*response) + "\n")) {
      break;
    }
  }
  --open_;
}

} // namespace bdg::nymph::mcp
