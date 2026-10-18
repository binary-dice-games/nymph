// MIT License © 2026 Binary Dice Games
/// @file font_testing.hpp
/// @brief Test hook: makes a build with every font embedded behave like one
///        built with `NYMPH_EMBED_FONTS=none`, so the on-demand path is
///        tested without compiling the library twice.
#pragma once

namespace bdg::nymph {

/// @brief While `hide` is true, font_data() ignores embedded bytes and
///        returns only registered ones. Not for production use.
void hide_embedded_fonts_for_testing(bool hide);

} // namespace bdg::nymph
