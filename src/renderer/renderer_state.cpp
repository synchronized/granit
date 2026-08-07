// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "renderer/renderer_state.h"

#include "backend/vulkan/surface.h"

namespace granit::detail {

granit_result renderer_state::initialize(std::string_view application_name, bool enable_validation,
                                         std::uint32_t surface_types) {
  const auto instance_result = instance_.initialize({.application_name = application_name,
                                                     .enable_validation = enable_validation,
                                                     .surface_types = surface_types});
  if (instance_result != GRANIT_SUCCESS) {
    return instance_result;
  }

  const auto device_result = device_.initialize(instance_, surface_types);
  if (device_result != GRANIT_SUCCESS) {
    instance_.reset();
    return device_result;
  }
  surface_types_ = surface_types;
  return GRANIT_SUCCESS;
}

granit_result renderer_state::create_win32_surface(void* native_instance, void* native_window,
                                                   VkSurfaceKHR& surface) noexcept {
  std::lock_guard lock{surface_mutex_};
  if ((surface_types_ & GRANIT_SURFACE_TYPE_WIN32_BIT) == 0) {
    return GRANIT_ERROR_UNSUPPORTED;
  }
  return detail::create_win32_surface(instance_, device_, native_instance, native_window, surface);
}

void renderer_state::destroy_native_surface(VkSurfaceKHR surface) noexcept {
  std::lock_guard lock{surface_mutex_};
  detail::destroy_surface(instance_, surface);
}

} // namespace granit::detail
