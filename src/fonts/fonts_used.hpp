// MIT License © 2026 Binary Dice Games
/// @file fonts_used.hpp
/// @brief Which catalog fonts a bound figure draws with, and which of them
///        have no bytes yet (spec/04_web_viewer/DESIGN.md, "Fonts on
///        demand").
#pragma once

#include <string>
#include <vector>
#include "document/bind.hpp"

namespace bdg::nymph {

/// @brief Every catalog font `f` draws with, sorted and unique:
///        `image.font`, every element's `font`, and the fonts element
///        classes imply (element_class::implied_fonts(), e.g. the
///        `-Italic` sibling diagrams use). ImGui's built-in font is not
///        listed (it is always available).
std::vector<std::string> fonts_used(const figure& f);

/// @brief The fonts of fonts_used() whose bytes are neither embedded nor
///        registered.
std::vector<std::string> fonts_missing(const figure& f);

/// @brief Throws if a font `f` names has no bytes.
/// @throws nymph::error at the first `font` field (or `image.font`) naming
///         a font without bytes; a font only implied by a class is not an
///         error (the class falls back to the current font).
void require_fonts(const figure& f);

} // namespace bdg::nymph
