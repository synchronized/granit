// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "renderer/renderer_factory.h"

#include "backend/webgpu/renderer_factory.h"
#include "renderer/renderer_registry.h"

#include "core/diagnostic_sink.h"
#include "platform/shared_library.h"

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace granit::detail {
namespace {

#if defined(_WIN32)
constexpr std::string_view webgpu_library_name = "granit_backend_webgpu.dll";
#elif defined(__APPLE__)
constexpr std::string_view webgpu_library_name = "libgranit_backend_webgpu.dylib";
#else
constexpr std::string_view webgpu_library_name = "libgranit_backend_webgpu.so";
#endif

std::vector<std::string> default_webgpu_paths() {
  const auto directory = platform::module_directory();
  if (directory.empty())
    return {};
  const std::filesystem::path module_path{directory};
  std::vector<std::string> paths;
  paths.push_back((module_path / webgpu_library_name).lexically_normal().string());
  paths.push_back(
      (module_path / "granit" / "backends" / webgpu_library_name).lexically_normal().string());
#if defined(_WIN32)
  paths.push_back((module_path / ".." / "lib" / "granit" / "backends" / webgpu_library_name)
                      .lexically_normal()
                      .string());
#endif
  return paths;
}

void emit_backend_attempt(const diagnostic_sink& diagnostics, std::string_view backend,
                          granit_result result) {
  try {
    std::string message{"Renderer 尝试后端 "};
    message.append(backend);
    message.append(" 失败：");
    message.append(granit_result_message(result));
    diagnostics.emit(diagnostic_severity::warning, diagnostic_category::device, message);
  } catch (...) {
  }
}

granit_result create_webgpu(std::string_view requested_path, std::uint32_t surface_types,
                            granit_diagnostic_callback callback, void* user_data,
                            granit_renderer& renderer) {
  if (!requested_path.empty())
    return create_webgpu_renderer_dynamic(requested_path, surface_types, callback, user_data,
                                          renderer);
  granit_result result = GRANIT_ERROR_BACKEND_UNAVAILABLE;
  for (const auto& path : default_webgpu_paths()) {
    result = create_webgpu_renderer_dynamic(path, surface_types, callback, user_data, renderer);
    if (result == GRANIT_SUCCESS || result == GRANIT_ERROR_INCOMPATIBLE_DRIVER)
      return result;
  }
  return result;
}

} // namespace

granit_result create_default_renderer(const granit_renderer_desc& desc, granit_renderer& renderer) {
  constexpr std::string_view default_application_name = "Granit Application";
  const auto application_name =
      desc.application_name == nullptr
          ? default_application_name
          : std::string_view{desc.application_name, desc.application_name_length};
  const auto validation_enabled = (desc.flags & GRANIT_RENDERER_ENABLE_VALIDATION_BIT) != 0;
  const auto surface_types =
      desc.struct_size >= GRANIT_RENDERER_DESC_VERSION_2_SIZE ? desc.surface_types : UINT32_C(0);
  const auto frames_in_flight = desc.struct_size >= GRANIT_RENDERER_DESC_VERSION_3_SIZE
                                    ? desc.frames_in_flight
                                    : GRANIT_DEFAULT_FRAMES_IN_FLIGHT;
  const auto diagnostic_callback =
      desc.struct_size >= GRANIT_RENDERER_DESC_VERSION_4_SIZE ? desc.diagnostic_callback : nullptr;
  auto* diagnostic_user_data =
      desc.struct_size >= GRANIT_RENDERER_DESC_VERSION_4_SIZE ? desc.diagnostic_user_data : nullptr;
  const auto backend = desc.struct_size >= GRANIT_RENDERER_DESC_VERSION_5_SIZE
                           ? desc.backend
                           : GRANIT_RENDERER_BACKEND_AUTO;
  const auto backend_path =
      desc.struct_size >= GRANIT_RENDERER_DESC_VERSION_5_SIZE &&
              desc.backend_library_path != nullptr
          ? std::string_view{desc.backend_library_path, desc.backend_library_path_length}
          : std::string_view{};
  auto& registry = renderer_registry::instance();
  const diagnostic_sink diagnostics{diagnostic_callback, diagnostic_user_data};
  if (backend == GRANIT_RENDERER_BACKEND_WEBGPU) {
    const auto result = create_webgpu(backend_path, surface_types, diagnostic_callback,
                                      diagnostic_user_data, renderer);
    if (result != GRANIT_SUCCESS)
      emit_backend_attempt(diagnostics, "WebGPU", result);
    return result;
  }

  const auto vulkan_result =
      registry.create(application_name, validation_enabled, surface_types, frames_in_flight,
                      diagnostic_callback, diagnostic_user_data, renderer);
  if (backend == GRANIT_RENDERER_BACKEND_VULKAN || vulkan_result == GRANIT_SUCCESS ||
      (vulkan_result != GRANIT_ERROR_BACKEND_UNAVAILABLE &&
       vulkan_result != GRANIT_ERROR_INCOMPATIBLE_DRIVER &&
       vulkan_result != GRANIT_ERROR_NO_SUITABLE_DEVICE &&
       vulkan_result != GRANIT_ERROR_UNSUPPORTED)) {
    return vulkan_result;
  }
  emit_backend_attempt(diagnostics, "Vulkan", vulkan_result);
  const auto webgpu_result = create_webgpu(backend_path, surface_types, diagnostic_callback,
                                           diagnostic_user_data, renderer);
  if (webgpu_result != GRANIT_SUCCESS)
    emit_backend_attempt(diagnostics, "WebGPU", webgpu_result);
  return webgpu_result == GRANIT_SUCCESS ? GRANIT_SUCCESS : vulkan_result;
}

} // namespace granit::detail
