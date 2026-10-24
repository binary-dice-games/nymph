// MIT License © 2026 Binary Dice Games
/// @file ws_test_client.hpp
/// @brief A WebSocket client for tests, standing in for a browser tab
///        (spec/17_agent_connectors plan steps 6 and 7): the upgrade with a
///        chosen Host and Origin, masked text frames, raw frames for the
///        cases a server must refuse.
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include "ipc/loopback.hpp"

namespace bdg::nymph::test {

class ws_client {
 public:
  /// Connects to 127.0.0.1:`port` and asks for the upgrade.
  ws_client(uint16_t port, const std::string& origin, std::string host = {}, std::string path = "/nymph")
      : stream_(ipc::loopback_connect(port)) {
    if (host.empty()) {
      host = "127.0.0.1:" + std::to_string(port);
    }
    stream_.write("GET " + path + " HTTP/1.1\r\nHost: " + host + "\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n" +
                  (origin.empty() ? std::string() : "Origin: " + origin + "\r\n") +
                  "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\nSec-WebSocket-Version: 13\r\n\r\n");
    size_t end = std::string::npos;
    while ((end = in_.find("\r\n\r\n")) == std::string::npos) {
      if (!stream_.read_some(in_)) {
        status_ = in_;
        return;
      }
    }
    status_ = in_.substr(0, in_.find("\r\n"));
    in_.erase(0, end + 4);
  }

  /// The response's status line ("HTTP/1.1 101 Switching Protocols").
  const std::string& status() const {
    return status_;
  }
  bool upgraded() const {
    return status_.find(" 101 ") != std::string::npos;
  }

  /// Sends a frame; `mask` false makes an invalid client frame.
  bool send_frame(uint8_t opcode, const std::string& payload, bool fin = true, bool mask = true) {
    std::string f;
    f.push_back(static_cast<char>((fin ? 0x80 : 0) | opcode));
    const uint8_t m = mask ? 0x80 : 0;
    if (payload.size() < 126) {
      f.push_back(static_cast<char>(m | payload.size()));
    } else if (payload.size() <= 0xFFFF) {
      f.push_back(static_cast<char>(m | 126));
      f.push_back(static_cast<char>(payload.size() >> 8));
      f.push_back(static_cast<char>(payload.size() & 0xFF));
    } else {
      f.push_back(static_cast<char>(m | 127));
      for (int i = 7; i >= 0; --i) {
        f.push_back(static_cast<char>((static_cast<uint64_t>(payload.size()) >> (i * 8)) & 0xFF));
      }
    }
    const char key[4] = {0x12, 0x34, 0x56, 0x78};
    if (mask) {
      f.append(key, 4);
    }
    for (size_t i = 0; i < payload.size(); ++i) {
      f.push_back(mask ? static_cast<char>(payload[i] ^ key[i % 4]) : payload[i]);
    }
    return stream_.write(f);
  }
  /// Sends bytes as they are (a frame header alone, say).
  bool send_raw(const std::string& bytes) {
    return stream_.write(bytes);
  }
  bool send(const std::string& text) {
    return send_frame(0x1, text);
  }

  /// The next frame from the server: its opcode and payload; nullopt when
  /// the connection ended.
  std::optional<std::pair<uint8_t, std::string>> read_frame() {
    if (!fill(2)) {
      return std::nullopt;
    }
    const uint8_t opcode = static_cast<uint8_t>(in_[0]) & 0x0F;
    uint64_t length = static_cast<uint8_t>(in_[1]) & 0x7F;
    size_t head = 2;
    if (length == 126) {
      if (!fill(4)) {
        return std::nullopt;
      }
      length = (uint64_t(uint8_t(in_[2])) << 8) | uint8_t(in_[3]);
      head = 4;
    } else if (length == 127) {
      if (!fill(10)) {
        return std::nullopt;
      }
      length = 0;
      for (int i = 0; i < 8; ++i) {
        length = (length << 8) | uint8_t(in_[2 + i]);
      }
      head = 10;
    }
    if (!fill(head + length)) {
      return std::nullopt;
    }
    std::string payload = in_.substr(head, static_cast<size_t>(length));
    in_.erase(0, head + static_cast<size_t>(length));
    return std::make_pair(opcode, std::move(payload));
  }

  /// The next text message (pings and pongs skipped); nullopt at a close or
  /// the end.
  std::optional<std::string> read() {
    while (auto f = read_frame()) {
      if (f->first == 0x1) {
        return f->second;
      }
      if (f->first == 0x8) {
        return std::nullopt;
      }
    }
    return std::nullopt;
  }

  void close() {
    stream_.shutdown();
  }

 private:
  bool fill(size_t n) {
    while (in_.size() < n) {
      if (!stream_.read_some(in_)) {
        return false;
      }
    }
    return true;
  }
  ipc::local_stream stream_;
  std::string in_;
  std::string status_;
};

} // namespace bdg::nymph::test
