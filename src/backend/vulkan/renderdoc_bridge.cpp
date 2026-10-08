// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "backend/vulkan/renderdoc_bridge.h"

#include "core/diagnostic_sink.h"

#include <cstdlib>
#include <filesystem>
#include <string>
#include <string_view>

#if defined(_WIN32)
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace granit::detail {
namespace {

constexpr std::uint32_t renderdoc_api_version_1_0_0 = 10000;

#if defined(_WIN32)
#define GRANIT_RENDERDOC_CC __cdecl
#else
#define GRANIT_RENDERDOC_CC
#endif

using get_api_function = int(GRANIT_RENDERDOC_CC*)(std::uint32_t, void**);
using opaque_api_function = void(GRANIT_RENDERDOC_CC*)();

// 只保留官方 API 结构中 TriggerCapture 的位置；所有函数指针同宽且保持顺序。
struct renderdoc_api_1_0 {
  opaque_api_function prefix[15]{};
  opaque_api_function trigger_capture{};
  opaque_api_function suffix[13]{};
};

#undef GRANIT_RENDERDOC_CC

std::string environment_value(const char* name) noexcept {
  try {
#if defined(_WIN32)
    char* value = nullptr;
    std::size_t length = 0;
    if (_dupenv_s(&value, &length, name) != 0 || value == nullptr)
      return {};
    try {
      std::string result{value};
      std::free(value);
      return result;
    } catch (...) {
      std::free(value);
      throw;
    }
#else
    const char* value = std::getenv(name);
    return value == nullptr ? std::string{} : std::string{value};
#endif
  } catch (...) {
    return {};
  }
}

renderdoc_bridge::mode configured_mode(std::uint64_t& target_frame) noexcept {
  target_frame = 0;
  const auto value = environment_value("GRANIT_RENDERDOC");
  if (value.empty() || value == "off" || value == "0")
    return renderdoc_bridge::mode::off;
  if (value == "trigger" || value == "1")
    return renderdoc_bridge::mode::trigger;
  constexpr std::string_view prefix = "frame:";
  if (value.starts_with(prefix)) {
    try {
      const auto number = std::stoull(value.substr(prefix.size()));
      if (number != 0) {
        target_frame = number;
        return renderdoc_bridge::mode::frame;
      }
    } catch (...) {
    }
  }
  return renderdoc_bridge::mode::off;
}

void* load_renderdoc_module(const std::string& configured_path) noexcept {
#if defined(_WIN32)
  if (!configured_path.empty()) {
    const auto path = std::filesystem::path{configured_path};
    return reinterpret_cast<void*>(LoadLibraryW(path.c_str()));
  }
  return reinterpret_cast<void*>(LoadLibraryW(L"renderdoc.dll"));
#else
  if (!configured_path.empty())
    return dlopen(configured_path.c_str(), RTLD_NOW | RTLD_LOCAL);
#if defined(__APPLE__)
  if (auto* module = dlopen("librenderdoc.dylib", RTLD_NOW | RTLD_LOCAL))
    return module;
#else
  if (auto* module = dlopen("librenderdoc.so", RTLD_NOW | RTLD_LOCAL))
    return module;
  if (auto* module = dlopen("librenderdoc.so.1", RTLD_NOW | RTLD_LOCAL))
    return module;
#endif
  return nullptr;
#endif
}

void unload_renderdoc_module(void* module) noexcept {
  if (module == nullptr)
    return;
#if defined(_WIN32)
  static_cast<void>(FreeLibrary(static_cast<HMODULE>(module)));
#else
  static_cast<void>(dlclose(module));
#endif
}

void* resolve_get_api(void* module) noexcept {
#if defined(_WIN32)
  return reinterpret_cast<void*>(GetProcAddress(static_cast<HMODULE>(module), "RENDERDOC_GetAPI"));
#else
  return dlsym(module, "RENDERDOC_GetAPI");
#endif
}

} // namespace

renderdoc_bridge::~renderdoc_bridge() { reset(); }

void renderdoc_bridge::emit_status(const diagnostic_sink& diagnostics,
                                   const char* message) const noexcept {
  diagnostics.emit(diagnostic_severity::info, diagnostic_category::general, message);
}

void renderdoc_bridge::initialize(const diagnostic_sink& diagnostics) noexcept {
  reset();
  mode_ = configured_mode(target_frame_);
  if (mode_ == mode::off)
    return;

  module_ = load_renderdoc_module(environment_value("GRANIT_RENDERDOC_PATH"));
  if (module_ == nullptr) {
    emit_status(diagnostics, "RenderDoc runtime unavailable; continuing without capture");
    return;
  }
  const auto get_api = reinterpret_cast<get_api_function>(resolve_get_api(module_));
  if (get_api == nullptr || get_api(renderdoc_api_version_1_0_0, &api_) == 0 || api_ == nullptr) {
    emit_status(diagnostics, "RenderDoc API version unavailable; continuing without capture");
    reset();
    return;
  }
  available_ = true;
}

void renderdoc_bridge::trigger_initial_capture() noexcept {
  if (available_ && mode_ == mode::trigger && !triggered_) {
    static_cast<renderdoc_api_1_0*>(api_)->trigger_capture();
    triggered_ = true;
  }
}

void renderdoc_bridge::notify_frame_boundary() noexcept {
  if (!available_ || mode_ != mode::frame || triggered_)
    return;
  ++presented_frames_;
  if (presented_frames_ == target_frame_) {
    static_cast<renderdoc_api_1_0*>(api_)->trigger_capture();
    triggered_ = true;
  }
}

void renderdoc_bridge::reset() noexcept {
  unload_renderdoc_module(module_);
  mode_ = mode::off;
  target_frame_ = 0;
  presented_frames_ = 0;
  triggered_ = false;
  available_ = false;
  module_ = nullptr;
  api_ = nullptr;
}

} // namespace granit::detail
