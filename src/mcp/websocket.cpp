// MIT License © 2026 Binary Dice Games
/// @file websocket.cpp
/// @brief RFC 6455, server side (see websocket.hpp).
#include "mcp/websocket.hpp"
#include <algorithm>
#include <cctype>
#include <span>
#include "mcp/base64.hpp"
#include "mcp/sha1.hpp"

namespace bdg::nymph::mcp {

namespace {

constexpr size_t kMaxHead = 8192;
constexpr uint8_t kContinuation = 0x0, kText = 0x1, kClose = 0x8, kPing = 0x9, kPong = 0xA;

std::string lower(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return s;
}

std::string trim(std::string_view s) {
  const size_t a = s.find_first_not_of(" \t");
  if (a == std::string_view::npos) {
    return {};
  }
  const size_t b = s.find_last_not_of(" \t\r");
  return std::string(s.substr(a, b - a + 1));
}

/// Parses the request head (up to the blank line). nullopt when malformed.
std::optional<ws_request> parse_head(std::string_view head) {
  ws_request r;
  size_t pos = head.find("\r\n");
  if (pos == std::string_view::npos) {
    return std::nullopt;
  }
  const std::string_view line = head.substr(0, pos);
  const size_t sp1 = line.find(' ');
  const size_t sp2 = line.find(' ', sp1 == std::string_view::npos ? 0 : sp1 + 1);
  if (sp1 == std::string_view::npos || sp2 == std::string_view::npos) {
    return std::nullopt;
  }
  r.method = std::string(line.substr(0, sp1));
  r.path = std::string(line.substr(sp1 + 1, sp2 - sp1 - 1));
  pos += 2;
  while (pos < head.size()) {
    const size_t end = head.find("\r\n", pos);
    const std::string_view h = head.substr(pos, (end == std::string_view::npos ? head.size() : end) - pos);
    pos = end == std::string_view::npos ? head.size() : end + 2;
    if (h.empty()) {
      break;
    }
    const size_t colon = h.find(':');
    if (colon == std::string_view::npos) {
      return std::nullopt;
    }
    const std::string name = lower(std::string(h.substr(0, colon)));
    const std::string value = trim(h.substr(colon + 1));
    if (name == "host") {
      r.host = value;
    } else if (name == "origin") {
      r.origin = value;
    } else if (name == "sec-websocket-key") {
      r.key = value;
    } else if (name == "upgrade") {
      r.upgrade = lower(value);
    } else if (name == "connection") {
      r.connection = lower(value);
    } else if (name == "sec-websocket-version") {
      r.version = value;
    }
  }
  return r;
}

void answer(ipc::local_stream& s, std::string_view status) {
  s.write("HTTP/1.1 " + std::string(status) + "\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
}

} // namespace

std::string ws_accept_value(std::string_view key) {
  const auto digest = sha1(std::string(key) + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11");
  return base64_encode(std::span<const uint8_t>(digest.data(), digest.size()));
}

websocket::websocket(ipc::local_stream stream) : stream_(std::move(stream)) {}

bool websocket::accept_upgrade(std::string_view path, const std::function<bool(const ws_request&)>& allow) {
  size_t end = std::string::npos;
  while ((end = in_.find("\r\n\r\n")) == std::string::npos) {
    if (in_.size() > kMaxHead || !stream_.read_some(in_)) {
      return false;
    }
  }
  const std::optional<ws_request> request = parse_head(std::string_view(in_).substr(0, end + 2));
  in_.erase(0, end + 4);
  if (!request || request->method != "GET" || request->upgrade != "websocket" ||
      request->connection.find("upgrade") == std::string::npos || request->version != "13" ||
      request->key.empty() || request->path != path) {
    answer(stream_, "400 Bad Request");
    return false;
  }
  if (!allow(*request)) {
    answer(stream_, "403 Forbidden");
    return false;
  }
  return stream_.write("HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
                       "Sec-WebSocket-Accept: " +
                       ws_accept_value(request->key) + "\r\n\r\n");
}

bool websocket::fill(size_t n) {
  while (in_.size() < n) {
    if (!stream_.read_some(in_)) {
      return false;
    }
  }
  return true;
}

bool websocket::read_message(std::string& text) {
  text.clear();
  bool in_message = false;
  while (true) {
    if (!fill(2)) {
      return false;
    }
    const uint8_t b0 = static_cast<uint8_t>(in_[0]);
    const uint8_t b1 = static_cast<uint8_t>(in_[1]);
    const bool fin = (b0 & 0x80) != 0;
    const uint8_t opcode = b0 & 0x0F;
    const bool masked = (b1 & 0x80) != 0;
    uint64_t length = b1 & 0x7F;
    size_t head = 2;
    if ((b0 & 0x70) != 0 || !masked) {
      close(1002); // no extensions; clients must mask
      return false;
    }
    if (length == 126) {
      if (!fill(4)) {
        return false;
      }
      length = (uint64_t(uint8_t(in_[2])) << 8) | uint8_t(in_[3]);
      head = 4;
    } else if (length == 127) {
      if (!fill(10)) {
        return false;
      }
      length = 0;
      for (int i = 0; i < 8; ++i) {
        length = (length << 8) | uint8_t(in_[2 + i]);
      }
      head = 10;
    }
    const bool control = (opcode & 0x8) != 0;
    if ((control && (length > 125 || !fin)) || length > kMaxMessage || text.size() + length > kMaxMessage) {
      close(control ? 1002 : 1009);
      return false;
    }
    if (!fill(head + 4 + static_cast<size_t>(length))) {
      return false;
    }
    const std::string mask = in_.substr(head, 4);
    std::string payload = in_.substr(head + 4, static_cast<size_t>(length));
    in_.erase(0, head + 4 + static_cast<size_t>(length));
    for (size_t i = 0; i < payload.size(); ++i) {
      payload[i] = static_cast<char>(payload[i] ^ mask[i % 4]);
    }
    if (opcode == kPing) {
      send_frame(kPong, payload);
      continue;
    }
    if (opcode == kPong) {
      continue;
    }
    if (opcode == kClose) {
      close(1000);
      return false;
    }
    if (opcode == kText && !in_message) {
      in_message = true;
    } else if (opcode != kContinuation || !in_message) {
      close(opcode == 0x2 ? 1003 : 1002); // binary, or a stray continuation
      return false;
    }
    text += payload;
    if (fin) {
      return true;
    }
  }
}

bool websocket::send_frame(uint8_t opcode, std::string_view payload) {
  std::string frame;
  frame.push_back(static_cast<char>(0x80 | opcode));
  if (payload.size() < 126) {
    frame.push_back(static_cast<char>(payload.size()));
  } else if (payload.size() <= 0xFFFF) {
    frame.push_back(static_cast<char>(126));
    frame.push_back(static_cast<char>((payload.size() >> 8) & 0xFF));
    frame.push_back(static_cast<char>(payload.size() & 0xFF));
  } else {
    frame.push_back(static_cast<char>(127));
    for (int i = 7; i >= 0; --i) {
      frame.push_back(static_cast<char>((static_cast<uint64_t>(payload.size()) >> (i * 8)) & 0xFF));
    }
  }
  frame.append(payload);
  const std::lock_guard<std::mutex> lock(send_mtx_);
  if (close_sent_) {
    return false;
  }
  if (opcode == kClose) {
    close_sent_ = true;
  }
  return stream_.write(frame);
}

bool websocket::send_text(std::string_view text) {
  if (text.size() > kMaxMessage) {
    return false;
  }
  return send_frame(kText, text);
}

void websocket::close(uint16_t code) {
  const char payload[2] = {static_cast<char>(code >> 8), static_cast<char>(code & 0xFF)};
  send_frame(kClose, std::string_view(payload, 2));
}

void websocket::shutdown() {
  stream_.shutdown();
}

} // namespace bdg::nymph::mcp
