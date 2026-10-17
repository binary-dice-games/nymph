// MIT License © 2026 Binary Dice Games
/// @file nymph.hpp
/// @brief nymph's public library API: a YAML chart source to a PNG that
///        carries it, and back.
#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>
#include "core/error.hpp"

namespace bdg::nymph {

/// @brief Returns the nymph library version as "MAJOR.MINOR.PATCH".
std::string_view version() noexcept;

/// @brief Validates and renders a source with the built-in elements.
/// @return PNG bytes with `source` embedded verbatim (and its description).
///         The same source and build give the same bytes.
/// @throws nymph::error for a problem in the source (positioned);
///         std::runtime_error for a rendering or encoding failure.
std::vector<uint8_t> render_png(std::string_view source);

/// @brief Validates a source without rendering it.
/// @throws nymph::error for the first problem found.
void check_source(std::string_view source);

/// @brief The source a nymph PNG carries, byte for byte.
/// @return nullopt if `png` is not a PNG or has no nymph source.
/// @throws std::runtime_error for a corrupt or truncated PNG.
std::optional<std::string> extract_source(std::span<const uint8_t> png);

} // namespace bdg::nymph
