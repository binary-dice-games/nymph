// MIT License © 2026 Binary Dice Games
/// @file nymph.cpp
/// @brief Library-wide entry points.
#include "nymph.hpp"
#include "version.hpp"

namespace bdg::nymph {

std::string_view version() noexcept {
  return NYMPH_VERSION_STRING;
}

} // namespace bdg::nymph
