// MIT License © 2026 Binary Dice Games
/// @file sha256.hpp
/// @brief SHA-256 (FIPS 180-4), to check font bytes against the font
///        catalog.
#pragma once

#include <cstdint>
#include <span>
#include <string>

namespace bdg::nymph {

/// @brief The SHA-256 of `bytes` as 64 lowercase hex digits.
std::string sha256_hex(std::span<const uint8_t> bytes);

} // namespace bdg::nymph
