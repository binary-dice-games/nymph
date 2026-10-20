// MIT License © 2026 Binary Dice Games
/// @file test_app_icon.cpp
/// @brief The icon of nymph.exe: resources/icon.ico holds every size
///        Explorer asks for, and on Windows the built executable carries it
///        as its first icon resource (app/nymph.rc.in).
#include <gtest/gtest.h>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <vector>
#include "png/png_meta.hpp"
#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace bdg::nymph {
namespace {

std::vector<uint8_t> read_bytes(const std::string& path) {
  std::ifstream in(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

uint32_t le(const std::vector<uint8_t>& b, size_t at, int bytes) {
  uint32_t v = 0;
  for (int i = bytes - 1; i >= 0; --i) {
    v = v << 8 | b[at + static_cast<size_t>(i)];
  }
  return v;
}

TEST(app_icon, IcoHoldsEverySizeAsSquareImages) {
  const std::vector<uint8_t> ico = read_bytes(std::string(NYMPH_SOURCE_DIR) + "/resources/icon.ico");
  ASSERT_GE(ico.size(), 6u);
  EXPECT_EQ(le(ico, 0, 2), 0u); // reserved
  EXPECT_EQ(le(ico, 2, 2), 1u); // type: icon
  const uint32_t count = le(ico, 4, 2);
  ASSERT_GE(ico.size(), 6u + 16u * count);
  std::set<int> sizes;
  for (uint32_t i = 0; i < count; ++i) {
    const size_t entry = 6 + 16 * static_cast<size_t>(i);
    const int size = ico[entry] == 0 ? 256 : ico[entry]; // 0 means 256
    const uint32_t bytes = le(ico, entry + 8, 4);
    const uint32_t offset = le(ico, entry + 12, 4);
    ASSERT_LE(static_cast<size_t>(offset) + bytes, ico.size());
    const std::optional<image> pixels = decode_png(std::span<const uint8_t>(ico.data() + offset, bytes));
    ASSERT_TRUE(pixels.has_value()) << "entry " << i << " is a PNG";
    EXPECT_EQ(pixels->width, size);
    EXPECT_EQ(pixels->height, size);
    sizes.insert(size);
  }
  for (const int size : {16, 24, 32, 48, 256}) {
    EXPECT_TRUE(sizes.count(size)) << size << " px is missing";
  }
}

#if defined(_WIN32)
TEST(app_icon, ExecutableCarriesTheIconResource) {
  const std::wstring exe = std::filesystem::path(NYMPH_EXE).wstring();
  HMODULE module = LoadLibraryExW(exe.c_str(), nullptr, LOAD_LIBRARY_AS_DATAFILE | LOAD_LIBRARY_AS_IMAGE_RESOURCE);
  ASSERT_NE(module, nullptr) << "cannot open " << NYMPH_EXE;
  EXPECT_NE(FindResourceW(module, MAKEINTRESOURCEW(1), MAKEINTRESOURCEW(14) /* RT_GROUP_ICON */), nullptr);
  FreeLibrary(module);
}
#endif

} // namespace
} // namespace bdg::nymph
