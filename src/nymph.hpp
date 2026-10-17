// MIT License © 2026 Binary Dice Games
/// @file nymph.hpp
/// @brief Top-level umbrella header for nymph.
#pragma once

#include <string_view>

namespace bdg::nymph {

/// @brief Returns the nymph library version as "MAJOR.MINOR.PATCH".
std::string_view version() noexcept;

} // namespace bdg::nymph
