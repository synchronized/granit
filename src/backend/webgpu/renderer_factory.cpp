// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "renderer/renderer_factory.h"
#include "renderer/renderer_registry.h"

#include "backend/webgpu/renderer_state.h"

#include <new>

namespace granit::detail {

namespace {

granit_result create_webgpu_renderer(std::uint32_t surface_types,
                                     granit_diagnostic_callback diagnostic_callback,
                                     void* diagnostic_user_data, granit_renderer& renderer) {
  try {
    auto state = std::make_shared<webgpu_renderer_state>();
    const auto initialize_result =
        state->initialize_static(surface_types, diagnostic_callback, diagnostic_user_data);
    if (initialize_result != GRANIT_SUCCESS)
      return initialize_result;

    return renderer_registry::instance().register_backend(std::move(state), renderer);
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

} // namespace

granit_result create_default_renderer(const granit_renderer_desc& desc, granit_renderer& renderer) {
  const auto surface_types = desc.surface_types;
  if ((surface_types & ~GRANIT_SURFACE_TYPE_CANVAS_BIT) != 0) {
    return GRANIT_ERROR_UNSUPPORTED;
  }
  const auto backend = desc.backend;
  if (backend == GRANIT_RENDERER_BACKEND_VULKAN)
    return GRANIT_ERROR_BACKEND_UNAVAILABLE;
  const auto diagnostic_callback = desc.diagnostic_callback;
  auto* diagnostic_user_data = desc.diagnostic_user_data;
  return create_webgpu_renderer(surface_types, diagnostic_callback, diagnostic_user_data, renderer);
}

} // namespace granit::detail
