// MIT License © 2026 Binary Dice Games
/// @file editor_bridge.cpp
/// @brief The editor tools forwarded by `nymph mcp` (see editor_bridge.hpp).
#include "mcp/editor_bridge.hpp"
#include <algorithm>
#include "mcp/editor_tools.hpp"
#include "mcp/mcp_session.hpp"
#include "nymph.hpp"

namespace bdg::nymph::mcp {

namespace {

constexpr const char* kNoEditor =
    "no nymph editor is open: ask the user to open the chart with `nymph view --edit <file>` (the editor_* tools "
    "work on the chart open there), or, for the chart open in the nymph web app in a browser, call web_app_pair; to "
    "make or render a chart without the editor use render_chart";

} // namespace

std::filesystem::path editor_bridge::dir() const {
  if (!dir_.empty()) {
    return dir_;
  }
  try {
    return ipc::editors_dir();
  } catch (const ipc::ipc_error& e) {
    throw tool_error(std::string(kNoEditor) + " (" + e.what() + ")");
  }
}

double editor_bridge::latest_native_focus() const {
  double latest = -1.0;
  try {
    for (const ipc::editor_entry& e : ipc::list_editors(dir())) {
      if (ipc::process_alive(e.pid)) {
        latest = std::max(latest, e.focused);
      }
    }
  } catch (const std::exception&) {
  }
  return latest;
}

editor_bridge::link& editor_bridge::connect(const std::string& want) {
  const std::filesystem::path dir = this->dir();
  // Forget editors that are gone (a crash leaves its file behind).
  std::vector<ipc::editor_entry> editors;
  for (ipc::editor_entry& e : ipc::list_editors(dir)) {
    if (ipc::process_alive(e.pid)) {
      editors.push_back(std::move(e));
    } else {
      ipc::forget_editor(e);
      links_.erase(e.pid);
    }
  }
  while (true) {
    const std::optional<ipc::editor_entry> chosen = ipc::pick_editor(editors, want);
    if (!chosen) {
      if (!want.empty() && !editors.empty()) {
        std::string open;
        for (const ipc::editor_entry& e : editors) {
          open += (open.empty() ? "" : ", ") + e.file + " (" + std::to_string(e.pid) + ")";
        }
        throw tool_error("no editor has '" + want + "' open; open editors: " + open);
      }
      throw tool_error(kNoEditor);
    }
    if (const auto it = links_.find(chosen->pid); it != links_.end()) {
      return *it->second;
    }
    try {
      auto l = std::make_unique<link>();
      l->entry = *chosen;
      l->stream = ipc::local_stream::connect(chosen->socket);
      const json init = request(
          *l, "initialize",
          {{"protocolVersion", std::string(supported_protocol_versions().front())},
           {"capabilities", json::object()},
           {"clientInfo", {{"name", "nymph-mcp"}, {"version", std::string(version())}}}});
      if (!init.contains("result")) {
        throw ipc::ipc_error("the editor refused the connection");
      }
      l->stream.write(to_line(json{{"jsonrpc", "2.0"}, {"method", "notifications/initialized"}}) + "\n");
      link& ref = *l;
      links_[chosen->pid] = std::move(l);
      return ref;
    } catch (const std::exception&) {
      // Its socket does not answer: as good as gone.
      ipc::forget_editor(*chosen);
      editors.erase(std::remove_if(editors.begin(), editors.end(), [&](const ipc::editor_entry& e) { return e.pid == chosen->pid; }), editors.end());
    }
  }
}

json editor_bridge::request(link& l, const std::string& method, json params) {
  const int id = l.next_id++;
  const json message{{"jsonrpc", "2.0"}, {"id", id}, {"method", method}, {"params", std::move(params)}};
  if (!l.stream.write(to_line(message) + "\n")) {
    throw ipc::ipc_error("the editor closed");
  }
  std::string line;
  while (l.stream.read_line(line)) {
    json reply;
    try {
      reply = json::parse(line);
    } catch (const json::exception&) {
      continue;
    }
    if (reply.contains("id") && reply["id"] == id) {
      return reply;
    }
  }
  throw ipc::ipc_error("the editor closed");
}

json editor_bridge::call(const std::string& tool, json arguments) {
  std::string want;
  if (arguments.contains("editor")) {
    if (arguments["editor"].is_string()) {
      want = arguments["editor"].get<std::string>();
    } else if (arguments["editor"].is_number_integer()) {
      want = std::to_string(arguments["editor"].get<int64_t>());
    } else {
      throw tool_error("'editor' is the file the editor has open, or its process id");
    }
    arguments.erase("editor");
  }
  if (web_ && web_->has_pages()) {
    bool to_web = want.rfind("web:", 0) == 0;
    if (want.empty()) {
      to_web = web_->latest_focus() >= latest_native_focus();
    } else if (!to_web) {
      const std::vector<std::string> pages = web_->page_names();
      to_web = std::find(pages.begin(), pages.end(), "web:" + want) != pages.end();
    }
    if (to_web) {
      return web_->call(want, tool, std::move(arguments));
    }
  }
  link& l = connect(want);
  json reply;
  try {
    reply = request(l, "tools/call", {{"name", tool}, {"arguments", std::move(arguments)}});
  } catch (const ipc::ipc_error&) {
    links_.erase(l.entry.pid);
    throw tool_error("the nymph editor was closed while it worked on this; call editor_state again");
  }
  if (reply.contains("error")) {
    throw tool_error(reply["error"].value("message", std::string("the editor refused the request")));
  }
  return reply["result"];
}

void add_editor_bridge(tool_registry& registry, std::shared_ptr<editor_bridge> bridge) {
  for (json definition : editor_tool_definitions()) {
    definition["inputSchema"]["properties"]["editor"] = {
        {"type", "string"},
        {"description",
         "Which open editor: the file it edits, its process id, or web:<document name> for a paired web app tab; "
         "default the one used last."}};
    const std::string name = definition["name"];
    registry.add({std::move(definition), [bridge, name](const json& args) { return bridge->call(name, args); }});
  }
}

} // namespace bdg::nymph::mcp
