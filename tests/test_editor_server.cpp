// MIT License © 2026 Binary Dice Games
/// @file test_editor_server.cpp
/// @brief spec/14_editor_workspace plan steps 9 and 11: the editor's MCP
///        server on its socket, its tools against a model run on "the UI
///        thread" (this test's), stale versions, refusals, closing.
#include <gtest/gtest.h>
#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <functional>
#include <future>
#include <mutex>
#include <string>
#include "document/view.hpp"
#include "edit/tree_edit.hpp"
#include "ipc/local_socket.hpp"
#include "ipc/editor_registry.hpp"
#include "mcp/editor_bridge.hpp"
#include "mcp/editor_server.hpp"
#include "mcp/mcp_session.hpp"
#include "mcp/editor_tools.hpp"
#include "nymph.hpp"
#include "schema/element_class.hpp"

namespace bdg::nymph::mcp {
namespace {

const char* const kChart =
    "root:\n"
    "  type: Flowchart\n"
    "  children:\n"
    "    - {type: FlowNode, id: cart, text: Cart}\n"
    "    - {type: FlowNode, id: purchases, text: Purchases}\n"
    "    - {type: FlowNode, id: home, text: Home}\n"
    "data: |\n"
    "  x, y\n"
    "  1, 2\n";

/// An editor without a window.
struct test_host : editor_host {
  editor_model m{kChart};
  std::vector<std::string> changes;
  std::optional<std::string> tab;

  editor_model* model() override {
    return &m;
  }
  std::string file() const override {
    return "chart.yaml";
  }
  std::optional<std::string> data_tab() const override {
    return tab;
  }
  void show_tab(std::optional<std::string> t) override {
    tab = std::move(t);
  }
  std::vector<edit_element> tree() const override {
    std::vector<edit_element> out;
    for (const auto& [path, n] : element_paths(m.bound().root)) {
      out.push_back({path, std::string(n->cls->name()), "", static_cast<int>(std::count(path.begin(), path.end(), '/') / 2)});
    }
    std::sort(out.begin(), out.end(), [](const edit_element& a, const edit_element& b) { return path_before(a.path, b.path); });
    return out;
  }
  std::string view_source() const override {
    return m.source();
  }
  std::string panel_error() const override {
    return {};
  }
  void changed(const commit_result&, const std::string& tool) override {
    changes.push_back(tool);
  }
  void select(const edit_target_id& t) override {
    m.select(t);
  }
  std::vector<uint8_t> view_image(int, int& width, int& height) override {
    // No screen here: the chart drawn as a PNG stands in for it.
    width = 1;
    height = 1;
    return render_png(m.source());
  }
};

std::filesystem::path test_dir() {
  const auto* info = ::testing::UnitTest::GetInstance()->current_test_info();
  const size_t h = std::hash<std::string>{}(std::string(info->test_suite_name()) + "." + info->name());
  const std::filesystem::path dir = std::filesystem::temp_directory_path() / ("nye" + std::to_string(h % 100000000));
  std::filesystem::remove_all(dir);
  std::filesystem::create_directories(dir);
  return dir;
}

/// A server, a host, and the UI thread's loop: run jobs while waiting.
struct fixture {
  std::filesystem::path dir = test_dir();
  test_host host;
  std::mutex mtx;
  std::condition_variable woken;
  bool wake = false;
  std::unique_ptr<editor_server> server = std::make_unique<editor_server>(dir / "e.sock", [this] {
    const std::lock_guard<std::mutex> lock(mtx);
    wake = true;
    woken.notify_one();
  });

  ~fixture() {
    server.reset();
    std::filesystem::remove_all(dir);
  }

  /// Waits for `f` while running the jobs it queues, as the window's loop does.
  template <typename T>
  T serve_until(std::future<T>& f) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(20);
    while (f.wait_for(std::chrono::seconds(0)) != std::future_status::ready) {
      std::unique_lock<std::mutex> lock(mtx);
      woken.wait_for(lock, std::chrono::milliseconds(20), [this] { return wake; });
      wake = false;
      lock.unlock();
      server->run_pending(host);
      if (std::chrono::steady_clock::now() > deadline) {
        ADD_FAILURE() << "no answer";
        break;
      }
    }
    return f.get();
  }
};

/// A client: one MCP session over the socket.
struct client {
  ipc::local_stream s;
  int next_id = 1;
  explicit client(const std::filesystem::path& socket) : s(ipc::local_stream::connect(socket)) {}

  json request(const std::string& method, json params) {
    json message{{"jsonrpc", "2.0"}, {"id", next_id++}, {"method", method}, {"params", std::move(params)}};
    EXPECT_TRUE(s.write(message.dump() + "\n"));
    std::string line;
    EXPECT_TRUE(s.read_line(line));
    return json::parse(line);
  }
  json initialize() {
    return request("initialize", {{"protocolVersion", "2025-11-25"}, {"capabilities", json::object()}, {"clientInfo", {{"name", "t"}, {"version", "1"}}}});
  }
  json call(const std::string& tool, json args = json::object()) {
    return request("tools/call", {{"name", tool}, {"arguments", std::move(args)}})["result"];
  }
};

/// Calls `tool` from another thread while this one serves the jobs.
json call(fixture& f, client& c, const std::string& tool, json args = json::object()) {
  std::future<json> answer = std::async(std::launch::async, [&] { return c.call(tool, args); });
  return f.serve_until(answer);
}

TEST(editor_server, ListsTheEditorToolsUnderItsOwnName) {
  fixture f;
  client c(f.server->socket());
  const json init = c.initialize();
  EXPECT_EQ(init["result"]["serverInfo"]["name"], "nymph-editor");
  EXPECT_NE(init["result"]["instructions"].get<std::string>().find("editor_state"), std::string::npos);
  const json list = c.request("tools/list", json::object())["result"]["tools"];
  std::vector<std::string> names;
  for (const json& t : list) {
    names.push_back(t["name"]);
  }
  std::vector<std::string> expected;
  for (const json& t : editor_tool_definitions()) {
    expected.push_back(t["name"]);
  }
  EXPECT_EQ(names, expected);
  EXPECT_EQ(std::count(names.begin(), names.end(), "render_chart"), 0);
  EXPECT_EQ(names.front(), "editor_state");
}

TEST(editor_server, StateShowsTheTreeTheSelectionAndTheTables) {
  fixture f;
  f.host.m.select(edit_target_id{"root/children/0", ""});
  client c(f.server->socket());
  c.initialize();
  const json state = call(f, c, "editor_state")["structuredContent"];
  EXPECT_EQ(state["file"], "chart.yaml");
  EXPECT_EQ(state["tab"], "chart");
  EXPECT_EQ(state["selection"]["path"], "root/children/0");
  EXPECT_EQ(state["selection"]["fields"]["id"], "cart");
  EXPECT_EQ(state["tree"][2]["id"], "purchases");
  EXPECT_EQ(state["tables"][0]["columns"], (json{"x", "y"}));
  EXPECT_TRUE(state["selection"]["takes"].empty()); // a node takes no children
  f.host.m.select(edit_target_id{"root", ""});
  const json root = call(f, c, "editor_state")["structuredContent"]["selection"];
  EXPECT_NE(std::find(root["takes"].begin(), root["takes"].end(), "FlowLink"), root["takes"].end()) << root.dump();
}

TEST(editor_server, ConnectsTheSelectedNodeToAnotherByName) {
  fixture f;
  f.host.m.select(edit_target_id{"root/children/0", ""});
  client c(f.server->socket());
  c.initialize();
  const uint64_t version = call(f, c, "editor_state")["structuredContent"]["version"];
  const json r = call(
      f, c, "editor_add_element",
      {{"parent", "root"}, {"type", "FlowLink"}, {"fields", {{"from", "cart"}, {"to", "purchases"}}}, {"expected_version", version}});
  ASSERT_FALSE(r.value("isError", false)) << r.dump();
  EXPECT_EQ(r["structuredContent"]["path"], "root/children/3");
  EXPECT_NE(f.host.m.source().find("{type: FlowLink, from: cart, to: purchases}"), std::string::npos) << f.host.m.source();
  EXPECT_EQ(f.host.changes, std::vector<std::string>{"editor_add_element"});
}

TEST(editor_server, AStaleVersionChangesNothing) {
  fixture f;
  client c(f.server->socket());
  c.initialize();
  const uint64_t version = call(f, c, "editor_state")["structuredContent"]["version"];
  ASSERT_TRUE(f.host.m.set_fields("root/children/0", {{"text", std::string("Basket")}}).ok); // the user, meanwhile
  const std::string source = f.host.m.source();
  const json r = call(f, c, "editor_set_fields", {{"path", "root/children/0"}, {"fields", {{"text", "Cart"}}}, {"expected_version", version}});
  EXPECT_TRUE(r.value("isError", false));
  EXPECT_NE(r["content"][0]["text"].get<std::string>().find("changed since you read it"), std::string::npos);
  EXPECT_EQ(f.host.m.source(), source);
}

TEST(editor_server, AnInvalidEditIsRefusedWithTheLoadersMessage) {
  fixture f;
  client c(f.server->socket());
  c.initialize();
  const json r = call(f, c, "editor_set_fields", {{"path", "root/children/0"}, {"fields", {{"shape", "Banana"}}}});
  EXPECT_TRUE(r.value("isError", false));
  EXPECT_NE(r["content"][0]["text"].get<std::string>().find("refused"), std::string::npos) << r.dump();
  EXPECT_TRUE(f.host.changes.empty());
  const json removed = call(f, c, "editor_remove_element", {{"path", "root"}});
  EXPECT_TRUE(removed.value("isError", false));
}

TEST(editor_server, TablesAndCellsAreChangedAsOneStep) {
  fixture f;
  client c(f.server->socket());
  c.initialize();
  const json cells = call(f, c, "editor_set_cells", {{"cells", {{{"row", 0}, {"column", "y"}, {"value", "7"}}, {{"row", 2}, {"column", 0}, {"value", "3"}}}}});
  ASSERT_FALSE(cells.value("isError", false)) << cells.dump();
  const json table = call(f, c, "editor_table")["structuredContent"];
  EXPECT_EQ(table["csv"], "x, y\n1, 7\n, \n3, \n");
  EXPECT_TRUE(f.host.m.undo());
  EXPECT_EQ(f.host.m.source(), kChart);
  const json created = call(f, c, "editor_set_table", {{"table", "extra"}, {"csv", "a\n1\n"}, {"create", true}, {"first_name", "main"}});
  ASSERT_FALSE(created.value("isError", false)) << created.dump();
  EXPECT_EQ(f.host.m.tables().size(), 2u);
}

TEST(editor_server, SelectShowsATabAndAnElement) {
  fixture f;
  client c(f.server->socket());
  c.initialize();
  call(f, c, "editor_select", {{"tab", ""}, {"path", "root/children/2"}});
  EXPECT_EQ(f.host.tab, std::optional<std::string>(""));
  EXPECT_EQ(f.host.m.selection()->element, "root/children/2");
  EXPECT_TRUE(call(f, c, "editor_select", {{"tab", "nope"}}).value("isError", false));
}

TEST(editor_server, TheViewImageIsAPng) {
  fixture f;
  client c(f.server->socket());
  c.initialize();
  const json r = call(f, c, "editor_view_image", {{"max_width", 300}});
  ASSERT_FALSE(r.value("isError", false)) << r.dump();
  EXPECT_EQ(r["content"][0]["type"], "image");
  EXPECT_EQ(r["content"][0]["mimeType"], "image/png");
  EXPECT_EQ(r["content"][0]["data"].get<std::string>().substr(0, 4), "iVBO"); // base64 of the PNG signature
}

TEST(editor_server, TwoClientsAreServedInTurn) {
  fixture f;
  client a(f.server->socket());
  client b(f.server->socket());
  a.initialize();
  b.initialize();
  call(f, a, "editor_set_fields", {{"path", "root/children/0"}, {"fields", {{"text", "A"}}}});
  call(f, b, "editor_set_fields", {{"path", "root/children/1"}, {"fields", {{"text", "B"}}}});
  EXPECT_EQ(f.host.changes.size(), 2u);
  EXPECT_EQ(f.server->connections(), 2);
}

TEST(editor_server, ClosingTheServerFailsAPendingCall) {
  fixture f;
  client c(f.server->socket());
  c.initialize();
  std::future<json> pending = std::async(std::launch::async, [&] { return c.call("editor_state"); });
  // Nobody runs the job; the editor closes instead.
  while (pending.wait_for(std::chrono::milliseconds(10)) != std::future_status::ready && f.server->connections() == 0) {
  }
  std::thread closer([&] { f.server.reset(); });
  const json r = pending.get();
  closer.join();
  EXPECT_TRUE(r.value("isError", false));
  EXPECT_NE(r["content"][0]["text"].get<std::string>().find("the editor was closed"), std::string::npos) << r.dump();
}

// ── nymph mcp's bridge (plan step 10) ────────────────────────────────────────

/// Calls `tool` through the bridge from another thread while this one serves.
json bridged(fixture& f, editor_bridge& b, const std::string& tool, json args = json::object()) {
  std::future<json> answer = std::async(std::launch::async, [&] {
    try {
      return b.call(tool, args);
    } catch (const tool_error& e) {
      return json{{"bridge_error", e.what()}};
    }
  });
  return f.serve_until(answer);
}

TEST(editor_bridge, WithNoEditorOpenTheAgentIsToldWhatToDo) {
  const std::filesystem::path dir = test_dir();
  editor_bridge b(dir);
  try {
    (void)b.call("editor_state", json::object());
    FAIL() << "a call with no editor answered";
  } catch (const tool_error& e) {
    EXPECT_NE(std::string(e.what()).find("nymph view --edit"), std::string::npos) << e.what();
  }
  std::filesystem::remove_all(dir);
}

TEST(editor_bridge, ForwardsToTheOpenEditor) {
  fixture f;
  const std::filesystem::path records = f.dir / "records";
  std::filesystem::create_directories(records);
  ipc::editor_record r(records, ipc::editor_entry{ipc::current_pid(), f.server->socket().string(), "/charts/chart.yaml", "1", 1.0, {}});
  editor_bridge b(records);
  const json state = bridged(f, b, "editor_state");
  ASSERT_FALSE(state.contains("bridge_error")) << state.dump();
  EXPECT_EQ(state["structuredContent"]["file"], "chart.yaml"); // what the host says
  const json added = bridged(f, b, "editor_add_element", {{"parent", "root"}, {"type", "FlowLink"}, {"fields", {{"from", "home"}, {"to", "cart"}}}, {"editor", "chart.yaml"}});
  ASSERT_FALSE(added.contains("bridge_error")) << added.dump();
  EXPECT_FALSE(added.value("isError", false)) << added.dump();
  EXPECT_NE(f.host.m.source().find("{type: FlowLink, from: home, to: cart}"), std::string::npos);
  const json other = bridged(f, b, "editor_state", {{"editor", "other.yaml"}});
  EXPECT_NE(other["bridge_error"].get<std::string>().find("open editors: /charts/chart.yaml"), std::string::npos) << other.dump();
}

TEST(editor_bridge, AFileOfAProcessThatIsGoneIsSkippedAndRemoved) {
  fixture f;
  const std::filesystem::path records = f.dir / "records";
  std::filesystem::create_directories(records);
  {
    // A crashed editor: its file stays, focused later than the live one.
    std::ofstream(records / "999999999.json") << R"({"pid": 999999999, "socket": "gone.sock", "file": "old.yaml", "focused": 9.0})";
  }
  ipc::editor_record r(records, ipc::editor_entry{ipc::current_pid(), f.server->socket().string(), "chart.yaml", "1", 1.0, {}});
  editor_bridge b(records);
  const json state = bridged(f, b, "editor_state");
  ASSERT_FALSE(state.contains("bridge_error")) << state.dump();
  EXPECT_FALSE(std::filesystem::exists(records / "999999999.json"));
}

TEST(editor_bridge, NymphMcpListsTheEditorToolsWithAnEditorArgument) {
  server_options options;
  session s(options);
  const json list = s.tools().list()["tools"];
  bool found = false;
  for (const json& t : list) {
    if (t["name"] == "editor_add_element") {
      found = true;
      EXPECT_TRUE(t["inputSchema"]["properties"].contains("editor"));
    }
  }
  EXPECT_TRUE(found);
}

} // namespace
} // namespace bdg::nymph::mcp
