// MIT License © 2026 Binary Dice Games
/// @file png_meta.cpp
/// @brief encode_png() / read_source(): PNG chunks carrying the nymph source.
///
/// Port of wish nymph's `nymph_png_meta.cpp`, with nymph's own CRC-32 and
/// stb's zlib in place of miniz.
#include "png/png_meta.hpp"
#include <array>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <stdexcept>
#include "core/limits.hpp"

// stb, compiled into this file only and with internal linkage, so it never
// clashes with another copy of stb in a program that links nymph.
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#endif
#define STB_IMAGE_WRITE_STATIC
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>
#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#include <stb_image.h>
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

namespace bdg::nymph {

namespace {

constexpr std::array<uint8_t, 8> kSignature{0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};
constexpr std::string_view kSourceKeyword{"nymph"};
constexpr std::string_view kDescriptionKeyword{"Description"};
constexpr size_t kIhdrEnd = 8 + 4 + 4 + 13 + 4; // signature + the IHDR chunk

using bytes_t = std::vector<uint8_t>;

void put_u32(bytes_t& out, uint32_t v) {
  out.push_back(static_cast<uint8_t>(v >> 24));
  out.push_back(static_cast<uint8_t>(v >> 16));
  out.push_back(static_cast<uint8_t>(v >> 8));
  out.push_back(static_cast<uint8_t>(v));
}

uint32_t get_u32(std::span<const uint8_t> bytes, size_t at) {
  return static_cast<uint32_t>(bytes[at]) << 24 | static_cast<uint32_t>(bytes[at + 1]) << 16 |
      static_cast<uint32_t>(bytes[at + 2]) << 8 | static_cast<uint32_t>(bytes[at + 3]);
}

void append(bytes_t& out, std::string_view text) {
  out.insert(out.end(), text.begin(), text.end());
}

/// A complete iTXt chunk (length, type, data, CRC) holding `text`.
bytes_t text_chunk(std::string_view keyword, std::string_view text, bool compress) {
  bytes_t body;
  append(body, "iTXt");
  append(body, keyword);
  body.push_back(0);
  body.push_back(compress ? 1 : 0); // compression flag
  body.push_back(0); // compression method: zlib
  body.push_back(0); // empty language tag
  body.push_back(0); // empty translated keyword
  if (compress) {
    int packed_size = 0;
    unsigned char* packed = stbi_zlib_compress(
        reinterpret_cast<unsigned char*>(const_cast<char*>(text.data())),
        static_cast<int>(text.size()),
        &packed_size,
        8);
    if (packed == nullptr) {
      throw std::runtime_error("could not compress the embedded source");
    }
    body.insert(body.end(), packed, packed + packed_size);
    STBIW_FREE(packed);
  } else {
    append(body, text);
  }
  bytes_t chunk;
  put_u32(chunk, static_cast<uint32_t>(body.size() - 4));
  chunk.insert(chunk.end(), body.begin(), body.end());
  put_u32(chunk, crc32(body));
  return chunk;
}

void append_bytes(void* context, void* data, int size) {
  const auto* p = static_cast<const uint8_t*>(data);
  static_cast<bytes_t*>(context)->insert(static_cast<bytes_t*>(context)->end(), p, p + size);
}

/// The text of an iTXt chunk's data if its keyword is `keyword`.
std::optional<std::string> text_of(std::span<const uint8_t> data, std::string_view keyword) {
  const std::string_view view(reinterpret_cast<const char*>(data.data()), data.size());
  if (view.size() < keyword.size() + 3 || view.substr(0, keyword.size()) != keyword || view[keyword.size()] != '\0') {
    return std::nullopt;
  }
  size_t pos = keyword.size() + 1;
  const bool compressed = view[pos] != '\0';
  pos += 2; // compression flag + method
  for (int field = 0; field < 2; ++field) { // language tag, translated keyword
    const size_t end = view.find('\0', pos);
    if (end == std::string_view::npos) {
      throw std::runtime_error("malformed text chunk in PNG");
    }
    pos = end + 1;
  }
  const std::string_view payload = view.substr(pos);
  if (!compressed) {
    if (payload.size() > kMaxSourceBytes) {
      throw std::runtime_error("the embedded source is larger than 16 MiB");
    }
    return std::string(payload);
  }
  // Decompressed into a bounded buffer, so a crafted chunk cannot make this
  // allocate without limit.
  std::string text(kMaxSourceBytes, '\0');
  const int out_size = stbi_zlib_decode_buffer(
      text.data(), static_cast<int>(text.size()), payload.data(), static_cast<int>(payload.size()));
  if (out_size < 0) {
    throw std::runtime_error("could not decompress the embedded source (corrupt, or larger than 16 MiB)");
  }
  text.resize(static_cast<size_t>(out_size));
  return text;
}

constexpr std::array<uint32_t, 256> make_crc_table() {
  std::array<uint32_t, 256> table{};
  for (uint32_t n = 0; n < 256; ++n) {
    uint32_t c = n;
    for (int k = 0; k < 8; ++k) {
      c = (c & 1u) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
    }
    table[n] = c;
  }
  return table;
}

constexpr std::array<uint32_t, 256> kCrcTable = make_crc_table();

} // namespace

uint32_t crc32(std::span<const uint8_t> bytes) {
  uint32_t c = 0xFFFFFFFFu;
  for (uint8_t b : bytes) {
    c = kCrcTable[(c ^ b) & 0xFFu] ^ (c >> 8);
  }
  return c ^ 0xFFFFFFFFu;
}

bool is_png(std::span<const uint8_t> bytes) {
  return bytes.size() >= kSignature.size() && std::memcmp(bytes.data(), kSignature.data(), kSignature.size()) == 0;
}

std::optional<image> decode_png(std::span<const uint8_t> bytes) {
  if (!is_png(bytes) || bytes.size() > static_cast<size_t>(std::numeric_limits<int>::max())) {
    return std::nullopt;
  }
  int width = 0;
  int height = 0;
  int channels = 0;
  stbi_uc* pixels = stbi_load_from_memory(bytes.data(), static_cast<int>(bytes.size()), &width, &height, &channels, 4);
  if (pixels == nullptr) {
    return std::nullopt;
  }
  image img;
  img.width = width;
  img.height = height;
  img.rgba.assign(pixels, pixels + static_cast<size_t>(width) * height * 4);
  stbi_image_free(pixels);
  return img;
}

std::vector<uint8_t> encode_png(const image& img, std::string_view source, std::string_view description) {
  if (img.width <= 0 || img.height <= 0 || img.rgba.size() != static_cast<size_t>(img.width) * img.height * 4) {
    throw std::runtime_error("cannot encode an empty image");
  }
  if (source.size() > kMaxSourceBytes) {
    throw std::runtime_error("the source is larger than 16 MiB");
  }
  bytes_t png;
  if (!stbi_write_png_to_func(append_bytes, &png, img.width, img.height, 4, img.rgba.data(), img.width * 4) ||
      png.size() < kIhdrEnd || !is_png(png)) {
    throw std::runtime_error("PNG encoding failed");
  }

  bytes_t chunks = text_chunk(kSourceKeyword, source, source.size() > kInlineSourceBytes);
  if (!description.empty()) {
    const bytes_t description_chunk = text_chunk(kDescriptionKeyword, description, false);
    chunks.insert(chunks.end(), description_chunk.begin(), description_chunk.end());
  }
  png.insert(png.begin() + kIhdrEnd, chunks.begin(), chunks.end());
  return png;
}

std::optional<std::string> read_source(std::span<const uint8_t> bytes) {
  if (!is_png(bytes)) {
    return std::nullopt;
  }
  size_t pos = kSignature.size();
  while (pos < bytes.size()) {
    if (bytes.size() - pos < 12) {
      throw std::runtime_error("truncated PNG");
    }
    const size_t length = get_u32(bytes, pos);
    if (length > bytes.size() - pos - 12) {
      throw std::runtime_error("truncated PNG");
    }
    const std::span<const uint8_t> type_and_data = bytes.subspan(pos + 4, 4 + length);
    if (crc32(type_and_data) != get_u32(bytes, pos + 8 + length)) {
      throw std::runtime_error("corrupt PNG (bad chunk checksum)");
    }
    const std::string_view type(reinterpret_cast<const char*>(type_and_data.data()), 4);
    if (type == "iTXt") {
      if (auto text = text_of(type_and_data.subspan(4), kSourceKeyword)) {
        return text;
      }
    } else if (type == "IEND") {
      break;
    }
    pos += 12 + length;
  }
  return std::nullopt;
}

} // namespace bdg::nymph
