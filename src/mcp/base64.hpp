// MIT License © 2026 Binary Dice Games
/// @file base64.hpp
/// @brief Base64 (RFC 4648, standard alphabet, padded) for images in MCP
///        tool results and arguments.
#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace bdg::nymph::mcp {

/// @brief The bytes as padded base64.
std::string base64_encode(std::span<const uint8_t> bytes);

/// @brief Decodes padded or unpadded base64; whitespace is skipped.
/// @return nullopt for any other character, misplaced padding or a
///         truncated final group.
std::optional<std::vector<uint8_t>> base64_decode(std::string_view text);

} // namespace bdg::nymph::mcp
