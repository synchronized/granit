// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "renderer/renderer_registry.h"

#include <new>
#include <utility>
#include <vector>

namespace granit::detail {

renderer_registry& renderer_registry::instance() {
  static renderer_registry registry;
  return registry;
}

granit_result renderer_registry::create(std::string_view application_name, bool enable_validation,
                                        std::uint32_t surface_types, granit_renderer& renderer) {
  try {
    auto state = std::make_shared<renderer_state>();
    const auto initialize_result =
        state->initialize(application_name, enable_validation, surface_types);
    if (initialize_result != GRANIT_SUCCESS) {
      return initialize_result;
    }

    std::lock_guard lock{mutex_};
    state->set_domain(allocate_domain());
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

granit_result renderer_registry::destroy(granit_renderer renderer) {
  std::shared_ptr<renderer_state> state;
  std::vector<VkSurfaceKHR> native_surfaces;
  {
    std::lock_guard lock{mutex_};
    if (handles_.find(renderer, resource_type::renderer, 0) == nullptr) {
      return GRANIT_ERROR_INVALID_HANDLE;
    }
    auto found = renderers_.find(renderer);
    if (found == renderers_.end()) {
      return GRANIT_ERROR_INTERNAL;
    }
    state = std::move(found->second);
    renderers_.erase(found);
    for (auto surface = surfaces_.begin(); surface != surfaces_.end();) {
      if (surface->second.renderer == state) {
        native_surfaces.push_back(surface->second.native_handle);
        static_cast<void>(handles_.erase(surface->first, resource_type::surface, state->domain()));
        surface = surfaces_.erase(surface);
      } else {
        ++surface;
      }
    }
    const auto erase_result = handles_.erase(renderer, resource_type::renderer, 0);
    if (erase_result != GRANIT_SUCCESS) {
      return erase_result;
    }
  }
  for (const auto surface : native_surfaces) {
    state->destroy_native_surface(surface);
  }
  // 析构可能等待 GPU 空闲，不应占用全局 registry 锁。
  state.reset();
  return GRANIT_SUCCESS;
}

granit_result renderer_registry::create_win32_surface(granit_renderer renderer,
                                                      void* native_instance, void* native_window,
                                                      granit_surface& surface) {
  try {
    auto state = acquire(renderer);
    if (!state) {
      return GRANIT_ERROR_INVALID_HANDLE;
    }

    VkSurfaceKHR native_surface = VK_NULL_HANDLE;
    const auto create_result =
        state->create_win32_surface(native_instance, native_window, native_surface);
    if (create_result != GRANIT_SUCCESS) {
      return create_result;
    }

    std::lock_guard lock{mutex_};
    const auto renderer_found = renderers_.find(renderer);
    if (renderer_found == renderers_.end() || renderer_found->second != state) {
      state->destroy_native_surface(native_surface);
      return GRANIT_ERROR_INVALID_HANDLE;
    }
    const auto handle = handles_.insert(state.get(), resource_type::surface, state->domain());
    if (handle == GRANIT_NULL_HANDLE) {
      state->destroy_native_surface(native_surface);
      return GRANIT_ERROR_OUT_OF_MEMORY;
    }
    try {
      surfaces_.emplace(handle, surface_record{state, native_surface});
    } catch (...) {
      static_cast<void>(handles_.erase(handle, resource_type::surface, state->domain()));
      state->destroy_native_surface(native_surface);
      throw;
    }
    surface = handle;
    return GRANIT_SUCCESS;
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result renderer_registry::destroy_surface(granit_renderer renderer, granit_surface surface) {
  std::shared_ptr<renderer_state> state;
  VkSurfaceKHR native_surface = VK_NULL_HANDLE;
  {
    std::lock_guard lock{mutex_};
    const auto renderer_found = renderers_.find(renderer);
    if (renderer_found == renderers_.end() ||
        handles_.find(renderer, resource_type::renderer, 0) == nullptr) {
      return GRANIT_ERROR_INVALID_HANDLE;
    }
    state = renderer_found->second;
    if (handles_.find(surface, resource_type::surface, state->domain()) == nullptr) {
      return GRANIT_ERROR_INVALID_HANDLE;
    }
    const auto found = surfaces_.find(surface);
    if (found == surfaces_.end() || found->second.renderer != state) {
      return GRANIT_ERROR_INVALID_HANDLE;
    }
    native_surface = found->second.native_handle;
    surfaces_.erase(found);
    const auto erase_result = handles_.erase(surface, resource_type::surface, state->domain());
    if (erase_result != GRANIT_SUCCESS) {
      return erase_result;
    }
  }
  state->destroy_native_surface(native_surface);
  return GRANIT_SUCCESS;
}

std::shared_ptr<renderer_state> renderer_registry::acquire(granit_renderer renderer) {
  std::lock_guard lock{mutex_};
  if (handles_.find(renderer, resource_type::renderer, 0) == nullptr) {
    return {};
  }
  const auto found = renderers_.find(renderer);
  return found == renderers_.end() ? std::shared_ptr<renderer_state>{} : found->second;
}

std::uint32_t renderer_registry::allocate_domain() noexcept {
  const auto domain = next_domain_++;
  if (next_domain_ == 0) {
    next_domain_ = 1;
  }
  return domain == 0 ? allocate_domain() : domain;
}

} // namespace granit::detail
