// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "backend/webgpu/renderer_factory.h"
#include "backend/webgpu/renderer_state.h"
#include "renderer/renderer_registry.h"

#include <new>

namespace granit::detail {

granit_result create_webgpu_renderer_static(const granit_backend_plugin_api* api,
                                            std::uint32_t surface_types,
                                            granit_diagnostic_callback diagnostic_callback,
                                            void* diagnostic_user_data, granit_renderer& renderer) {
  try {
    auto state = std::make_shared<webgpu_renderer_state>();
    const auto initialize_result =
        state->initialize_static(api, surface_types, diagnostic_callback, diagnostic_user_data);
    if (initialize_result != GRANIT_SUCCESS)
      return initialize_result;

    return renderer_registry::instance().register_backend(std::move(state), renderer);
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result create_webgpu_renderer_dynamic(std::string_view library_path,
                                             std::uint32_t surface_types,
                                             granit_diagnostic_callback diagnostic_callback,
                                             void* diagnostic_user_data,
                                             granit_renderer& renderer) {
  try {
    auto state = std::make_shared<webgpu_renderer_state>();
    const auto initialize_result = state->initialize_dynamic(
        library_path, surface_types, diagnostic_callback, diagnostic_user_data);
    if (initialize_result != GRANIT_SUCCESS)
      return initialize_result;

    return renderer_registry::instance().register_backend(std::move(state), renderer);
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

} // namespace granit::detail
