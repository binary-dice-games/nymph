// MIT License © 2026 Binary Dice Games
/// @file texture_store.cpp
/// @brief texture_store: the texture half of nymph's ImGui renderer backend.
#include "render/texture_store.hpp"
#include <cstring>
#include <stdexcept>
#include <string>

namespace bdg::nymph {

namespace {

int bytes_per_texel(ImTextureFormat format) {
  switch (format) {
    case ImTextureFormat_Alpha8:
      return 1;
    case ImTextureFormat_RGBA32:
      return 4;
  }
  throw std::runtime_error("unsupported ImGui texture format " + std::to_string(static_cast<int>(format)));
}

/// Copies one rectangle of `tex`'s pixels into `dst` (same size and format).
void copy_rect(const ImTextureData& tex, texture& dst, int x, int y, int w, int h) {
  const int bpp = bytes_per_texel(dst.format);
  for (int row = 0; row < h; ++row) {
    const size_t offset = (static_cast<size_t>(y + row) * dst.width + x) * bpp;
    std::memcpy(dst.pixels.data() + offset, tex.Pixels + offset, static_cast<size_t>(w) * bpp);
  }
}

} // namespace

void texture_store::apply(ImTextureData& tex) {
  switch (tex.Status) {
    case ImTextureStatus_WantCreate: {
      texture copy;
      copy.format = tex.Format;
      copy.width = tex.Width;
      copy.height = tex.Height;
      copy.pixels.resize(static_cast<size_t>(tex.Width) * tex.Height * bytes_per_texel(tex.Format));
      copy_rect(tex, copy, 0, 0, tex.Width, tex.Height);
      tex.SetTexID(add(std::move(copy)));
      tex.SetStatus(ImTextureStatus_OK);
      break;
    }
    case ImTextureStatus_WantUpdates: {
      auto it = textures_.find(tex.TexID);
      if (it == textures_.end()) {
        throw std::runtime_error("texture update for an unknown texture id");
      }
      if (tex.Updates.Size > 0) {
        for (const ImTextureRect& r : tex.Updates) {
          copy_rect(tex, it->second, r.x, r.y, r.w, r.h);
        }
      } else if (tex.UpdateRect.w > 0 && tex.UpdateRect.h > 0) {
        const ImTextureRect& r = tex.UpdateRect;
        copy_rect(tex, it->second, r.x, r.y, r.w, r.h);
      }
      tex.SetStatus(ImTextureStatus_OK);
      break;
    }
    case ImTextureStatus_WantDestroy:
      textures_.erase(tex.TexID);
      tex.SetTexID(ImTextureID_Invalid);
      tex.SetStatus(ImTextureStatus_Destroyed);
      break;
    case ImTextureStatus_OK:
    case ImTextureStatus_Destroyed:
      break;
  }
}

void texture_store::apply_all(ImDrawData& draw_data) {
  if (draw_data.Textures == nullptr) {
    return;
  }
  for (ImTextureData* tex : *draw_data.Textures) {
    apply(*tex);
  }
}

ImTextureID texture_store::add(texture tex) {
  bytes_per_texel(tex.format); // validates the format
  const ImTextureID id = next_id_++;
  textures_.emplace(id, std::move(tex));
  return id;
}

const texture* texture_store::find(ImTextureID id) const {
  auto it = textures_.find(id);
  return it == textures_.end() ? nullptr : &it->second;
}

} // namespace bdg::nymph
