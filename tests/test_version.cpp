// MIT License © 2026 Binary Dice Games
#include <gtest/gtest.h>
#include <nymph.hpp>
#include <version.hpp>
#include <string>

TEST(Version, MatchesProjectVersion) {
  const std::string expected = std::to_string(NYMPH_VERSION_MAJOR) + "." + std::to_string(NYMPH_VERSION_MINOR) + "." +
      std::to_string(NYMPH_VERSION_PATCH);
  EXPECT_EQ(bdg::nymph::version(), expected);
  EXPECT_EQ(bdg::nymph::version(), NYMPH_VERSION_STRING);
}
