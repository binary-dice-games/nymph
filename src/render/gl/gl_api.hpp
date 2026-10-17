// MIT License © 2026 Binary Dice Games
/// @file gl_api.hpp
/// @brief nymph's own OpenGL 3.3 declarations: the types, constants and
///        function pointers the OpenGL rasterizer uses, filled at run time.
///
/// No system GL header is included and no GL function is called by its
/// linker name anywhere in nymph, so building needs no OpenGL SDK and the
/// executable links no OpenGL library
/// (spec/02_opengl_rasterizer/DESIGN.md section 3, `gl_api`).
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#if defined(_WIN32)
#define NYMPH_GLAPI __stdcall
#else
#define NYMPH_GLAPI
#endif

namespace bdg::nymph {

// ── Types ────────────────────────────────────────────────────────────────────

using GLenum = unsigned int;
using GLboolean = unsigned char;
using GLbitfield = unsigned int;
using GLint = int;
using GLuint = unsigned int;
using GLsizei = int;
using GLfloat = float;
using GLchar = char;
using GLubyte = unsigned char;
using GLsizeiptr = std::ptrdiff_t;

// ── Constants ────────────────────────────────────────────────────────────────

inline constexpr GLenum GL_NO_ERROR = 0;
inline constexpr GLenum GL_ZERO = 0;
inline constexpr GLenum GL_ONE = 1;
inline constexpr GLboolean GL_FALSE = 0;
inline constexpr GLenum GL_TRIANGLES = 0x0004;
inline constexpr GLenum GL_SRC_ALPHA = 0x0302;
inline constexpr GLenum GL_ONE_MINUS_SRC_ALPHA = 0x0303;
inline constexpr GLenum GL_CULL_FACE = 0x0B44;
inline constexpr GLenum GL_DEPTH_TEST = 0x0B71;
inline constexpr GLenum GL_STENCIL_TEST = 0x0B90;
inline constexpr GLenum GL_DITHER = 0x0BD0;
inline constexpr GLenum GL_BLEND = 0x0BE2;
inline constexpr GLenum GL_SCISSOR_TEST = 0x0C11;
inline constexpr GLenum GL_UNPACK_ROW_LENGTH = 0x0CF2;
inline constexpr GLenum GL_UNPACK_ALIGNMENT = 0x0CF5;
inline constexpr GLenum GL_PACK_ROW_LENGTH = 0x0D02;
inline constexpr GLenum GL_PACK_ALIGNMENT = 0x0D05;
inline constexpr GLenum GL_MAX_TEXTURE_SIZE = 0x0D33;
inline constexpr GLenum GL_MAX_VIEWPORT_DIMS = 0x0D3A;
inline constexpr GLenum GL_TEXTURE_2D = 0x0DE1;
inline constexpr GLenum GL_UNSIGNED_BYTE = 0x1401;
inline constexpr GLenum GL_UNSIGNED_SHORT = 0x1403;
inline constexpr GLenum GL_UNSIGNED_INT = 0x1405;
inline constexpr GLenum GL_FLOAT = 0x1406;
inline constexpr GLenum GL_RED = 0x1903;
inline constexpr GLenum GL_RGBA = 0x1908;
inline constexpr GLenum GL_VENDOR = 0x1F00;
inline constexpr GLenum GL_RENDERER = 0x1F01;
inline constexpr GLenum GL_VERSION = 0x1F02;
inline constexpr GLenum GL_LINEAR = 0x2601;
inline constexpr GLenum GL_TEXTURE_MAG_FILTER = 0x2800;
inline constexpr GLenum GL_TEXTURE_MIN_FILTER = 0x2801;
inline constexpr GLenum GL_TEXTURE_WRAP_S = 0x2802;
inline constexpr GLenum GL_TEXTURE_WRAP_T = 0x2803;
inline constexpr GLbitfield GL_COLOR_BUFFER_BIT = 0x4000;
inline constexpr GLenum GL_FUNC_ADD = 0x8006;
inline constexpr GLenum GL_RGBA8 = 0x8058;
inline constexpr GLenum GL_MULTISAMPLE = 0x809D;
inline constexpr GLenum GL_CLAMP_TO_EDGE = 0x812F;
inline constexpr GLenum GL_MAJOR_VERSION = 0x821B;
inline constexpr GLenum GL_MINOR_VERSION = 0x821C;
inline constexpr GLenum GL_R8 = 0x8229;
inline constexpr GLenum GL_TEXTURE0 = 0x84C0;
inline constexpr GLenum GL_MAX_RENDERBUFFER_SIZE = 0x84E8;
inline constexpr GLenum GL_ARRAY_BUFFER = 0x8892;
inline constexpr GLenum GL_ELEMENT_ARRAY_BUFFER = 0x8893;
inline constexpr GLenum GL_STREAM_DRAW = 0x88E0;
inline constexpr GLenum GL_FRAGMENT_SHADER = 0x8B30;
inline constexpr GLenum GL_VERTEX_SHADER = 0x8B31;
inline constexpr GLenum GL_COMPILE_STATUS = 0x8B81;
inline constexpr GLenum GL_LINK_STATUS = 0x8B82;
inline constexpr GLenum GL_INFO_LOG_LENGTH = 0x8B84;
inline constexpr GLenum GL_FRAMEBUFFER_COMPLETE = 0x8CD5;
inline constexpr GLenum GL_COLOR_ATTACHMENT0 = 0x8CE0;
inline constexpr GLenum GL_FRAMEBUFFER = 0x8D40;
inline constexpr GLenum GL_FRAMEBUFFER_SRGB = 0x8DB9;
inline constexpr GLenum GL_TEXTURE_SWIZZLE_R = 0x8E42;
inline constexpr GLenum GL_TEXTURE_SWIZZLE_G = 0x8E43;
inline constexpr GLenum GL_TEXTURE_SWIZZLE_B = 0x8E44;
inline constexpr GLenum GL_TEXTURE_SWIZZLE_A = 0x8E45;

// ── Functions ────────────────────────────────────────────────────────────────

/// X(return type, member name, parameter list) for every GL function nymph
/// calls; the GL name is "gl" + member name.
#define NYMPH_GL_FUNCTIONS(X)                                                                                      \
  X(const GLubyte*, GetString, (GLenum name))                                                                      \
  X(void, GetIntegerv, (GLenum pname, GLint * data))                                                               \
  X(GLenum, GetError, ())                                                                                          \
  X(void, Enable, (GLenum cap))                                                                                    \
  X(void, Disable, (GLenum cap))                                                                                   \
  X(void, Viewport, (GLint x, GLint y, GLsizei width, GLsizei height))                                             \
  X(void, Scissor, (GLint x, GLint y, GLsizei width, GLsizei height))                                              \
  X(void, ClearColor, (GLfloat r, GLfloat g, GLfloat b, GLfloat a))                                                \
  X(void, Clear, (GLbitfield mask))                                                                                \
  X(void, PixelStorei, (GLenum pname, GLint param))                                                                \
  X(void, ReadPixels, (GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, void* pixels)) \
  X(void, Finish, ())                                                                                              \
  X(void, GenTextures, (GLsizei n, GLuint * textures))                                                             \
  X(void, DeleteTextures, (GLsizei n, const GLuint* textures))                                                     \
  X(void, BindTexture, (GLenum target, GLuint texture))                                                            \
  X(void, TexParameteri, (GLenum target, GLenum pname, GLint param))                                               \
  X(void,                                                                                                          \
    TexImage2D,                                                                                                    \
    (GLenum target,                                                                                                \
     GLint level,                                                                                                  \
     GLint internal_format,                                                                                        \
     GLsizei width,                                                                                                \
     GLsizei height,                                                                                               \
     GLint border,                                                                                                 \
     GLenum format,                                                                                                \
     GLenum type,                                                                                                  \
     const void* pixels))                                                                                          \
  X(void, ActiveTexture, (GLenum texture))                                                                         \
  X(void, BlendEquation, (GLenum mode))                                                                            \
  X(void, BlendFuncSeparate, (GLenum src_rgb, GLenum dst_rgb, GLenum src_alpha, GLenum dst_alpha))                 \
  X(void, GenBuffers, (GLsizei n, GLuint * buffers))                                                               \
  X(void, DeleteBuffers, (GLsizei n, const GLuint* buffers))                                                       \
  X(void, BindBuffer, (GLenum target, GLuint buffer))                                                              \
  X(void, BufferData, (GLenum target, GLsizeiptr size, const void* data, GLenum usage))                            \
  X(void, GenVertexArrays, (GLsizei n, GLuint * arrays))                                                           \
  X(void, DeleteVertexArrays, (GLsizei n, const GLuint* arrays))                                                   \
  X(void, BindVertexArray, (GLuint array))                                                                         \
  X(void, EnableVertexAttribArray, (GLuint index))                                                                 \
  X(void,                                                                                                          \
    VertexAttribPointer,                                                                                           \
    (GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void* offset))             \
  X(GLuint, CreateShader, (GLenum type))                                                                           \
  X(void, ShaderSource, (GLuint shader, GLsizei count, const GLchar* const* strings, const GLint* lengths))        \
  X(void, CompileShader, (GLuint shader))                                                                          \
  X(void, GetShaderiv, (GLuint shader, GLenum pname, GLint * params))                                              \
  X(void, GetShaderInfoLog, (GLuint shader, GLsizei size, GLsizei * length, GLchar * log))                         \
  X(void, DeleteShader, (GLuint shader))                                                                           \
  X(GLuint, CreateProgram, ())                                                                                     \
  X(void, AttachShader, (GLuint program, GLuint shader))                                                           \
  X(void, LinkProgram, (GLuint program))                                                                           \
  X(void, GetProgramiv, (GLuint program, GLenum pname, GLint * params))                                            \
  X(void, GetProgramInfoLog, (GLuint program, GLsizei size, GLsizei * length, GLchar * log))                       \
  X(void, DeleteProgram, (GLuint program))                                                                         \
  X(void, UseProgram, (GLuint program))                                                                            \
  X(GLint, GetUniformLocation, (GLuint program, const GLchar* name))                                               \
  X(void, Uniform1i, (GLint location, GLint value))                                                                \
  X(void, UniformMatrix4fv, (GLint location, GLsizei count, GLboolean transpose, const GLfloat* value))            \
  X(void, GenFramebuffers, (GLsizei n, GLuint * framebuffers))                                                     \
  X(void, DeleteFramebuffers, (GLsizei n, const GLuint* framebuffers))                                             \
  X(void, BindFramebuffer, (GLenum target, GLuint framebuffer))                                                    \
  X(void, FramebufferTexture2D, (GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level)) \
  X(GLenum, CheckFramebufferStatus, (GLenum target))                                                               \
  X(void, DrawElementsBaseVertex, (GLenum mode, GLsizei count, GLenum type, const void* indices, GLint base_vertex))

/// @brief Looks up a GL function by its name ("glGetString"); returns
///        nullptr when it is not available.
using gl_lookup_fn = void* (*)(const char* name, void* user);

/// @brief The OpenGL functions nymph uses, as pointers filled by load().
///        Valid only while the context they were loaded for is current.
struct gl_api {
#define NYMPH_GL_MEMBER(ret, name, params) ret(NYMPH_GLAPI* name) params = nullptr;
  NYMPH_GL_FUNCTIONS(NYMPH_GL_MEMBER)
#undef NYMPH_GL_MEMBER

  /// @brief Fills every pointer through `lookup`.
  /// @return The GL name of the first function `lookup` did not find
  ///         ("glReadPixels"), or an empty string when all were found.
  std::string load(gl_lookup_fn lookup, void* user);

  /// @brief The GL names of every function load() fills, in order.
  static const char* const* names(size_t& count);
};

} // namespace bdg::nymph
