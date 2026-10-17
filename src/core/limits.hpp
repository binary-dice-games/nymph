// MIT License © 2026 Binary Dice Games
/// @file limits.hpp
/// @brief Input size limits, checked before the allocations they bound
///        (spec/01_core_features/DESIGN.md section 8).
#pragma once

#include <cstddef>

namespace bdg::nymph {

/// @brief Largest accepted source text, and largest source extracted from a
///        PNG: 16 MiB.
inline constexpr size_t kMaxSourceBytes = 16u * 1024u * 1024u;

/// @brief Largest number of data rows in a source's `data`.
inline constexpr size_t kMaxDataRows = 1000000u;

} // namespace bdg::nymph
