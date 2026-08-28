// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "renderer_registry.h"

#include "backend/plugin_api.h"

#include <new>
#include <utility>

extern "C" const granit_backend_plugin_api*
granit_backend_plugin_query(std::uint32_t requested_abi) noexcept;

namespace granit::detail {

web_renderer_registry& web_renderer_registry::instance() {
  static web_renderer_registry registry;
  return registry;
}

granit_result web_renderer_registry::create(granit_diagnostic_callback diagnostic_callback,
                                            void* diagnostic_user_data, granit_renderer& renderer) {
  try {
    auto state = std::make_shared<webgpu_renderer_state>();
    const auto result =
        state->initialize_static(granit_backend_plugin_query(GRANIT_BACKEND_PLUGIN_ABI_VERSION),
                                 diagnostic_callback, diagnostic_user_data);
    if (result != GRANIT_SUCCESS) {
      return result;
    }

    std::lock_guard lock{mutex_};
    const auto handle = handles_.insert(state.get(), resource_type::renderer, 0);
    if (handle == GRANIT_NULL_HANDLE) {
      return GRANIT_ERROR_OUT_OF_MEMORY;
    }
    try {
      renderers_.emplace(handle, std::move(state));
    } catch (...) {
      static_cast<void>(handles_.erase(handle, resource_type::renderer, 0));
      throw;
    }
    renderer = handle;
    return GRANIT_SUCCESS;
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result web_renderer_registry::destroy(granit_renderer renderer) {
  std::shared_ptr<webgpu_renderer_state> state;
  {
    std::lock_guard lock{mutex_};
    if (handles_.find(renderer, resource_type::renderer, 0) == nullptr) {
      return GRANIT_ERROR_INVALID_HANDLE;
    }
    const auto found = renderers_.find(renderer);
    if (found == renderers_.end()) {
      return GRANIT_ERROR_INTERNAL;
    }
    state = std::move(found->second);
    renderers_.erase(found);
    const auto result = handles_.erase(renderer, resource_type::renderer, 0);
    if (result != GRANIT_SUCCESS) {
      return result;
    }
  }
  state.reset();
  return GRANIT_SUCCESS;
}

granit_result web_renderer_registry::get_limits(granit_renderer renderer,
                                                granit_renderer_limits& limits) {
  const auto state = acquire(renderer);
  if (!state) {
    return GRANIT_ERROR_INVALID_HANDLE;
  }
  const auto& capabilities = state->capabilities();
  limits.reserved = 0;
  limits.uniform_buffer_offset_alignment = capabilities.uniform_buffer_offset_alignment;
  limits.max_uniform_buffer_binding_size = capabilities.max_uniform_buffer_binding_size;
  return GRANIT_SUCCESS;
}

granit_result web_renderer_registry::get_status(granit_renderer renderer,
                                                granit_renderer_status& status) {
  const auto state = acquire(renderer);
  if (!state) {
    return GRANIT_ERROR_INVALID_HANDLE;
  }
  status.reserved = 0;
  const auto lifecycle = state->lifecycle_status();
  switch (lifecycle.state) {
  case backend_lifecycle_state::initializing:
    status.state = GRANIT_RENDERER_STATE_INITIALIZING;
    break;
  case backend_lifecycle_state::ready:
    status.state = GRANIT_RENDERER_STATE_READY;
    break;
  case backend_lifecycle_state::failed:
    status.state = GRANIT_RENDERER_STATE_FAILED;
    break;
  case backend_lifecycle_state::device_lost:
    status.state = GRANIT_RENDERER_STATE_DEVICE_LOST;
    break;
  }
  status.failure_result = lifecycle.failure_result;
  return GRANIT_SUCCESS;
}

granit_result web_renderer_registry::process_events(granit_renderer renderer) {
  const auto state = acquire(renderer);
  return state ? state->process_backend_events() : GRANIT_ERROR_INVALID_HANDLE;
}

std::shared_ptr<webgpu_renderer_state> web_renderer_registry::acquire(granit_renderer renderer) {
  std::lock_guard lock{mutex_};
  if (handles_.find(renderer, resource_type::renderer, 0) == nullptr) {
    return {};
  }
  const auto found = renderers_.find(renderer);
  return found == renderers_.end() ? std::shared_ptr<webgpu_renderer_state>{} : found->second;
}

} // namespace granit::detail
