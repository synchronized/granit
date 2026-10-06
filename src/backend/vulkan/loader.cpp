// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "backend/vulkan/loader.h"

#include <cstdlib>
#include <filesystem>
#include <string>

#include <volk.h>

#if defined(_WIN32)
#  include <windows.h>
#else
#  include <dlfcn.h>
#  include <limits.h>
#  include <unistd.h>
#endif

namespace granit::detail {
namespace {

enum class loader_mode { system, bundled, auto_select, none };

struct bundled_loader {
#if defined(_WIN32)
  HMODULE handle{};
#else
  void* handle{};
#endif
};

std::string environment_value(const char* name) noexcept {
  try {
#if defined(_WIN32)
    char* value = nullptr;
    std::size_t length = 0;
    if (_dupenv_s(&value, &length, name) != 0 || value == nullptr)
      return {};
    std::string result{value, length};
    free(value);
    return result;
#else
    const auto* value = std::getenv(name);
    return value == nullptr ? std::string{} : std::string{value};
#endif
  } catch (...) {
    return {};
  }
}

bundled_loader& bundled_loader_state() noexcept {
  static bundled_loader state;
  return state;
}

loader_mode configured_mode() noexcept {
  const auto value = environment_value("GRANIT_VULKAN_RUNTIME");
  if (value.empty() || value == "auto")
    return loader_mode::auto_select;
  if (value == "system")
    return loader_mode::system;
  if (value == "bundled")
    return loader_mode::bundled;
  if (value == "none")
    return loader_mode::none;
  return loader_mode::auto_select;
}

std::filesystem::path executable_directory() noexcept {
#if defined(_WIN32)
  std::wstring buffer(256, L'\0');
  for (;;) {
    const auto length = GetModuleFileNameW(nullptr, buffer.data(),
                                           static_cast<DWORD>(buffer.size()));
    if (length == 0)
      return {};
    if (length < buffer.size() - 1)
      return std::filesystem::path{buffer.substr(0, length)}.parent_path();
    buffer.resize(buffer.size() * 2);
  }
#else
  std::string buffer(PATH_MAX, '\0');
  const auto length = readlink("/proc/self/exe", buffer.data(), buffer.size() - 1);
  if (length <= 0)
    return {};
  buffer.resize(static_cast<std::size_t>(length));
  return std::filesystem::path{buffer}.parent_path();
#endif
}

std::filesystem::path bundled_loader_path() noexcept {
  const auto configured = environment_value("GRANIT_VULKAN_LOADER_PATH");
  if (!configured.empty())
    return configured;

  const auto executable = executable_directory();
  if (executable.empty())
    return {};
#if defined(_WIN32)
  return executable / "vulkan-1.dll";
#elif defined(__APPLE__)
  return executable / "../lib/libvulkan.1.dylib";
#else
  return executable / "../lib/libvulkan.so.1";
#endif
}

bool initialize_bundled_loader() noexcept {
  const auto path = bundled_loader_path();
  if (path.empty())
    return false;

  auto& state = bundled_loader_state();
#if defined(_WIN32)
  state.handle = LoadLibraryW(path.c_str());
  if (state.handle == nullptr)
    return false;
  const auto entry = GetProcAddress(state.handle, "vkGetInstanceProcAddr");
#else
  state.handle = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
  if (state.handle == nullptr)
    return false;
  const auto entry = dlsym(state.handle, "vkGetInstanceProcAddr");
#endif
  if (entry == nullptr) {
#if defined(_WIN32)
    FreeLibrary(state.handle);
    state.handle = nullptr;
#else
    dlclose(state.handle);
    state.handle = nullptr;
#endif
    return false;
  }

  volk::volkInitializeCustom(reinterpret_cast<PFN_vkGetInstanceProcAddr>(entry));
  return volk::volkGetInstanceVersion() >= VK_API_VERSION_1_3;
}

vulkan_loader_status create_loader_status() noexcept {
  vulkan_loader_status status{GRANIT_ERROR_BACKEND_UNAVAILABLE, 0};
  const auto mode = configured_mode();
  if (mode == loader_mode::none) {
    return status;
  }

  bool initialized = false;
  const auto explicit_path = environment_value("GRANIT_VULKAN_LOADER_PATH");
  if (!explicit_path.empty()) {
    initialized = initialize_bundled_loader();
  } else if (mode == loader_mode::system || mode == loader_mode::auto_select) {
    initialized = volk::volkInitialize() == VK_SUCCESS;
    if (initialized && mode == loader_mode::auto_select &&
        volk::volkGetInstanceVersion() < VK_API_VERSION_1_3) {
      initialized = false;
    }
  }
  if (!initialized && (mode == loader_mode::bundled || mode == loader_mode::auto_select)) {
    initialized = initialize_bundled_loader();
  }
  if (!initialized) {
    return status;
  }

  status.api_version = volk::volkGetInstanceVersion();
  if (status.api_version < VK_API_VERSION_1_3) {
    status.result = GRANIT_ERROR_INCOMPATIBLE_DRIVER;
    return status;
  }
  status.result = GRANIT_SUCCESS;
  return status;
}

} // namespace

vulkan_loader_status initialize_vulkan_loader() noexcept {
  static const vulkan_loader_status status = create_loader_status();
  return status;
}

} // namespace granit::detail
