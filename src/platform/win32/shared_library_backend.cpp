// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "platform/shared_library.h"

#include <windows.h>

namespace granit::detail::platform {

bool shared_library::open(const char* absolute_path) noexcept {
  close();
  if (absolute_path == nullptr || absolute_path[0] == '\0')
    return false;
  const bool drive_path =
      absolute_path[1] == ':' && (absolute_path[2] == '\\' || absolute_path[2] == '/');
  const bool unc_path = absolute_path[0] == '\\' && absolute_path[1] == '\\';
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
