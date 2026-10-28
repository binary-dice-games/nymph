// MIT License © 2026 Binary Dice Games
/// @file sha1.cpp
/// @brief SHA-1 (FIPS 180-4), see sha1.hpp.
#include "mcp/sha1.hpp"
#include <string>

namespace bdg::nymph::mcp {

namespace {

uint32_t rotl(uint32_t x, int n) {
  return (x << n) | (x >> (32 - n));
}

} // namespace

std::array<uint8_t, 20> sha1(std::string_view data) {
  uint32_t h[5] = {0x67452301u, 0xEFCDAB89u, 0x98BADCFEu, 0x10325476u, 0xC3D2E1F0u};
  std::string m(data);
  const uint64_t bits = static_cast<uint64_t>(data.size()) * 8u;
  m.push_back(static_cast<char>(0x80));
  while (m.size() % 64 != 56) {
    m.push_back('\0');
  }
  for (int i = 7; i >= 0; --i) {
    m.push_back(static_cast<char>((bits >> (i * 8)) & 0xFFu));
  }
  for (size_t chunk = 0; chunk < m.size(); chunk += 64) {
    uint32_t w[80];
    for (int i = 0; i < 16; ++i) {
      const auto* p = reinterpret_cast<const uint8_t*>(m.data() + chunk + i * 4);
      w[i] = (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | uint32_t(p[3]);
    }
    for (int i = 16; i < 80; ++i) {
      w[i] = rotl(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
    }
    uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4];
    for (int i = 0; i < 80; ++i) {
      uint32_t f, k;
      if (i < 20) {
        f = (b & c) | (~b & d);
        k = 0x5A827999u;
      } else if (i < 40) {
        f = b ^ c ^ d;
        k = 0x6ED9EBA1u;
      } else if (i < 60) {
        f = (b & c) | (b & d) | (c & d);
        k = 0x8F1BBCDCu;
      } else {
        f = b ^ c ^ d;
        k = 0xCA62C1D6u;
      }
      const uint32_t t = rotl(a, 5) + f + e + k + w[i];
      e = d;
      d = c;
      c = rotl(b, 30);
      b = a;
      a = t;
    }
    h[0] += a;
    h[1] += b;
    h[2] += c;
    h[3] += d;
    h[4] += e;
  }
  std::array<uint8_t, 20> out{};
  for (int i = 0; i < 5; ++i) {
    out[i * 4] = static_cast<uint8_t>(h[i] >> 24);
    out[i * 4 + 1] = static_cast<uint8_t>(h[i] >> 16);
    out[i * 4 + 2] = static_cast<uint8_t>(h[i] >> 8);
    out[i * 4 + 3] = static_cast<uint8_t>(h[i]);
  }
  return out;
}

} // namespace bdg::nymph::mcp
