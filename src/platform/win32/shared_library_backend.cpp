// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "platform/shared_library.h"

#include <windows.h>

#include <filesystem>
#include <string>
#include <vector>

namespace granit::detail::platform {
namespace {
const int module_marker{};
}

std::string module_directory() noexcept {
  HMODULE module{};
  if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                             GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                         reinterpret_cast<LPCSTR>(&module_marker), &module) == 0) {
    return {};
  }
  try {
    std::vector<char> path(512);
    for (;;) {
      const auto length = GetModuleFileNameA(module, path.data(), static_cast<DWORD>(path.size()));
      if (length == 0)
        return {};
      if (length < path.size() - 1)
        return std::filesystem::path{std::string_view{path.data(), length}}.parent_path().string();
      path.resize(path.size() * 2);
    }
  } catch (...) {
    return {};
  }
}

bool shared_library::open(const char* absolute_path) noexcept {
  close();
  if (absolute_path == nullptr)
    return false;
  const std::string_view path{absolute_path};
  const bool drive_path =
      path.size() >= 3 && path[1] == ':' && (path[2] == '\\' || path[2] == '/');
  const bool unc_path = path.size() >= 2 && path[0] == '\\' && path[1] == '\\';
  if (!drive_path && !unc_path)
    return false;
  handle_ = static_cast<void*>(LoadLibraryExA(
      absolute_path, nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32));
  return handle_ != nullptr;
}

void shared_library::close() noexcept {
  if (handle_ == nullptr)
    return;
  static_cast<void>(FreeLibrary(static_cast<HMODULE>(handle_)));
  handle_ = nullptr;
}

void* shared_library::symbol(const char* name) const noexcept {
  if (handle_ == nullptr || name == nullptr || name[0] == '\0')
    return nullptr;
  return reinterpret_cast<void*>(GetProcAddress(static_cast<HMODULE>(handle_), name));
}

} // namespace granit::detail::platform
