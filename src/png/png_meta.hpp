// MIT License © 2026 Binary Dice Games
/// @file png_meta.hpp
/// @brief PNG encoding with the nymph source embedded as metadata, and
///        reading that source back without decoding any pixels.
///
/// The source is an `iTXt` chunk with keyword `nymph`, placed directly
/// after `IHDR`: stored as is up to kInlineSourceBytes, zlib-compressed
/// above. The description is a second `iTXt` chunk with the standard
/// keyword `Description`, omitted when empty. The layout is wish nymph's,
/// so each tool reads the other's images.
#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>
#include "render/image.hpp"

namespace bdg::nymph {

/// @brief Sources up to this size are embedded uncompressed, so plain text
///        tools (`strings`, `grep`) can read them straight out of the PNG.
inline constexpr size_t kInlineSourceBytes = 64u * 1024u;

/// @brief True if `bytes` starts with the 8-byte PNG signature.
bool is_png(std::span<const uint8_t> bytes);

/// @brief Encodes `img` as a PNG carrying `source` and `description`.
/// @return The PNG file bytes. The same inputs give the same bytes.
/// @throws std::runtime_error if `img` is empty or inconsistent, or
///         encoding fails.
std::vector<uint8_t> encode_png(const image& img, std::string_view source, std::string_view description);

/// @brief Reads the nymph source embedded in a PNG, checking the signature
///        and every chunk CRC on the way; pixels are never decoded.
/// @return The source text, byte for byte as encoded; `std::nullopt` if
///         `bytes` is not a PNG or has no `nymph` chunk.
/// @throws std::runtime_error if the PNG is truncated, a chunk fails its
///         CRC, or the embedded text cannot be decompressed or is larger
///         than kMaxSourceBytes.
std::optional<std::string> read_source(std::span<const uint8_t> bytes);

/// @brief CRC-32 as PNG uses it (ISO 3309 / ITU-T V.42, polynomial
///        0xEDB88320, initial and final XOR 0xFFFFFFFF).
uint32_t crc32(std::span<const uint8_t> bytes);

} // namespace bdg::nymph
