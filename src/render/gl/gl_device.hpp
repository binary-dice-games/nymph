// MIT License © 2026 Binary Dice Games
/// @file gl_device.hpp
/// @brief An OpenGL 3.3 core context with no window, opened at run time:
///        EGL on Linux, WGL on Windows and MSYS2
///        (spec/02_opengl_rasterizer/DESIGN.md section 3, `gl_device`).
#pragma once

#include <array>
#include <memory>
#include <string>
#include <vector>
#include "render/gl/gl_api.hpp"

namespace bdg::nymph {

/// @brief How gl_device::create() looks for OpenGL.
struct gl_device_options {
  /// Accept a software OpenGL (Mesa llvmpipe / softpipe, SwiftShader,
  /// Microsoft's GDI Generic). Off for users: a CPU OpenGL is slower than
  /// nymph's own rasterizer. Tests turn it on to run without a GPU.
  bool allow_software_gl = false;
  /// Library names to try instead of the platform's (`libEGL.so.1`,
  /// `opengl32.dll`); tests use it to simulate a machine without OpenGL.
  std::vector<std::string> library_names;
};

struct gl_platform;

/// @brief Owns the loaded libraries, a headless OpenGL 3.3 core context and
///        its function table. Used by one thread at a time.
class gl_device {
 public:
  /// @brief A device, or why there is none.
  struct result {
    std::unique_ptr<gl_device> device; ///< null on failure
    std::string reason; ///< why, when `device` is null
  };

  /// @brief Opens OpenGL and creates the context. Leaves the thread's
  ///        current context as it found it.
  /// @return Never throws for an OpenGL problem; reports it in `reason`.
  static result create(const gl_device_options& options);

  ~gl_device();
  gl_device(const gl_device&) = delete;
  gl_device& operator=(const gl_device&) = delete;

  /// @brief The function table; call only inside a current_scope.
  const gl_api& gl() const noexcept {
    return gl_;
  }

  /// @brief "<vendor> <renderer>, OpenGL <version>".
  const std::string& renderer_name() const noexcept {
    return name_;
  }

  /// @brief The largest framebuffer side the device can render: the
  ///        smallest of its texture, renderbuffer and viewport limits.
  int max_size() const noexcept {
    return max_size_;
  }

  /// @brief Makes the device's context current for its lifetime, then
  ///        makes current again whatever context (of the same platform API)
  ///        was current on the thread before, also when unwinding.
  class current_scope {
   public:
    /// @throws raster_unavailable if the context cannot be made current.
    explicit current_scope(const gl_device& device);
    ~current_scope();
    current_scope(const current_scope&) = delete;
    current_scope& operator=(const current_scope&) = delete;

   private:
    const gl_device& device_;
    std::array<void*, 4> saved_{};
    unsigned saved_api_ = 0;
  };

 private:
  explicit gl_device(std::unique_ptr<gl_platform> platform);

  /// Platform: make this context current, saving the previous one.
  bool make_current(std::array<void*, 4>& saved, unsigned& saved_api) const;
  /// Platform: make the saved context current again.
  void restore(const std::array<void*, 4>& saved, unsigned saved_api) const;

  /// Common: with the context current, loads the function table, checks
  /// the version and the software-GL rule, reads the name and limits.
  /// Returns an empty string, or why the device cannot be used.
  std::string finish(gl_lookup_fn lookup, void* user, const gl_device_options& options);

  std::unique_ptr<gl_platform> platform_;
  gl_api gl_;
  std::string name_;
  int max_size_ = 0;
};

/// @brief Whether a GL_RENDERER string names a CPU implementation of
///        OpenGL (llvmpipe, softpipe, SwiftShader, GDI Generic, ...).
bool is_software_renderer(const std::string& renderer);

} // namespace bdg::nymph
