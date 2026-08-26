// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "platform/shared_library.h"

#include <utility>

#if defined(_WIN32)
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace granit::detail::platform {
namespace {

bool is_absolute_path(const char* path) noexcept {
  if (path == nullptr || path[0] == '\0') {
    return false;
  }
#if defined(_WIN32)
  const bool drive_path = path[0] != '\0' && path[1] == ':' && (path[2] == '\\' || path[2] == '/');
  const bool unc_path = path[0] == '\\' && path[1] == '\\';
  return drive_path || unc_path;
#else
  return path[0] == '/';
#endif
}

} // namespace

shared_library::~shared_library() { close(); }

shared_library::shared_library(shared_library&& other) noexcept
    : handle_(std::exchange(other.handle_, nullptr)) {}

shared_library& shared_library::operator=(shared_library&& other) noexcept {
  if (this != &other) {
    close();
    handle_ = std::exchange(other.handle_, nullptr);
  }
  return *this;
}

bool shared_library::open(const char* absolute_path) noexcept {
  close();
  if (!is_absolute_path(absolute_path)) {
    return false;
  }
#if defined(_WIN32)
  handle_ = static_cast<void*>(LoadLibraryExA(
      absolute_path, nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32));
#else
  handle_ = dlopen(absolute_path, RTLD_NOW | RTLD_LOCAL);
#endif
  return handle_ != nullptr;
}

void shared_library::close() noexcept {
  if (handle_ == nullptr) {
    return;
  }
#if defined(_WIN32)
  static_cast<void>(FreeLibrary(static_cast<HMODULE>(handle_)));
#else
  static_cast<void>(dlclose(handle_));
#endif
  handle_ = nullptr;
}

void* shared_library::symbol(const char* name) const noexcept {
  if (handle_ == nullptr || name == nullptr || name[0] == '\0') {
    return nullptr;
  }
#if defined(_WIN32)
  return reinterpret_cast<void*>(GetProcAddress(static_cast<HMODULE>(handle_), name));
#else
  return dlsym(handle_, name);
#endif
}

} // namespace granit::detail::platform
