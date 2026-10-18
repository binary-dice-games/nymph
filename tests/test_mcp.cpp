// MIT License © 2026 Binary Dice Games
/// @file test_mcp.cpp
/// @brief spec/06_claude_integration/PLAN.md step 2: `nymph mcp`'s JSON-RPC
///        layer and MCP session, driven in-process through string streams.
#include <gtest/gtest.h>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>
#include "mcp/json_rpc.hpp"
#include "mcp/mcp_server.hpp"
#include "mcp/mcp_session.hpp"
#include "nymph.hpp"

namespace bdg::nymph::mcp {
namespace {

// ── Helpers ─────────────────────────────────────────────────────────────────

/// What one run_mcp() call wrote.
struct run_result {
  int code = 0;
  std::vector<json> responses; ///< one per output line
  std::string log;
};

/// Runs the server over `input`; every output line must be one JSON object.
run_result serve(const std::string& input) {
  std::istringstream in(input);
  std::ostringstream out;
  std::ostringstream log;
  run_result r;
  r.code = run_mcp(in, out, log);
  r.log = log.str();
  std::istringstream lines(out.str());
  std::string line;
  while (std::getline(lines, line)) {
    json value = json::parse(line, nullptr, false);
    EXPECT_FALSE(value.is_discarded()) << "not JSON: " << line;
    EXPECT_TRUE(value.is_object()) << line;
    r.responses.push_back(value);
  }
  EXPECT_TRUE(out.str().empty() || out.str().back() == '\n');
  return r;
}

std::string request(int id, const std::string& method, const json& params = nullptr) {
  json m{{"jsonrpc", "2.0"}, {"id", id}, {"method", method}};
  if (!params.is_null()) {
    m["params"] = params;
  }
  return m.dump() + "\n";
}

std::string notification(const std::string& method) {
  return json{{"jsonrpc", "2.0"}, {"method", method}}.dump() + "\n";
}

std::string initialize(int id = 1, const std::string& version = "2025-11-25") {
  return request(
      id,
      "initialize",
      {{"protocolVersion", version}, {"capabilities", json::object()}, {"clientInfo", {{"name", "test"}}}});
}

int error_code(const json& response) {
  return response.at("error").at("code").get<int>();
}

// ── Lifecycle ───────────────────────────────────────────────────────────────

TEST(mcp_session, initialize_returns_each_supported_version) {
  for (std::string_view version : supported_protocol_versions()) {
    const run_result r = serve(initialize(1, std::string(version)));
    ASSERT_EQ(r.responses.size(), 1u);
    EXPECT_EQ(r.responses[0]["result"]["protocolVersion"], version);
  }
}

TEST(mcp_session, initialize_answers_an_unsupported_version_with_the_newest) {
  const run_result r = serve(initialize(1, "2024-11-05"));
  ASSERT_EQ(r.responses.size(), 1u);
  EXPECT_EQ(r.responses[0]["result"]["protocolVersion"], supported_protocol_versions().front());
}

TEST(mcp_session, initialize_reports_the_server_and_its_capabilities) {
  const run_result r = serve(initialize(7));
  ASSERT_EQ(r.responses.size(), 1u);
  const json& response = r.responses[0];
  EXPECT_EQ(response["jsonrpc"], "2.0");
  EXPECT_EQ(response["id"], 7);
  EXPECT_EQ(response["result"]["serverInfo"]["name"], "nymph");
  EXPECT_EQ(response["result"]["serverInfo"]["version"], std::string(version()));
  EXPECT_TRUE(response["result"]["capabilities"].contains("tools"));
  EXPECT_TRUE(response["result"]["capabilities"].contains("resources"));
}

TEST(mcp_session, initialize_needs_a_protocol_version) {
  const run_result r = serve(request(1, "initialize", {{"capabilities", json::object()}}));
  ASSERT_EQ(r.responses.size(), 1u);
  EXPECT_EQ(error_code(r.responses[0]), -32602);
}

TEST(mcp_session, a_second_initialize_is_refused) {
  const run_result r = serve(initialize(1) + initialize(2));
  ASSERT_EQ(r.responses.size(), 2u);
  EXPECT_TRUE(r.responses[0].contains("result"));
  EXPECT_EQ(error_code(r.responses[1]), -32600);
}

TEST(mcp_session, lists_are_refused_before_initialize) {
  const run_result r = serve(request(1, "tools/list") + request(2, "resources/list"));
  ASSERT_EQ(r.responses.size(), 2u);
  EXPECT_EQ(error_code(r.responses[0]), -32600);
  EXPECT_EQ(error_code(r.responses[1]), -32600);
}

TEST(mcp_session, lists_answer_after_initialize) {
  const run_result r = serve(
      initialize() + notification("notifications/initialized") + request(2, "tools/list") +
      request(3, "resources/list", {{"cursor", "x"}}));
  ASSERT_EQ(r.responses.size(), 3u);
  EXPECT_EQ(r.responses[1]["result"]["tools"].size(), 5u);
  EXPECT_EQ(r.responses[2]["result"], json({{"resources", json::array()}}));
}

TEST(mcp_session, ping_works_before_and_after_initialize) {
  const run_result r = serve(request(1, "ping") + initialize(2) + request(3, "ping"));
  ASSERT_EQ(r.responses.size(), 3u);
  EXPECT_EQ(r.responses[0]["result"], json::object());
  EXPECT_EQ(r.responses[2]["result"], json::object());
}

TEST(mcp_session, a_newer_revisions_discovery_gets_method_not_found) {
  // Claude Code 2.1.302 tries the 2026-07-28 revision first and falls back
  // to initialize on -32601 (PLAN step 1).
  const run_result r = serve(
      json{{"jsonrpc", "2.0"}, {"id", "server-discover-probe-1"}, {"method", "server/discover"}}.dump() + "\n" +
      initialize(0));
  ASSERT_EQ(r.responses.size(), 2u);
  EXPECT_EQ(r.responses[0]["id"], "server-discover-probe-1");
  EXPECT_EQ(error_code(r.responses[0]), -32601);
  EXPECT_TRUE(r.responses[1].contains("result"));
}

// ── JSON-RPC ────────────────────────────────────────────────────────────────

TEST(mcp_json_rpc, unknown_methods_get_method_not_found) {
  const run_result r = serve(initialize() + request(2, "nymph/unknown"));
  ASSERT_EQ(r.responses.size(), 2u);
  EXPECT_EQ(r.responses[1]["id"], 2);
  EXPECT_EQ(error_code(r.responses[1]), -32601);
}

TEST(mcp_json_rpc, a_malformed_line_gets_a_parse_error_and_the_next_line_works) {
  const run_result r = serve("{\"jsonrpc\": \"2.0\", \"id\": 1, \"method\": \n" + request(2, "ping"));
  ASSERT_EQ(r.responses.size(), 2u);
  EXPECT_EQ(error_code(r.responses[0]), -32700);
  EXPECT_TRUE(r.responses[0]["id"].is_null());
  EXPECT_EQ(r.responses[1]["id"], 2);
}

TEST(mcp_json_rpc, invalid_utf8_is_a_parse_error) {
  const run_result r = serve("{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"p\xff\"}\n");
  ASSERT_EQ(r.responses.size(), 1u);
  EXPECT_EQ(error_code(r.responses[0]), -32700);
}

TEST(mcp_json_rpc, a_batch_is_an_invalid_request) {
  const run_result r = serve("[" + request(1, "ping").substr(0, request(1, "ping").size() - 1) + "]\n");
  ASSERT_EQ(r.responses.size(), 1u);
  EXPECT_EQ(error_code(r.responses[0]), -32600);
  EXPECT_TRUE(r.responses[0]["id"].is_null());
}

TEST(mcp_json_rpc, malformed_requests_are_invalid_and_keep_their_id) {
  const run_result r = serve(
      "{\"id\": 1, \"method\": \"ping\"}\n" // no jsonrpc
      "{\"jsonrpc\": \"2.0\", \"id\": 2, \"method\": 5}\n" // method not a string
      "{\"jsonrpc\": \"2.0\", \"id\": 3, \"method\": \"ping\", \"params\": 1}\n"
      "{\"jsonrpc\": \"2.0\", \"id\": 1.5, \"method\": \"ping\"}\n" // fractional id
      "{\"jsonrpc\": \"2.0\", \"id\": null, \"method\": \"ping\"}\n" // null id
      "\"ping\"\n");
  ASSERT_EQ(r.responses.size(), 6u);
  for (const json& response : r.responses) {
    EXPECT_EQ(error_code(response), -32600) << response.dump();
  }
  EXPECT_EQ(r.responses[0]["id"], 1);
  EXPECT_EQ(r.responses[1]["id"], 2);
  EXPECT_EQ(r.responses[2]["id"], 3);
  EXPECT_TRUE(r.responses[3]["id"].is_null());
}

TEST(mcp_json_rpc, notifications_and_client_responses_get_no_response) {
  const run_result r = serve(
      notification("notifications/initialized") + notification("notifications/cancelled") +
      notification("nymph/unknown") + "{\"jsonrpc\": \"2.0\", \"id\": 9, \"result\": {}}\n");
  EXPECT_TRUE(r.responses.empty());
  EXPECT_EQ(r.code, 0);
}

TEST(mcp_json_rpc, string_ids_are_echoed) {
  const run_result r = serve(json{{"jsonrpc", "2.0"}, {"id", "abc"}, {"method", "ping"}}.dump() + "\n");
  ASSERT_EQ(r.responses.size(), 1u);
  EXPECT_EQ(r.responses[0]["id"], "abc");
}

TEST(mcp_json_rpc, blank_lines_and_crlf_endings_are_accepted) {
  std::string input = "\n  \r\n" + request(1, "ping");
  input.insert(input.size() - 1, "\r");
  const run_result r = serve(input);
  ASSERT_EQ(r.responses.size(), 1u);
  EXPECT_EQ(r.responses[0]["id"], 1);
}

TEST(mcp_json_rpc, invalid_utf8_in_output_is_replaced) {
  const std::string line = to_line(json{{"text", std::string("a\xff") + "b\nc"}});
  EXPECT_EQ(line.find('\n'), std::string::npos);
  EXPECT_FALSE(json::parse(line, nullptr, false).is_discarded());
}

// ── Server loop and command line ────────────────────────────────────────────

TEST(mcp_server, end_of_input_returns_zero) {
  EXPECT_EQ(serve("").code, 0);
  EXPECT_EQ(serve(request(1, "ping")).code, 0);
}

TEST(mcp_server, an_unwritable_output_returns_one) {
  std::istringstream in(request(1, "ping") + request(2, "ping"));
  std::ostringstream out;
  out.setstate(std::ios::badbit);
  std::ostringstream log;
  EXPECT_EQ(run_mcp(in, out, log), 1);
  EXPECT_NE(log.str().find("cannot write"), std::string::npos);
}

int run_cli(std::vector<std::string_view> args, std::string& out, std::string& err) {
  std::istringstream in;
  std::ostringstream o;
  std::ostringstream e;
  const int code = run_mcp_cli(args, in, o, e);
  out = o.str();
  err = e.str();
  return code;
}

TEST(mcp_server, bad_arguments_exit_two_with_the_usage) {
  std::string out;
  std::string err;
  EXPECT_EQ(run_cli({"--bogus"}, out, err), 2);
  EXPECT_NE(err.find("unknown argument '--bogus'"), std::string::npos);
  EXPECT_NE(err.find("usage: nymph mcp"), std::string::npos);
  EXPECT_TRUE(out.empty());
  EXPECT_EQ(run_cli({"--rasterizer"}, out, err), 2);
  EXPECT_EQ(run_cli({"--rasterizer", "vulkan"}, out, err), 2);
  EXPECT_NE(err.find("unknown rasterizer 'vulkan'"), std::string::npos);
}

TEST(mcp_server, help_prints_the_usage_and_valid_arguments_serve) {
  std::string out;
  std::string err;
  EXPECT_EQ(run_cli({"--help"}, out, err), 0);
  EXPECT_NE(out.find("usage: nymph mcp"), std::string::npos);
  EXPECT_EQ(run_cli({"--rasterizer", "software"}, out, err), 0);
  EXPECT_TRUE(out.empty());
  EXPECT_TRUE(err.empty());
}

} // namespace
} // namespace bdg::nymph::mcp
