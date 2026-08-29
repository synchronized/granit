// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "platform/shared_library.h"

#include <dlfcn.h>

#include <filesystem>
#include <string>

namespace granit::detail::platform {
namespace {
const int module_marker{};
}

std::string module_directory() noexcept {
  Dl_info info{};
  if (dladdr(&module_marker, &info) == 0 || info.dli_fname == nullptr) {
    return {};
  }
  try {
    return std::filesystem::absolute(info.dli_fname).parent_path().string();
  } catch (...) {
    return {};
  }
}

bool shared_library::open(const char* absolute_path) noexcept {
  close();
  if (absolute_path == nullptr || absolute_path[0] != '/')
    return false;
  handle_ = dlopen(absolute_path, RTLD_NOW | RTLD_LOCAL);
  return handle_ != nullptr;
}

void shared_library::close() noexcept {
  if (handle_ == nullptr)
    return;
  static_cast<void>(dlclose(handle_));
  handle_ = nullptr;
}

void* shared_library::symbol(const char* name) const noexcept {
  if (handle_ == nullptr || name == nullptr || name[0] == '\0')
    return nullptr;
  return dlsym(handle_, name);
}

} // namespace granit::detail::platform
