// MIT License © 2026 Binary Dice Games
/// @file gl_device.cpp
/// @brief The platform-independent part of gl_device; creation and context
///        switching are in gl_device_posix.cpp (EGL) and gl_device_win.cpp
///        (WGL).
#include "render/gl/gl_device.hpp"
#include <algorithm>
#include <cctype>
#include "render/raster_backend.hpp"

namespace bdg::nymph {

namespace {

std::string gl_string(const gl_api& gl, GLenum name) {
  const GLubyte* s = gl.GetString(name);
  return s != nullptr ? std::string(reinterpret_cast<const char*>(s)) : std::string();
}

} // namespace

bool is_software_renderer(const std::string& renderer) {
  std::string lower = renderer;
  std::transform(
      lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  for (const char* pattern :
       {"llvmpipe", "softpipe", "swiftshader", "gdi generic", "software rasterizer", "basic render driver"}) {
    if (lower.find(pattern) != std::string::npos) {
      return true;
    }
  }
  return false;
}

std::string gl_device::finish(gl_lookup_fn lookup, void* user, const gl_device_options& options) {
  if (const std::string missing = gl_.load(lookup, user); !missing.empty()) {
    return "OpenGL function " + missing + " not found";
  }
  GLint major = 0;
  GLint minor = 0;
  gl_.GetIntegerv(GL_MAJOR_VERSION, &major);
  gl_.GetIntegerv(GL_MINOR_VERSION, &minor);
  const std::string vendor = gl_string(gl_, GL_VENDOR);
  const std::string renderer = gl_string(gl_, GL_RENDERER);
  const std::string version = gl_string(gl_, GL_VERSION);
  if (major < 3 || (major == 3 && minor < 3)) {
    return "OpenGL 3.3 is needed, the context is " + (version.empty() ? std::string("older") : version);
  }
  if (!options.allow_software_gl && is_software_renderer(renderer)) {
    return "software OpenGL (" + renderer + ") refused: no GPU";
  }
  name_ = vendor + " " + renderer + ", OpenGL " + version;

  GLint texture_size = 0;
  GLint renderbuffer_size = 0;
  GLint viewport[2] = {0, 0};
  gl_.GetIntegerv(GL_MAX_TEXTURE_SIZE, &texture_size);
  gl_.GetIntegerv(GL_MAX_RENDERBUFFER_SIZE, &renderbuffer_size);
  gl_.GetIntegerv(GL_MAX_VIEWPORT_DIMS, viewport);
  max_size_ = std::min({texture_size, renderbuffer_size, viewport[0], viewport[1]});
  while (gl_.GetError() != GL_NO_ERROR) {
  }
  if (max_size_ < 64) {
    return "OpenGL reports a maximum framebuffer size of " + std::to_string(max_size_);
  }
  return {};
}

gl_device::current_scope::current_scope(const gl_device& device) : device_(device) {
  if (!device_.make_current(saved_, saved_api_)) {
    throw raster_unavailable("the OpenGL context cannot be made current");
  }
}

gl_device::current_scope::~current_scope() {
  device_.restore(saved_, saved_api_);
}

} // namespace bdg::nymph
