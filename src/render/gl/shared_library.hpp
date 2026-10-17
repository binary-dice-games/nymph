// MIT License © 2026 Binary Dice Games
/// @file shared_library.hpp
/// @brief A library opened at run time (`dlopen` / `LoadLibraryW`), so
///        nymph never links a graphics library
///        (spec/02_opengl_rasterizer/DESIGN.md section 3).
#pragma once

#include <span>
#include <string>

namespace bdg::nymph {

/// @brief Move-only handle to a shared library opened at run time; closes
///        it on destruction. The only code in nymph that calls the
///        platform's dynamic loader.
class shared_library {
 public:
  shared_library() = default;
  ~shared_library();
  shared_library(shared_library&& other) noexcept;
  shared_library& operator=(shared_library&& other) noexcept;
  shared_library(const shared_library&) = delete;
  shared_library& operator=(const shared_library&) = delete;

  /// @brief Opens the first of `names` that loads.
  /// @param names File names, tried in order (searched the platform's way).
  /// @param error Set to the loader's message for each failed name when no
  ///              name loads; untouched on success.
  /// @param pin   Keep the library in the process after this handle closes
  ///              (`RTLD_NODELETE`, `GET_MODULE_HANDLE_EX_FLAG_PIN`). Graphics
  ///              drivers keep per-process state and may crash when unloaded
  ///              and loaded again, so nymph pins EGL and opengl32.
  /// @return The library; an empty one (`!loaded()`) if none loaded.
  static shared_library open(std::span<const std::string> names, std::string& error, bool pin = false);

  /// @brief Whether a library is held.
  bool loaded() const noexcept {
    return handle_ != nullptr;
  }

  /// @brief The address of exported symbol `name`, or nullptr.
  void* symbol(const char* name) const noexcept;

  /// @brief The file name the library was opened by.
  const std::string& name() const noexcept {
    return name_;
  }

 private:
  void close() noexcept;

  void* handle_ = nullptr;
  std::string name_;
};

} // namespace bdg::nymph
