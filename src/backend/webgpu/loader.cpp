// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "backend/webgpu/loader.h"

#if defined(_WIN32)
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace granit::detail {
namespace {

void* open_library(const char* path) noexcept {
#if defined(_WIN32)
  return static_cast<void*>(LoadLibraryA(path));
#else
  return dlopen(path, RTLD_NOW | RTLD_LOCAL);
#endif
}

void close_library(void* handle) noexcept {
#if defined(_WIN32)
  static_cast<void>(FreeLibrary(static_cast<HMODULE>(handle)));
#else
  static_cast<void>(dlclose(handle));
#endif
}

void* load_symbol(void* handle, const char* name) noexcept {
#if defined(_WIN32)
  return reinterpret_cast<void*>(GetProcAddress(static_cast<HMODULE>(handle), name));
#else
  return dlsym(handle, name);
#endif
}

} // namespace

webgpu_loader::~webgpu_loader() { close(); }

granit_result webgpu_loader::open(const char* library_path) noexcept {
  close();
  if (library_path == nullptr || library_path[0] == '\0') {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }

  handle_ = open_library(library_path);
  if (handle_ == nullptr) {
    return GRANIT_ERROR_BACKEND_UNAVAILABLE;
  }

  create_instance_ =
      reinterpret_cast<create_instance_fn>(load_symbol(handle_, "wgpuCreateInstance"));
  if (create_instance_ == nullptr) {
    close();
    return GRANIT_ERROR_INCOMPATIBLE_DRIVER;
  }
  return GRANIT_SUCCESS;
}

void webgpu_loader::close() noexcept {
  create_instance_ = nullptr;
  if (handle_ == nullptr) {
    return;
  }
  close_library(handle_);
  handle_ = nullptr;
}

} // namespace granit::detail
