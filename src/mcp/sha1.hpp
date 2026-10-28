// MIT License © 2026 Binary Dice Games
/// @file sha1.hpp
/// @brief SHA-1, for the WebSocket handshake's `Sec-WebSocket-Accept` only
///        (RFC 6455 section 4.2.2). Not for anything that needs a secure
///        hash.
#pragma once

#include <array>
#include <cstdint>
#include <string_view>

namespace bdg::nymph::mcp {

/// @brief The SHA-1 digest of `data`.
std::array<uint8_t, 20> sha1(std::string_view data);

} // namespace bdg::nymph::mcp
