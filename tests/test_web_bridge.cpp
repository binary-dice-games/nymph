// MIT License © 2026 Binary Dice Games
/// @file test_web_bridge.cpp
/// @brief spec/17_agent_connectors plan step 7: `nymph mcp` pairs with a web
///        app tab over a loopback WebSocket and forwards tools to it. A test
///        client stands in for the tab: it pairs with the code, then serves
///        MCP (initialize, tools/list, tools/call) as the app does.
#include <gtest/gtest.h>
#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <regex>
#include <string>
#include <thread>
#include <vector>
#include "mcp/mcp_session.hpp"
#include "ws_test_client.hpp"

namespace bdg::nymph::mcp {
namespace {

using namespace std::chrono_literals;

constexpr const char* kOrigin = "https://app.test";

/// An empty editors folder of this test's own: no native editor is found.
std::filesystem::path test_dir() {
  const auto* info = ::testing::UnitTest::GetInstance()->current_test_info();
  const std::filesystem::path dir =
      std::filesystem::temp_directory_path() / ("nymph-web-bridge-" + std::string(info->name()));
  std::filesystem::remove_all(dir);
  std::filesystem::create_directories(dir);
  return dir;
}

/// A tab of the web app: pairs, then answers MCP requests from the bridge.
class fake_page {
 public:
  fake_page(uint16_t port, json hello, std::string origin = kOrigin) : c_(port, origin) {
    if (!c_.upgraded()) {
      return;
    }
    c_.send(hello.dump());
    if (const std::optional<std::string> r = c_.read()) {
      reply_ = json::parse(*r);
    }
    if (paired()) {
      thread_ = std::thread([this] { serve(); });
    }
  }
  ~fake_page() {
    close();
  }

  bool upgraded() const {
    return c_.upgraded();
  }
  bool paired() const {
    return reply_.value("paired", false);
  }
  const json& reply() const {
    return reply_;
  }
  /// The tools/call requests received.
  std::vector<json> calls() {
    const std::lock_guard<std::mutex> lock(mtx_);
    return calls_;
  }
  /// Waits until `n` calls have arrived.
  bool wait_calls(size_t n) {
    std::unique_lock<std::mutex> lock(mtx_);
    return cv_.wait_for(lock, 5s, [&] { return calls_.size() >= n; });
  }
  void send(const json& message) {
    c_.send(message.dump());
  }
  void close() {
    c_.close();
    if (thread_.joinable()) {
      thread_.join();
    }
  }

 private:
  void serve() {
    while (const std::optional<std::string> text = c_.read()) {
      const json m = json::parse(*text);
      if (!m.contains("id") || !m.contains("method")) {
        continue;
      }
      const std::string method = m["method"];
      json result;
      if (method == "initialize") {
        result = {
            {"protocolVersion", "2025-11-25"},
            {"capabilities", {{"tools", json::object()}}},
            {"serverInfo", {{"name", "nymph-web-app"}, {"version", "test"}}}};
      } else if (method == "tools/list") {
        result = {
            {"tools",
             {{{"name", "document_state"}, {"description", "state"}, {"inputSchema", {{"type", "object"}}}},
              {{"name", "document_set_source"},
               {"description", "set"},
               {"inputSchema",
                {{"type", "object"},
                 {"properties", {{"source", {{"type", "string"}}}}},
                 {"additionalProperties", false}}}},
              {{"name", "document_slow"}, {"description", "never answers"}, {"inputSchema", {{"type", "object"}}}},
              {{"name", "editor_set_fields"}, {"description", "editor"}, {"inputSchema", {{"type", "object"}}}}}}};
      } else if (method == "tools/call") {
        {
          const std::lock_guard<std::mutex> lock(mtx_);
          calls_.push_back(m["params"]);
        }
        cv_.notify_all();
        if (m["params"]["name"] == "document_slow") {
          continue;
        }
        result = {{"content", {{{"type", "text"}, {"text", "page:" + m["params"]["name"].get<std::string>()}}}}};
      }
      c_.send(json{{"jsonrpc", "2.0"}, {"id", m["id"]}, {"result", result}}.dump());
    }
  }

  test::ws_client c_;
  json reply_ = json::object();
  std::thread thread_;
  std::mutex mtx_;
  std::condition_variable cv_;
  std::vector<json> calls_;
};

/// A `nymph mcp` session that counts its list_changed notifications.
class web_bridge_test : public ::testing::Test {
 protected:
  void SetUp() override {
    server_options o;
    o.web_app_origins = {kOrigin, "http://localhost:*"};
    o.web_app_url = "https://app.test/nymph/";
    o.web_call_timeout = 300ms;
    o.editors_dir = test_dir();
    o.notify = [this](const json& message) {
      const std::lock_guard<std::mutex> lock(mtx_);
      if (message.value("method", std::string()) == "notifications/tools/list_changed") {
        ++changes_;
      }
      cv_.notify_all();
    };
    s_ = std::make_unique<session>(o);
    rpc_message init;
    init.id = 1;
    init.method = "initialize";
    init.params = {
        {"protocolVersion", "2025-11-25"},
        {"capabilities", json::object()},
        {"clientInfo", {{"name", "Test Client"}, {"version", "1"}}}};
    initialize_ = s_->handle(init).value();
  }

  json call(const std::string& name, json args = json::object()) {
    return s_->tools().call({{"name", name}, {"arguments", std::move(args)}});
  }
  static std::string text_of(const json& r) {
    return r["content"][0].value("text", std::string());
  }
  /// The pairing: code and port.
  std::pair<std::string, uint16_t> pair() {
    const json r = call("web_app_pair");
    const json& sc = r["structuredContent"];
    return {sc["code"].get<std::string>(), sc["port"].get<uint16_t>()};
  }
  /// Waits until list_changed was sent `n` times.
  bool wait_changes(int n) {
    std::unique_lock<std::mutex> lock(mtx_);
    return cv_.wait_for(lock, 5s, [&] { return changes_ >= n; });
  }
  json tools() {
    return s_->tools().list()["tools"];
  }
  bool listed(const std::string& name) {
    for (const json& t : tools()) {
      if (t["name"] == name) {
        return true;
      }
    }
    return false;
  }

  std::unique_ptr<session> s_;
  json initialize_;
  std::mutex mtx_;
  std::condition_variable cv_;
  int changes_ = 0;
};

// ── Pairing ──────────────────────────────────────────────────────────────────

TEST_F(web_bridge_test, PairReturnsACodeWithItsPortAndALink) {
  EXPECT_TRUE(initialize_["result"]["capabilities"]["tools"]["listChanged"].get<bool>());
  const json r = call("web_app_pair");
  ASSERT_FALSE(r.value("isError", false)) << r.dump();
  const json& sc = r["structuredContent"];
  const std::string code = sc["code"];
  EXPECT_TRUE(std::regex_match(code, std::regex("[0-9]+-[0-9A-HJKMNP-TV-Z]{4}-[0-9A-HJKMNP-TV-Z]{4}"))) << code;
  EXPECT_EQ(code.substr(0, code.find('-')), std::to_string(sc["port"].get<int>()));
  EXPECT_EQ(sc["link"], "https://app.test/nymph/#pair=" + code);
  EXPECT_EQ(sc["expiresInSeconds"], 300);
  EXPECT_NE(text_of(r).find(code), std::string::npos);
}

TEST_F(web_bridge_test, APagePairsAndItsToolsJoinTheList) {
  EXPECT_FALSE(listed("document_set_source"));
  const auto [code, port] = pair();
  fake_page page(port, {{"pair", code}, {"name", "flow.yaml"}});
  ASSERT_TRUE(page.paired()) << page.reply().dump();
  EXPECT_EQ(page.reply()["resume"].get<std::string>().size(), 32u);
  ASSERT_TRUE(wait_changes(1));
  ASSERT_TRUE(listed("document_set_source"));
  int editor_tools = 0;
  for (const json& t : tools()) {
    if (t["name"] == "document_set_source") {
      EXPECT_TRUE(t["inputSchema"]["properties"].contains("editor"));
      EXPECT_FALSE(t["inputSchema"].contains("additionalProperties"));
    }
    editor_tools += t["name"] == "editor_set_fields" ? 1 : 0;
  }
  EXPECT_EQ(editor_tools, 1); // nymph mcp's own, not the page's again

  const json r = call("document_set_source", {{"source", "root: {type: Plot}"}});
  EXPECT_EQ(text_of(r), "page:document_set_source") << r.dump();
  ASSERT_EQ(page.calls().size(), 1u);
  const json params = page.calls()[0];
  EXPECT_EQ(params["arguments"], json({{"source", "root: {type: Plot}"}}));
  EXPECT_EQ(params["_meta"]["nymph/origin"], "test-client");
}

TEST_F(web_bridge_test, ACodeWorksOnce) {
  const auto [code, port] = pair();
  fake_page first(port, {{"pair", code}});
  ASSERT_TRUE(first.paired());
  fake_page second(port, {{"pair", code}});
  EXPECT_FALSE(second.paired());
  EXPECT_NE(second.reply().value("error", std::string()).find("not valid"), std::string::npos);
}

TEST_F(web_bridge_test, ANewPairingReplacesTheCode) {
  const auto [old_code, port] = pair();
  const auto [code, port2] = pair();
  EXPECT_EQ(port, port2); // the same listener
  fake_page stale(port, {{"pair", old_code}});
  EXPECT_FALSE(stale.paired());
  fake_page page(port, {{"pair", code}});
  EXPECT_TRUE(page.paired());
}

TEST_F(web_bridge_test, ThreeWrongCodesCloseTheListener) {
  const auto [code, port] = pair();
  for (int i = 0; i < 3; ++i) {
    fake_page wrong(port, {{"pair", std::to_string(port) + "-AAAA-AAAA"}});
    EXPECT_FALSE(wrong.paired());
  }
  // Closed: even the right code cannot reach it now.
  bool reached = false;
  try {
    fake_page late(port, {{"pair", code}});
    reached = late.paired();
  } catch (const ipc::ipc_error&) {
  }
  EXPECT_FALSE(reached);
  // A new pairing opens it again.
  const auto [code2, port2] = pair();
  fake_page page(port2, {{"pair", code2}});
  EXPECT_TRUE(page.paired());
}

TEST_F(web_bridge_test, AResumeTokenReconnectsWithoutACode) {
  const auto [code, port] = pair();
  std::string token;
  {
    fake_page page(port, {{"pair", code}, {"name", "a.yaml"}});
    ASSERT_TRUE(page.paired());
    token = page.reply()["resume"];
    ASSERT_TRUE(wait_changes(1));
  }
  ASSERT_TRUE(wait_changes(2)); // the tab went away: its tools too
  EXPECT_FALSE(listed("document_set_source"));
  fake_page again(port, {{"resume", token}});
  EXPECT_TRUE(again.paired());
  ASSERT_TRUE(wait_changes(3));
  EXPECT_TRUE(listed("document_set_source"));
  fake_page forged(port, {{"resume", std::string(32, '0')}});
  EXPECT_FALSE(forged.paired());
}

TEST_F(web_bridge_test, AnOriginNotListedIsRefused) {
  const auto [code, port] = pair();
  fake_page evil(port, {{"pair", code}}, "https://evil.test");
  EXPECT_FALSE(evil.upgraded());
  fake_page none(port, {{"pair", code}}, "");
  EXPECT_FALSE(none.upgraded());
  fake_page local(port, {{"pair", code}}, "http://localhost:8123"); // a local preview, by the ":*" entry
  EXPECT_TRUE(local.paired());
}

// ── Forwarding ───────────────────────────────────────────────────────────────

TEST_F(web_bridge_test, EditorToolsGoToThePairedPage) {
  const auto [code, port] = pair();
  fake_page page(port, {{"pair", code}, {"name", "flow.yaml"}});
  ASSERT_TRUE(page.paired());
  ASSERT_TRUE(wait_changes(1));
  EXPECT_EQ(text_of(call("editor_set_fields", {{"path", "root"}})), "page:editor_set_fields");
  EXPECT_EQ(
      text_of(call("editor_set_fields", {{"path", "root"}, {"editor", "web:flow.yaml"}})), "page:editor_set_fields");
  ASSERT_EQ(page.calls().size(), 2u);
  EXPECT_FALSE(page.calls()[1]["arguments"].contains("editor")); // taken out
  const json other = call("document_state", {{"editor", "web:other.yaml"}});
  EXPECT_TRUE(other.value("isError", false));
  EXPECT_NE(text_of(other).find("web:flow.yaml"), std::string::npos) << other.dump();
}

TEST_F(web_bridge_test, FocusAndNameComeFromThePage) {
  const auto [code, port] = pair();
  fake_page page(port, {{"pair", code}, {"name", "a.yaml"}});
  ASSERT_TRUE(wait_changes(1));
  page.send({{"jsonrpc", "2.0"}, {"method", "nymph/focus"}, {"params", {{"name", "b.yaml"}}}});
  // The rename is seen once the next call is routed by it.
  for (int i = 0; i < 100; ++i) {
    if (!call("document_state", {{"editor", "web:b.yaml"}}).value("isError", false)) {
      break;
    }
    std::this_thread::sleep_for(10ms);
  }
  EXPECT_FALSE(call("document_state", {{"editor", "web:b.yaml"}}).value("isError", false));
}

TEST_F(web_bridge_test, WebAppCallReachesAnyToolOfThePage) {
  const auto [code, port] = pair();
  fake_page page(port, {{"pair", code}});
  ASSERT_TRUE(wait_changes(1));
  const json r = call("web_app_call", {{"name", "document_state"}, {"arguments", json::object()}});
  EXPECT_EQ(text_of(r), "page:document_state") << r.dump();
}

TEST_F(web_bridge_test, APageThatDoesNotAnswerFailsTheCall) {
  const auto [code, port] = pair();
  fake_page page(port, {{"pair", code}});
  ASSERT_TRUE(wait_changes(1));
  const json r = call("document_slow");
  EXPECT_TRUE(r.value("isError", false));
  EXPECT_NE(text_of(r).find("did not answer"), std::string::npos) << r.dump();
}

TEST_F(web_bridge_test, AClosedPageFailsItsCallsAndDropsItsTools) {
  const auto [code, port] = pair();
  auto page = std::make_unique<fake_page>(port, json{{"pair", code}});
  ASSERT_TRUE(wait_changes(1));
  std::thread closer([&] {
    page->wait_calls(1);
    page->close();
  });
  const json r = call("document_slow");
  closer.join();
  EXPECT_TRUE(r.value("isError", false));
  ASSERT_TRUE(wait_changes(2));
  EXPECT_FALSE(listed("document_state"));
  const json after = call("editor_set_fields", {{"path", "root"}});
  EXPECT_TRUE(after.value("isError", false));
}

// ── Without pairing ──────────────────────────────────────────────────────────

TEST_F(web_bridge_test, WithoutPairingOnlyThePairingToolsAreNew) {
  EXPECT_TRUE(listed("web_app_pair"));
  EXPECT_TRUE(listed("web_app_call"));
  EXPECT_FALSE(listed("document_state"));
  const json r = call("editor_state");
  EXPECT_TRUE(r.value("isError", false));
  EXPECT_NE(text_of(r).find("web_app_pair"), std::string::npos) << r.dump();
  const json w = call("web_app_call", {{"name", "document_state"}});
  EXPECT_TRUE(w.value("isError", false));
  EXPECT_NE(text_of(w).find("web_app_pair"), std::string::npos);
}

} // namespace
} // namespace bdg::nymph::mcp
