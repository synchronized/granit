// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "renderer/renderer_registry.h"

#include "backend/vulkan/vulkan_renderer_state.h"

#include <new>

namespace granit::detail {

granit_result renderer_registry::create(std::string_view application_name, bool enable_validation,
                                        std::uint32_t surface_types, std::uint32_t frames_in_flight,
                                        granit_diagnostic_callback diagnostic_callback,
                                        void* diagnostic_user_data, granit_renderer& renderer) {
  try {
    auto state = std::make_shared<vulkan_renderer_state>();
    const auto initialize_result =
        state->initialize(application_name, enable_validation, surface_types, frames_in_flight,
                          diagnostic_callback, diagnostic_user_data);
    if (initialize_result != GRANIT_SUCCESS)
      return initialize_result;

    return register_backend(std::move(state), renderer);
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

} // namespace granit::detail
