// MIT License © 2026 Binary Dice Games
/// @file gl_rasterizer.cpp
/// @brief gl_backend::rasterize(), structured after ImGui's
///        imgui_impl_opengl3.cpp but drawing into a framebuffer object and
///        reading the pixels back.
#include "render/gl_rasterizer.hpp"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace bdg::nymph {

namespace {

constexpr const char* kVertexShader =
    "#version 330 core\n"
    "layout (location = 0) in vec2 Position;\n"
    "layout (location = 1) in vec2 UV;\n"
    "layout (location = 2) in vec4 Color;\n"
    "uniform mat4 ProjMtx;\n"
    "out vec2 Frag_UV;\n"
    "out vec4 Frag_Color;\n"
    "void main() {\n"
    "  Frag_UV = UV;\n"
    "  Frag_Color = Color;\n"
    "  gl_Position = ProjMtx * vec4(Position.xy, 0.0, 1.0);\n"
    "}\n";

constexpr const char* kFragmentShader =
    "#version 330 core\n"
    "in vec2 Frag_UV;\n"
    "in vec4 Frag_Color;\n"
    "uniform sampler2D Texture;\n"
    "layout (location = 0) out vec4 Out_Color;\n"
    "void main() {\n"
    "  Out_Color = Frag_Color * texture(Texture, Frag_UV.st);\n"
    "}\n";

std::string hex(GLenum code) {
  char buf[16];
  std::snprintf(buf, sizeof(buf), "0x%04X", code);
  return buf;
}

bool is_reset_render_state(ImDrawCallback cb) {
  if (cb == &reset_render_state_callback) {
    return true;
  }
#ifdef ImDrawCallback_ResetRenderState
  if (cb == ImDrawCallback_ResetRenderState) {
    return true;
  }
#endif
  return false;
}

/// Index of the first pixel whose centre is at or after coordinate `v`, as
/// the software rasterizer maps clip rectangles.
int first_center_at_or_after(double v) {
  return static_cast<int>(std::ceil(v - 0.5));
}

/// Every GL object one rasterize() call creates, deleted on scope exit.
class call_objects {
 public:
  explicit call_objects(const gl_api& gl) : gl_(gl) {}
  ~call_objects() {
    if (!textures.empty()) {
      gl_.DeleteTextures(static_cast<GLsizei>(textures.size()), textures.data());
    }
    if (framebuffer != 0) {
      gl_.BindFramebuffer(GL_FRAMEBUFFER, 0);
      gl_.DeleteFramebuffers(1, &framebuffer);
    }
    if (target != 0) {
      gl_.DeleteTextures(1, &target);
    }
    if (vao != 0) {
      gl_.BindVertexArray(0);
      gl_.DeleteVertexArrays(1, &vao);
    }
    if (buffers[0] != 0) {
      gl_.DeleteBuffers(2, buffers);
    }
    gl_.UseProgram(0);
    // Leave no error behind for the next call.
    for (int i = 0; i < 16 && gl_.GetError() != GL_NO_ERROR; ++i) {
    }
  }
  call_objects(const call_objects&) = delete;
  call_objects& operator=(const call_objects&) = delete;

  std::vector<GLuint> textures;
  GLuint framebuffer = 0;
  GLuint target = 0;
  GLuint vao = 0;
  GLuint buffers[2] = {0, 0}; ///< vertices, indices

 private:
  const gl_api& gl_;
};

/// Uploads `tex` to a new GL texture: RGBA32 as RGBA8, Alpha8 as R8 read
/// as white with that alpha (how ImGui's GPU backends and the software
/// rasterizer treat Alpha8).
GLuint upload(const gl_api& gl, const texture& tex) {
  GLuint name = 0;
  gl.GenTextures(1, &name);
  gl.BindTexture(GL_TEXTURE_2D, name);
  gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, static_cast<GLint>(GL_LINEAR));
  gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, static_cast<GLint>(GL_LINEAR));
  gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, static_cast<GLint>(GL_CLAMP_TO_EDGE));
  gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, static_cast<GLint>(GL_CLAMP_TO_EDGE));
  gl.PixelStorei(GL_UNPACK_ALIGNMENT, 1);
  gl.PixelStorei(GL_UNPACK_ROW_LENGTH, 0);
  if (tex.format == ImTextureFormat_Alpha8) {
    gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, static_cast<GLint>(GL_ONE));
    gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_G, static_cast<GLint>(GL_ONE));
    gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, static_cast<GLint>(GL_ONE));
    gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_A, static_cast<GLint>(GL_RED));
    gl.TexImage2D(
        GL_TEXTURE_2D,
        0,
        static_cast<GLint>(GL_R8),
        tex.width,
        tex.height,
        0,
        GL_RED,
        GL_UNSIGNED_BYTE,
        tex.pixels.data());
  } else {
    gl.TexImage2D(
        GL_TEXTURE_2D,
        0,
        static_cast<GLint>(GL_RGBA8),
        tex.width,
        tex.height,
        0,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        tex.pixels.data());
  }
  return name;
}

/// Compiles one shader stage; throws raster_unavailable with its log.
GLuint compile(const gl_api& gl, GLenum type, const char* source) {
  const GLuint shader = gl.CreateShader(type);
  gl.ShaderSource(shader, 1, &source, nullptr);
  gl.CompileShader(shader);
  GLint ok = 0;
  gl.GetShaderiv(shader, GL_COMPILE_STATUS, &ok);
  if (!ok) {
    GLint length = 0;
    gl.GetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
    std::string log(static_cast<size_t>(length > 0 ? length : 1), '\0');
    gl.GetShaderInfoLog(shader, static_cast<GLsizei>(log.size()), nullptr, log.data());
    gl.DeleteShader(shader);
    throw raster_unavailable("OpenGL shader does not compile: " + std::string(log.c_str()));
  }
  return shader;
}

} // namespace

gl_backend::~gl_backend() {
  if (program_ == 0) {
    return;
  }
  try {
    const gl_device::current_scope scope(device_);
    device_.gl().DeleteProgram(program_);
  } catch (const std::exception&) {
    // The context is gone with the device; nothing left to free.
  }
}

void gl_backend::build_program() {
  const gl_api& gl = device_.gl();
  const GLuint vs = compile(gl, GL_VERTEX_SHADER, kVertexShader);
  GLuint fs = 0;
  try {
    fs = compile(gl, GL_FRAGMENT_SHADER, kFragmentShader);
  } catch (...) {
    gl.DeleteShader(vs);
    throw;
  }
  const GLuint program = gl.CreateProgram();
  gl.AttachShader(program, vs);
  gl.AttachShader(program, fs);
  gl.LinkProgram(program);
  gl.DeleteShader(vs);
  gl.DeleteShader(fs);
  GLint ok = 0;
  gl.GetProgramiv(program, GL_LINK_STATUS, &ok);
  if (!ok) {
    GLint length = 0;
    gl.GetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
    std::string log(static_cast<size_t>(length > 0 ? length : 1), '\0');
    gl.GetProgramInfoLog(program, static_cast<GLsizei>(log.size()), nullptr, log.data());
    gl.DeleteProgram(program);
    throw raster_unavailable("OpenGL program does not link: " + std::string(log.c_str()));
  }
  program_ = program;
  projection_location_ = gl.GetUniformLocation(program_, "ProjMtx");
  texture_location_ = gl.GetUniformLocation(program_, "Texture");
}

image gl_backend::rasterize(const ImDrawData& draw_data, const texture_store& textures, ImU32 clear_color) {
  if (fail_next_) {
    fail_next_ = false;
    throw raster_unavailable("injected OpenGL failure (test hook)");
  }
  image img;
  img.width = std::max(0, static_cast<int>(std::lround(draw_data.DisplaySize.x * draw_data.FramebufferScale.x)));
  img.height = std::max(0, static_cast<int>(std::lround(draw_data.DisplaySize.y * draw_data.FramebufferScale.y)));
  const int limit = size_limit_ > 0 ? std::min(size_limit_, device_.max_size()) : device_.max_size();
  if (img.width > limit || img.height > limit) {
    throw raster_too_large(
        "image of " + std::to_string(img.width) + "x" + std::to_string(img.height) +
        " pixels is larger than the OpenGL device allows (" + std::to_string(limit) + ")");
  }
  img.rgba.resize(static_cast<size_t>(img.width) * img.height * 4);
  if (img.width == 0 || img.height == 0) {
    return img;
  }

  const gl_device::current_scope scope(device_);
  const gl_api& gl = device_.gl();
  if (program_ == 0) {
    build_program();
  }
  call_objects objects(gl);

  // ── Target ─────────────────────────────────────────────────────────────────
  gl.GenTextures(1, &objects.target);
  gl.BindTexture(GL_TEXTURE_2D, objects.target);
  gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, static_cast<GLint>(GL_LINEAR));
  gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, static_cast<GLint>(GL_LINEAR));
  gl.TexImage2D(
      GL_TEXTURE_2D, 0, static_cast<GLint>(GL_RGBA8), img.width, img.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
  gl.GenFramebuffers(1, &objects.framebuffer);
  gl.BindFramebuffer(GL_FRAMEBUFFER, objects.framebuffer);
  gl.FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, objects.target, 0);
  if (const GLenum status = gl.CheckFramebufferStatus(GL_FRAMEBUFFER); status != GL_FRAMEBUFFER_COMPLETE) {
    throw raster_unavailable("OpenGL framebuffer is incomplete (" + hex(status) + ")");
  }

  // ── State: ImGui's blend, nothing else ─────────────────────────────────────
  gl.Viewport(0, 0, img.width, img.height);
  gl.Disable(GL_DITHER);
  gl.Disable(GL_FRAMEBUFFER_SRGB);
  gl.Disable(GL_MULTISAMPLE);
  gl.Disable(GL_CULL_FACE);
  gl.Disable(GL_DEPTH_TEST);
  gl.Disable(GL_STENCIL_TEST);
  gl.Disable(GL_SCISSOR_TEST);
  gl.ClearColor(
      static_cast<GLfloat>((clear_color >> IM_COL32_R_SHIFT) & 0xFF) / 255.0f,
      static_cast<GLfloat>((clear_color >> IM_COL32_G_SHIFT) & 0xFF) / 255.0f,
      static_cast<GLfloat>((clear_color >> IM_COL32_B_SHIFT) & 0xFF) / 255.0f,
      static_cast<GLfloat>((clear_color >> IM_COL32_A_SHIFT) & 0xFF) / 255.0f);
  gl.Clear(GL_COLOR_BUFFER_BIT);
  gl.Enable(GL_BLEND);
  gl.BlendEquation(GL_FUNC_ADD);
  gl.BlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
  gl.Enable(GL_SCISSOR_TEST);

  // The projection puts the display's top edge on framebuffer row 0, so
  // glReadPixels returns the image top row first, as `image` stores it.
  const float l = draw_data.DisplayPos.x;
  const float r = draw_data.DisplayPos.x + draw_data.DisplaySize.x;
  const float t = draw_data.DisplayPos.y;
  const float b = draw_data.DisplayPos.y + draw_data.DisplaySize.y;
  const float projection[4][4] = {
      {2.0f / (r - l), 0.0f, 0.0f, 0.0f},
      {0.0f, 2.0f / (b - t), 0.0f, 0.0f},
      {0.0f, 0.0f, -1.0f, 0.0f},
      {(r + l) / (l - r), (t + b) / (t - b), 0.0f, 1.0f},
  };
  gl.UseProgram(program_);
  gl.Uniform1i(texture_location_, 0);
  gl.UniformMatrix4fv(projection_location_, 1, GL_FALSE, &projection[0][0]);
  gl.ActiveTexture(GL_TEXTURE0);

  gl.GenVertexArrays(1, &objects.vao);
  gl.BindVertexArray(objects.vao);
  gl.GenBuffers(2, objects.buffers);
  gl.BindBuffer(GL_ARRAY_BUFFER, objects.buffers[0]);
  gl.BindBuffer(GL_ELEMENT_ARRAY_BUFFER, objects.buffers[1]);
  gl.EnableVertexAttribArray(0);
  gl.EnableVertexAttribArray(1);
  gl.EnableVertexAttribArray(2);
  constexpr GLsizei kStride = sizeof(ImDrawVert);
  gl.VertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, kStride, reinterpret_cast<const void*>(offsetof(ImDrawVert, pos)));
  gl.VertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, kStride, reinterpret_cast<const void*>(offsetof(ImDrawVert, uv)));
  gl.VertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, 1, kStride, reinterpret_cast<const void*>(offsetof(ImDrawVert, col)));

  // ── Commands ───────────────────────────────────────────────────────────────
  constexpr GLenum kIndexType = sizeof(ImDrawIdx) == 2 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT;
  std::unordered_map<ImTextureID, GLuint> uploaded;
  const ImVec2 origin = draw_data.DisplayPos;
  const ImVec2 scale = draw_data.FramebufferScale;
  for (const ImDrawList* list : draw_data.CmdLists) {
    gl.BufferData(
        GL_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(list->VtxBuffer.Size) * sizeof(ImDrawVert),
        list->VtxBuffer.Data,
        GL_STREAM_DRAW);
    gl.BufferData(
        GL_ELEMENT_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(list->IdxBuffer.Size) * sizeof(ImDrawIdx),
        list->IdxBuffer.Data,
        GL_STREAM_DRAW);
    for (const ImDrawCmd& cmd : list->CmdBuffer) {
      if (cmd.UserCallback != nullptr) {
        if (is_reset_render_state(cmd.UserCallback)) {
          continue;
        }
        throw std::runtime_error("unsupported ImGui draw callback");
      }
      if (cmd.ElemCount == 0) {
        continue;
      }
      const ImTextureID id = cmd.GetTexID();
      auto it = uploaded.find(id);
      if (it == uploaded.end()) {
        const texture* tex = textures.find(id);
        if (tex == nullptr) {
          throw std::runtime_error("draw command refers to an unknown texture");
        }
        objects.textures.push_back(upload(gl, *tex));
        it = uploaded.emplace(id, objects.textures.back()).first;
      }
      // Pixels whose centre lies inside the clip rectangle.
      const int x0 = std::max(0, first_center_at_or_after((cmd.ClipRect.x - origin.x) * scale.x));
      const int y0 = std::max(0, first_center_at_or_after((cmd.ClipRect.y - origin.y) * scale.y));
      const int x1 = std::min(img.width, first_center_at_or_after((cmd.ClipRect.z - origin.x) * scale.x));
      const int y1 = std::min(img.height, first_center_at_or_after((cmd.ClipRect.w - origin.y) * scale.y));
      if (x0 >= x1 || y0 >= y1) {
        continue;
      }
      gl.Scissor(x0, y0, x1 - x0, y1 - y0);
      gl.BindTexture(GL_TEXTURE_2D, it->second);
      const GLsizei count = static_cast<GLsizei>(cmd.ElemCount - cmd.ElemCount % 3);
      gl.DrawElementsBaseVertex(
          GL_TRIANGLES,
          count,
          kIndexType,
          reinterpret_cast<const void*>(cmd.IdxOffset * sizeof(ImDrawIdx)),
          static_cast<GLint>(cmd.VtxOffset));
    }
  }

  // ── Read back ──────────────────────────────────────────────────────────────
  gl.PixelStorei(GL_PACK_ALIGNMENT, 1);
  gl.PixelStorei(GL_PACK_ROW_LENGTH, 0);
  gl.ReadPixels(0, 0, img.width, img.height, GL_RGBA, GL_UNSIGNED_BYTE, img.rgba.data());
  gl.Finish();
  if (const GLenum error = gl.GetError(); error != GL_NO_ERROR) {
    throw raster_unavailable("OpenGL error " + hex(error) + " while rasterizing");
  }
  return img;
}

} // namespace bdg::nymph
