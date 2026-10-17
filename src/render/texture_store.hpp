// MIT License © 2026 Binary Dice Games
/// @file texture_store.hpp
/// @brief CPU-side copies of the textures ImGui draws with, kept up to date
///        through ImGui 1.92's texture-request protocol.
#pragma once

#include <imgui.h>
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace bdg::nymph {

/// @brief One texture's pixels: Alpha8 (1 byte per texel) or RGBA32 (4 bytes,
///        R, G, B, A), row-major, top row first.
struct texture {
  ImTextureFormat format = ImTextureFormat_RGBA32;
  int width = 0;
  int height = 0;
  std::vector<uint8_t> pixels;
};

/// @brief Textures by `ImTextureID`, the renderer side of the protocol that
///        ImGui uses when `ImGuiBackendFlags_RendererHasTextures` is set.
///
/// ImGui owns each `ImTextureData` and its pixels; this store keeps a copy,
/// so a texture stays readable while ImGui reallocates or edits its own.
class texture_store {
 public:
  /// @brief Handles one texture request: `WantCreate` copies the pixels and
  ///        assigns a new id, `WantUpdates` copies the changed rectangles,
  ///        `WantDestroy` drops the copy. Sets `TexID` and `Status` as a
  ///        backend must. Other statuses are left alone.
  /// @throws std::runtime_error on an update or destroy for an unknown id,
  ///         or an unsupported format.
  void apply(ImTextureData& tex);

  /// @brief `apply()` for every texture in `draw_data.Textures` (no-op when
  ///        the list is null, as it is for hand-built draw data).
  void apply_all(ImDrawData& draw_data);

  /// @brief Adds a texture directly (tests, or images nymph draws itself).
  /// @return Its new id, never `ImTextureID_Invalid`.
  ImTextureID add(texture tex);

  /// @brief The texture with this id, or nullptr.
  const texture* find(ImTextureID id) const;

  /// @brief Number of textures held.
  size_t size() const {
    return textures_.size();
  }

 private:
  std::unordered_map<ImTextureID, texture> textures_;
  ImTextureID next_id_ = 1;
};

} // namespace bdg::nymph
