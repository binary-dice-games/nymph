// MIT License © 2026 Binary Dice Games
/// @file shared_library.cpp
/// @brief shared_library over `dlopen` (POSIX) or `LoadLibraryW` (Windows).
#include "render/gl/shared_library.hpp"
#include <string>
#include <utility>
#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace bdg::nymph {

namespace {

#if defined(_WIN32)
void* load(const std::string& name, std::string& error, bool pin) {
  // Library names are ASCII; widen them byte for byte.
  const std::wstring wide(name.begin(), name.end());
  HMODULE module = LoadLibraryW(wide.c_str());
  if (module == nullptr) {
    error = name + ": cannot be loaded (Windows error " + std::to_string(GetLastError()) + ")";
    return nullptr;
  }
  if (pin) {
    HMODULE pinned = nullptr;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_PIN, wide.c_str(), &pinned);
  }
  return reinterpret_cast<void*>(module);
}

void unload(void* handle) {
  FreeLibrary(reinterpret_cast<HMODULE>(handle));
}

void* find(void* handle, const char* name) {
  // FARPROC to void*: the caller casts back to the right function type.
  return reinterpret_cast<void*>(GetProcAddress(reinterpret_cast<HMODULE>(handle), name));
}
#else
void* load(const std::string& name, std::string& error, bool pin) {
  void* handle = dlopen(name.c_str(), RTLD_NOW | RTLD_LOCAL | (pin ? RTLD_NODELETE : 0));
  if (handle == nullptr) {
    const char* message = dlerror();
    error = message != nullptr ? message : name + ": cannot be loaded";
  }
  return handle;
}

void unload(void* handle) {
  dlclose(handle);
}

void* find(void* handle, const char* name) {
  return dlsym(handle, name);
}
#endif

} // namespace

shared_library::~shared_library() {
  close();
}

shared_library::shared_library(shared_library&& other) noexcept
    : handle_(std::exchange(other.handle_, nullptr)), name_(std::move(other.name_)) {}

shared_library& shared_library::operator=(shared_library&& other) noexcept {
  if (this != &other) {
    close();
    handle_ = std::exchange(other.handle_, nullptr);
    name_ = std::move(other.name_);
  }
  return *this;
}

shared_library shared_library::open(std::span<const std::string> names, std::string& error, bool pin) {
  std::string errors;
  for (const std::string& name : names) {
    std::string message;
    if (void* handle = load(name, message, pin)) {
      shared_library lib;
      lib.handle_ = handle;
      lib.name_ = name;
      return lib;
    }
    errors += (errors.empty() ? "" : "; ") + message;
  }
  error = errors.empty() ? "no library name given" : errors;
  return {};
}

void* shared_library::symbol(const char* name) const noexcept {
  return handle_ != nullptr ? find(handle_, name) : nullptr;
}

void shared_library::close() noexcept {
  if (handle_ != nullptr) {
    unload(handle_);
    handle_ = nullptr;
  }
}

} // namespace bdg::nymph
