// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "backend/plugin_loader.h"

#include <cstddef>

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

bool is_compatible(const granit_backend_plugin_api* api,
                   granit_backend_plugin_kind expected_kind) noexcept {
  constexpr std::size_t minimum_size =
      offsetof(granit_backend_plugin_api, name_length) + sizeof(uint32_t);
  return api != nullptr && api->struct_size >= minimum_size &&
         api->abi_version == GRANIT_BACKEND_PLUGIN_ABI_VERSION && api->kind == expected_kind &&
         api->reserved == 0 && api->name != nullptr && api->name_length != 0;
}

} // namespace

backend_plugin_loader::~backend_plugin_loader() { close(); }

granit_result backend_plugin_loader::open(const char* library_path,
                                          granit_backend_plugin_kind expected_kind) noexcept {
  close();
  if (library_path == nullptr || library_path[0] == '\0') {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }

  handle_ = open_library(library_path);
  if (handle_ == nullptr) {
    return GRANIT_ERROR_BACKEND_UNAVAILABLE;
  }

  const auto query = reinterpret_cast<granit_backend_plugin_query_fn>(
      load_symbol(handle_, GRANIT_BACKEND_PLUGIN_QUERY_SYMBOL));
  if (query == nullptr) {
    close();
    return GRANIT_ERROR_INCOMPATIBLE_DRIVER;
  }

  api_ = query(GRANIT_BACKEND_PLUGIN_ABI_VERSION);
  if (!is_compatible(api_, expected_kind)) {
    close();
    return GRANIT_ERROR_INCOMPATIBLE_DRIVER;
  }
  return GRANIT_SUCCESS;
}

void backend_plugin_loader::close() noexcept {
  api_ = nullptr;
  if (handle_ == nullptr) {
    return;
  }
  close_library(handle_);
  handle_ = nullptr;
}

} // namespace granit::detail
