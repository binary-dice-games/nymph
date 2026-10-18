// MIT License © 2026 Binary Dice Games
/// @file base64.cpp
/// @brief Base64 encoding and decoding.
#include "mcp/base64.hpp"

namespace bdg::nymph::mcp {

namespace {

constexpr char kAlphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

int value_of(char c) {
  if (c >= 'A' && c <= 'Z') {
    return c - 'A';
  }
  if (c >= 'a' && c <= 'z') {
    return c - 'a' + 26;
  }
  if (c >= '0' && c <= '9') {
    return c - '0' + 52;
  }
  if (c == '+') {
    return 62;
  }
  if (c == '/') {
    return 63;
  }
  return -1;
}

} // namespace

std::string base64_encode(std::span<const uint8_t> bytes) {
  std::string text;
  text.reserve((bytes.size() + 2) / 3 * 4);
  size_t i = 0;
  for (; i + 2 < bytes.size(); i += 3) {
    const uint32_t v = (uint32_t(bytes[i]) << 16) | (uint32_t(bytes[i + 1]) << 8) | bytes[i + 2];
    text += kAlphabet[(v >> 18) & 63];
    text += kAlphabet[(v >> 12) & 63];
    text += kAlphabet[(v >> 6) & 63];
    text += kAlphabet[v & 63];
  }
  if (i < bytes.size()) {
    const bool two = i + 1 < bytes.size();
    const uint32_t v = (uint32_t(bytes[i]) << 16) | (two ? uint32_t(bytes[i + 1]) << 8 : 0);
    text += kAlphabet[(v >> 18) & 63];
    text += kAlphabet[(v >> 12) & 63];
    text += two ? kAlphabet[(v >> 6) & 63] : '=';
    text += '=';
  }
  return text;
}

std::optional<std::vector<uint8_t>> base64_decode(std::string_view text) {
  std::vector<uint8_t> bytes;
  bytes.reserve(text.size() / 4 * 3);
  uint32_t group = 0;
  int count = 0; // sextets in the current group
  int padding = 0; // '=' seen
  for (const char c : text) {
    if (c == ' ' || c == '\n' || c == '\r' || c == '\t') {
      continue;
    }
    if (c == '=') {
      ++padding;
      continue;
    }
    const int v = value_of(c);
    if (v < 0 || padding > 0) {
      return std::nullopt; // a foreign character, or data after padding
    }
    group = (group << 6) | uint32_t(v);
    if (++count == 4) {
      bytes.push_back(uint8_t(group >> 16));
      bytes.push_back(uint8_t(group >> 8));
      bytes.push_back(uint8_t(group));
      group = 0;
      count = 0;
    }
  }
  if (count == 1 || padding > 2 || (padding > 0 && count + padding != 4)) {
    return std::nullopt;
  }
  if (count == 2) {
    bytes.push_back(uint8_t(group >> 4));
  } else if (count == 3) {
    bytes.push_back(uint8_t(group >> 10));
    bytes.push_back(uint8_t(group >> 2));
  }
  return bytes;
}

} // namespace bdg::nymph::mcp
