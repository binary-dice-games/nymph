// MIT License © 2026 Binary Dice Games
/// @file test_png_meta.cpp
/// @brief Plan step 4: the source text embedded in, and read back from, the
///        PNG file. Ported from wish's test_nymph_png_meta.cpp.
#include <gtest/gtest.h>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>
#include "core/limits.hpp"
#include "png/png_meta.hpp"

// A private stb_image to decode pixels, test-only and with internal linkage.
#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#include <stb_image.h>

namespace bdg::nymph {
namespace {

using bytes_t = std::vector<uint8_t>;

image gradient(int w, int h) {
  image img;
  img.width = w;
  img.height = h;
  img.rgba.resize(static_cast<size_t>(w) * h * 4);
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      uint8_t* p = img.at(x, y);
      p[0] = static_cast<uint8_t>(x * 7);
      p[1] = static_cast<uint8_t>(y * 11);
      p[2] = static_cast<uint8_t>(x + y);
      p[3] = 255;
    }
  }
  return img;
}

std::string repeated(const std::string& unit, size_t bytes) {
  std::string out;
  while (out.size() < bytes) {
    out += unit;
  }
  return out;
}

std::string as_string(const bytes_t& b) {
  return std::string(reinterpret_cast<const char*>(b.data()), b.size());
}

bytes_t as_bytes(std::string_view s) {
  return bytes_t(s.begin(), s.end());
}

uint32_t get_u32(const bytes_t& b, size_t at) {
  return static_cast<uint32_t>(b[at]) << 24 | static_cast<uint32_t>(b[at + 1]) << 16 |
      static_cast<uint32_t>(b[at + 2]) << 8 | b[at + 3];
}

/// Type of the chunk starting at `pos`, and the offset of the next one.
std::string chunk_type(const bytes_t& png, size_t pos, size_t* next) {
  *next = pos + 12 + get_u32(png, pos);
  return std::string(reinterpret_cast<const char*>(png.data() + pos + 4), 4);
}

std::string round_trip(const std::string& source) {
  const auto back = read_source(encode_png(gradient(8, 8), source, ""));
  EXPECT_TRUE(back.has_value());
  return back.value_or("<none>");
}

// ── Round trip ───────────────────────────────────────────────────────────────

TEST(png_meta, SmallSourceRoundTripsAndStaysReadable) {
  const std::string source =
      "description: \xc3\x9c"
      "n\xc3\xaf"
      "code \xe2\x9c\x93" // "Ünïcode ✓"
      "\nroot: {type: Plot}\ndata: |\n  a,b\n  1,2\n";
  const bytes_t png = encode_png(gradient(20, 10), source, "alt text");
  ASSERT_TRUE(is_png(png));
  const auto back = read_source(png);
  ASSERT_TRUE(back.has_value());
  EXPECT_EQ(*back, source);
  // Stored as is, so plain text tools can find it in the file.
  EXPECT_NE(as_string(png).find(source), std::string::npos);
}

TEST(png_meta, LineEndingsAndMissingFinalNewlineArePreserved) {
  EXPECT_EQ(round_trip("a: 1\r\nb: 2\r\n"), "a: 1\r\nb: 2\r\n");
  EXPECT_EQ(round_trip("a: 1\nb: 2"), "a: 1\nb: 2");
  EXPECT_EQ(round_trip("  # comment only\n\n\n"), "  # comment only\n\n\n");
}

TEST(png_meta, EmptySourceRoundTrips) {
  EXPECT_EQ(round_trip(""), "");
}

TEST(png_meta, InlineLimitIsExactly64KiB) {
  const std::string at_limit = repeated("x", kInlineSourceBytes);
  ASSERT_EQ(at_limit.size(), kInlineSourceBytes);
  const bytes_t inline_png = encode_png(gradient(4, 4), at_limit, "");
  EXPECT_NE(as_string(inline_png).find(at_limit), std::string::npos);
  EXPECT_EQ(read_source(inline_png).value_or(""), at_limit);

  const std::string over = at_limit + "y";
  const bytes_t packed_png = encode_png(gradient(4, 4), over, "");
  EXPECT_EQ(as_string(packed_png).find(at_limit), std::string::npos); // compressed
  EXPECT_EQ(read_source(packed_png).value_or(""), over);
}

TEST(png_meta, LargeSourceRoundTripsCompressed) {
  const std::string source = "root: {}\ndata: |\n" + repeated("  12345,67890\n", 1024 * 1024);
  ASSERT_GT(source.size(), kInlineSourceBytes);
  const bytes_t png = encode_png(gradient(8, 8), source, "");
  EXPECT_LT(png.size(), source.size() / 4); // repetitive text compresses well
  EXPECT_EQ(read_source(png).value_or(""), source);
}

TEST(png_meta, EncodingIsDeterministic) {
  EXPECT_EQ(encode_png(gradient(16, 16), "s: 1\n", "d"), encode_png(gradient(16, 16), "s: 1\n", "d"));
}

TEST(png_meta, RejectsEmptyImageAndOversizedSource) {
  EXPECT_THROW(encode_png(image{}, "s", ""), std::runtime_error);
  image short_buffer = gradient(4, 4);
  short_buffer.rgba.pop_back();
  EXPECT_THROW(encode_png(short_buffer, "s", ""), std::runtime_error);
  EXPECT_THROW(encode_png(gradient(4, 4), std::string(kMaxSourceBytes + 1, 'x'), ""), std::runtime_error);
}

// ── File layout ──────────────────────────────────────────────────────────────

TEST(png_meta, PixelsSurviveForOrdinaryPngReaders) {
  const image img = gradient(33, 17);
  const bytes_t png = encode_png(img, "source", "description");
  int w = 0;
  int h = 0;
  int channels = 0;
  stbi_uc* pixels = stbi_load_from_memory(png.data(), static_cast<int>(png.size()), &w, &h, &channels, 4);
  ASSERT_NE(pixels, nullptr) << stbi_failure_reason();
  EXPECT_EQ(w, 33);
  EXPECT_EQ(h, 17);
  EXPECT_EQ(std::memcmp(pixels, img.rgba.data(), img.rgba.size()), 0);
  stbi_image_free(pixels);
}

TEST(png_meta, SourceChunkComesRightAfterIhdrThenDescription) {
  const bytes_t png = encode_png(gradient(4, 4), "the source", "the description");
  const std::string text = as_string(png);
  size_t pos = 8;
  size_t next = 0;
  EXPECT_EQ(chunk_type(png, pos, &next), "IHDR");
  pos = next;
  EXPECT_EQ(chunk_type(png, pos, &next), "iTXt");
  EXPECT_EQ(text.compare(pos + 8, 6, std::string("nymph\0", 6)), 0);
  pos = next;
  EXPECT_EQ(chunk_type(png, pos, &next), "iTXt");
  EXPECT_EQ(text.compare(pos + 8, 12, std::string("Description\0", 12)), 0);
  EXPECT_NE(text.find("the description"), std::string::npos);
}

TEST(png_meta, NoDescriptionChunkWhenDescriptionIsEmpty) {
  EXPECT_EQ(as_string(encode_png(gradient(4, 4), "s", "")).find("Description"), std::string::npos);
}

TEST(png_meta, Crc32MatchesTheStandardAndStbsChunks) {
  EXPECT_EQ(crc32(as_bytes("123456789")), 0xCBF43926u); // the CRC-32 check value
  EXPECT_EQ(crc32(as_bytes("")), 0u);
  // stb wrote IHDR with its own CRC; ours must agree on it.
  const bytes_t png = encode_png(gradient(5, 3), "s", "");
  const std::span<const uint8_t> ihdr(png.data() + 12, 4 + 13);
  EXPECT_EQ(crc32(ihdr), get_u32(png, 8 + 4 + 4 + 13));
}

// ── Not nymph, or broken ─────────────────────────────────────────────────────

TEST(png_meta, NotANymphImageIsNullopt) {
  EXPECT_FALSE(read_source(as_bytes("plain text, not a PNG")).has_value());
  EXPECT_FALSE(read_source(bytes_t{}).has_value());
  // A real PNG that simply has no nymph chunk: cut ours back out.
  bytes_t png = encode_png(gradient(4, 4), "s", "");
  size_t next = 0;
  chunk_type(png, 33, &next);
  png.erase(png.begin() + 33, png.begin() + static_cast<std::ptrdiff_t>(next));
  EXPECT_TRUE(is_png(png));
  EXPECT_FALSE(read_source(png).has_value());
}

TEST(png_meta, CorruptOrTruncatedPngThrows) {
  const bytes_t png = encode_png(gradient(4, 4), "the source", "");
  bytes_t flipped = png;
  flipped[as_string(png).find("the source") + 2] ^= 0x20; // payload no longer matches its CRC
  EXPECT_THROW(read_source(flipped), std::runtime_error);
  EXPECT_THROW(read_source(bytes_t(png.begin(), png.begin() + 40)), std::runtime_error);
}

TEST(png_meta, CorruptCompressedPayloadWithValidCrcThrows) {
  // Compressed chunk, then damage its zlib stream and recompute the CRC so
  // only decompression can notice.
  bytes_t png = encode_png(gradient(4, 4), repeated("abc\n", kInlineSourceBytes + 10), "");
  size_t next = 0;
  ASSERT_EQ(chunk_type(png, 33, &next), "iTXt");
  const size_t length = get_u32(png, 33);
  const size_t data = 33 + 8;
  const size_t zlib_start = data + 6 + 1 + 1 + 1 + 1 + 1; // "nymph\0", flag, method, lang\0, key\0
  for (size_t i = zlib_start + 2; i < zlib_start + 12; ++i) {
    png[i] ^= 0xFF;
  }
  const uint32_t crc = crc32(std::span<const uint8_t>(png.data() + 33 + 4, 4 + length));
  png[data + length] = static_cast<uint8_t>(crc >> 24);
  png[data + length + 1] = static_cast<uint8_t>(crc >> 16);
  png[data + length + 2] = static_cast<uint8_t>(crc >> 8);
  png[data + length + 3] = static_cast<uint8_t>(crc);
  EXPECT_THROW(read_source(png), std::runtime_error);
}

} // namespace
} // namespace bdg::nymph
