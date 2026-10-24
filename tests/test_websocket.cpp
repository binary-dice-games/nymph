// MIT License © 2026 Binary Dice Games
/// @file test_websocket.cpp
/// @brief spec/17_agent_connectors plan step 6: SHA-1, the RFC 6455
///        handshake and framing over a loopback TCP socket, and the cases a
///        server must refuse.
#include <gtest/gtest.h>
#include <future>
#include <memory>
#include <string>
#include <thread>
#include "ipc/loopback.hpp"
#include "mcp/sha1.hpp"
#include "mcp/websocket.hpp"
#include "ws_test_client.hpp"

namespace bdg::nymph::mcp {
namespace {

std::string hex(const std::array<uint8_t, 20>& d) {
  static const char* digits = "0123456789abcdef";
  std::string s;
  for (const uint8_t b : d) {
    s.push_back(digits[b >> 4]);
    s.push_back(digits[b & 0xF]);
  }
  return s;
}

/// A server that upgrades one connection (Host and Origin checked) and
/// echoes each message as "echo <text>" until the connection ends.
struct echo_server {
  ipc::loopback_listener listener{0};
  std::promise<bool> upgraded;
  std::promise<int> messages;
  std::thread thread;
  std::unique_ptr<websocket> ws;

  echo_server() {
    thread = std::thread([this] {
      std::optional<ipc::local_stream> s = listener.accept();
      if (!s) {
        upgraded.set_value(false);
        messages.set_value(0);
        return;
      }
      ws = std::make_unique<websocket>(std::move(*s));
      const std::string host = "127.0.0.1:" + std::to_string(listener.port());
      const bool ok = ws->accept_upgrade(
          "/nymph", [&](const ws_request& r) { return r.host == host && r.origin == "https://app.test"; });
      upgraded.set_value(ok);
      int n = 0;
      std::string text;
      while (ok && ws->read_message(text)) {
        ++n;
        ws->send_text("echo " + text);
      }
      messages.set_value(n);
    });
  }
  ~echo_server() {
    listener.stop();
    if (ws) {
      ws->shutdown();
    }
    thread.join();
  }
};

// ── SHA-1 and the accept value ───────────────────────────────────────────────

TEST(websocket, Sha1TestVectors) {
  EXPECT_EQ(hex(sha1("")), "da39a3ee5e6b4b0d3255bfef95601890afd80709");
  EXPECT_EQ(hex(sha1("abc")), "a9993e364706816aba3e25717850c26c9cd0d89d");
  EXPECT_EQ(
      hex(sha1("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq")),
      "84983e441c3bd26ebaae4aa1f95129e5e54670f1");
  EXPECT_EQ(hex(sha1(std::string(1000, 'a'))), "291e9a6c66994949b57ba5e650361e98fc36b1ba");
}

TEST(websocket, AcceptValueOfTheRfcExample) {
  EXPECT_EQ(ws_accept_value("dGhlIHNhbXBsZSBub25jZQ=="), "s3pPLMBiTxaQ9kYGzzhZRbK+xOo=");
}

// ── Messages ─────────────────────────────────────────────────────────────────

TEST(websocket, MessagesGoBothWaysInOrder) {
  echo_server server;
  test::ws_client c(server.listener.port(), "https://app.test");
  ASSERT_TRUE(c.upgraded()) << c.status();
  ASSERT_TRUE(c.send("one"));
  ASSERT_TRUE(c.send(std::string(70000, 'x'))); // a 64-bit length
  ASSERT_TRUE(c.send_frame(0x1, "fra", false)); // fragments, with a ping between
  ASSERT_TRUE(c.send_frame(0x9, "hi"));
  ASSERT_TRUE(c.send_frame(0x0, "gment", true));
  EXPECT_EQ(c.read(), "echo one");
  const std::optional<std::string> big = c.read();
  ASSERT_TRUE(big);
  EXPECT_EQ(big->size(), 70005u);
  const auto pong = c.read_frame();
  ASSERT_TRUE(pong);
  EXPECT_EQ(pong->first, 0xA);
  EXPECT_EQ(pong->second, "hi");
  EXPECT_EQ(c.read(), "echo fragment");
  ASSERT_TRUE(c.send_frame(0x8, std::string("\x03\xe8", 2)));
  const auto close = c.read_frame();
  ASSERT_TRUE(close);
  EXPECT_EQ(close->first, 0x8); // the close is answered
  EXPECT_EQ(server.messages.get_future().get(), 3);
}

TEST(websocket, AnUnmaskedClientFrameClosesTheConnection) {
  echo_server server;
  test::ws_client c(server.listener.port(), "https://app.test");
  ASSERT_TRUE(c.upgraded());
  ASSERT_TRUE(c.send_frame(0x1, "plain", true, false));
  const auto f = c.read_frame();
  ASSERT_TRUE(f);
  EXPECT_EQ(f->first, 0x8);
  EXPECT_EQ(server.messages.get_future().get(), 0);
}

TEST(websocket, ABinaryMessageClosesTheConnection) {
  echo_server server;
  test::ws_client c(server.listener.port(), "https://app.test");
  ASSERT_TRUE(c.upgraded());
  ASSERT_TRUE(c.send_frame(0x2, "bytes"));
  const auto f = c.read_frame();
  ASSERT_TRUE(f);
  EXPECT_EQ(f->first, 0x8);
  EXPECT_EQ(f->second, std::string("\x03\xeb", 2)); // 1003
}

TEST(websocket, AnOversizedMessageClosesTheConnection) {
  echo_server server;
  test::ws_client c(server.listener.port(), "https://app.test");
  ASSERT_TRUE(c.upgraded());
  // Only the header: the length alone is refused, before any payload.
  std::string header = "\x81\xff";
  for (int i = 7; i >= 0; --i) {
    header.push_back(static_cast<char>((static_cast<uint64_t>(websocket::kMaxMessage + 1) >> (i * 8)) & 0xFF));
  }
  header += std::string("\x01\x02\x03\x04", 4);
  ASSERT_TRUE(c.send_raw(header));
  const auto f = c.read_frame();
  ASSERT_TRUE(f);
  EXPECT_EQ(f->first, 0x8);
  EXPECT_EQ(f->second, std::string("\x03\xf1", 2)); // 1009
}

// ── The upgrade ──────────────────────────────────────────────────────────────

TEST(websocket, AWrongOriginIsRefused) {
  echo_server server;
  test::ws_client c(server.listener.port(), "https://evil.test");
  EXPECT_FALSE(c.upgraded());
  EXPECT_NE(c.status().find("403"), std::string::npos) << c.status();
  EXPECT_FALSE(server.upgraded.get_future().get());
}

TEST(websocket, AWrongHostIsRefused) {
  echo_server server;
  test::ws_client c(server.listener.port(), "https://app.test", "attacker.example:80");
  EXPECT_NE(c.status().find("403"), std::string::npos) << c.status();
}

TEST(websocket, AnotherPathIsABadRequest) {
  echo_server server;
  test::ws_client c(server.listener.port(), "https://app.test", "", "/other");
  EXPECT_NE(c.status().find("400"), std::string::npos) << c.status();
}

TEST(websocket, StoppingTheListenerEndsABlockedAccept) {
  auto listener = std::make_unique<ipc::loopback_listener>(0);
  std::thread t([&] { EXPECT_FALSE(listener->accept()); });
  listener->stop();
  t.join();
}

} // namespace
} // namespace bdg::nymph::mcp
