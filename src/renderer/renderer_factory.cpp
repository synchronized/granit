// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "renderer/renderer_factory.h"

#include "renderer/renderer_registry.h"

#include <string_view>

namespace granit::detail {

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
  if (backend == GRANIT_RENDERER_BACKEND_WEBGPU) {
    return backend_path.empty() ? GRANIT_ERROR_BACKEND_UNAVAILABLE
                                : registry.create_webgpu_dynamic(backend_path, diagnostic_callback,
                                                                 diagnostic_user_data, renderer);
  }

  const auto vulkan_result =
      registry.create(application_name, validation_enabled, surface_types, frames_in_flight,
                      diagnostic_callback, diagnostic_user_data, renderer);
  if (backend == GRANIT_RENDERER_BACKEND_VULKAN || vulkan_result == GRANIT_SUCCESS ||
      backend_path.empty()) {
    return vulkan_result;
  }
  if (vulkan_result != GRANIT_ERROR_BACKEND_UNAVAILABLE &&
      vulkan_result != GRANIT_ERROR_INCOMPATIBLE_DRIVER &&
      vulkan_result != GRANIT_ERROR_NO_SUITABLE_DEVICE &&
      vulkan_result != GRANIT_ERROR_UNSUPPORTED) {
    return vulkan_result;
  }
  const auto webgpu_result = registry.create_webgpu_dynamic(backend_path, diagnostic_callback,
                                                            diagnostic_user_data, renderer);
  return webgpu_result == GRANIT_SUCCESS ? GRANIT_SUCCESS : vulkan_result;
}

} // namespace granit::detail
