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

renderer_registry::surface_record::~surface_record() {
  if (renderer && native_handle != VK_NULL_HANDLE) {
    renderer->destroy_native_surface(native_handle);
  }
}

renderer_registry::swapchain_record::~swapchain_record() {
  if (renderer && native) {
    renderer->destroy_native_swapchain(*native);
  }
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
  std::vector<std::shared_ptr<swapchain_record>> native_swapchains;
  std::vector<std::shared_ptr<surface_record>> native_surfaces;
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
    for (auto swapchain = swapchains_.begin(); swapchain != swapchains_.end();) {
      if (swapchain->second->renderer == state) {
        native_swapchains.push_back(std::move(swapchain->second));
        static_cast<void>(
            handles_.erase(swapchain->first, resource_type::swapchain, state->domain()));
        swapchain = swapchains_.erase(swapchain);
      } else {
        ++swapchain;
      }
    }
    for (auto surface = surfaces_.begin(); surface != surfaces_.end();) {
      if (surface->second->renderer == state) {
        native_surfaces.push_back(std::move(surface->second));
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
  native_swapchains.clear();
  native_surfaces.clear();
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

    auto record = std::make_shared<surface_record>();
    record->renderer = state;
    const auto create_result =
        state->create_win32_surface(native_instance, native_window, record->native_handle);
    if (create_result != GRANIT_SUCCESS) {
      return create_result;
    }

    std::lock_guard lock{mutex_};
    const auto renderer_found = renderers_.find(renderer);
    if (renderer_found == renderers_.end() || renderer_found->second != state) {
      return GRANIT_ERROR_INVALID_HANDLE;
    }
    const auto handle = handles_.insert(record.get(), resource_type::surface, state->domain());
    if (handle == GRANIT_NULL_HANDLE) {
      return GRANIT_ERROR_OUT_OF_MEMORY;
    }
    try {
      surfaces_.emplace(handle, std::move(record));
    } catch (...) {
      static_cast<void>(handles_.erase(handle, resource_type::surface, state->domain()));
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
  std::shared_ptr<surface_record> native_surface;
  std::vector<std::shared_ptr<swapchain_record>> native_swapchains;
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
    if (found == surfaces_.end() || found->second->renderer != state) {
      return GRANIT_ERROR_INVALID_HANDLE;
    }
    for (auto swapchain = swapchains_.begin(); swapchain != swapchains_.end();) {
      if (swapchain->second->surface == found->second) {
        native_swapchains.push_back(std::move(swapchain->second));
        static_cast<void>(
            handles_.erase(swapchain->first, resource_type::swapchain, state->domain()));
        swapchain = swapchains_.erase(swapchain);
      } else {
        ++swapchain;
      }
    }
    native_surface = std::move(found->second);
    surfaces_.erase(found);
    const auto erase_result = handles_.erase(surface, resource_type::surface, state->domain());
    if (erase_result != GRANIT_SUCCESS) {
      return erase_result;
    }
  }
  native_swapchains.clear();
  native_surface.reset();
  return GRANIT_SUCCESS;
}

granit_result renderer_registry::create_swapchain(granit_renderer renderer, granit_surface surface,
                                                  const vulkan_swapchain_desc& desc,
                                                  granit_swapchain& swapchain) {
  try {
    std::shared_ptr<renderer_state> state;
    std::shared_ptr<surface_record> surface_state;
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
      const auto surface_found = surfaces_.find(surface);
      if (surface_found == surfaces_.end() || surface_found->second->renderer != state) {
        return GRANIT_ERROR_INVALID_HANDLE;
      }
      surface_state = surface_found->second;
    }

    auto record = std::make_shared<swapchain_record>();
    record->renderer = state;
    record->surface = surface_state;
    record->native = std::make_unique<vulkan_swapchain>();
    const auto create_result =
        state->create_swapchain(surface_state->native_handle, desc, *record->native);
    if (create_result != GRANIT_SUCCESS) {
      return create_result;
    }

    std::lock_guard lock{mutex_};
    const auto surface_found = surfaces_.find(surface);
    if (surface_found == surfaces_.end() || surface_found->second != surface_state) {
      return GRANIT_ERROR_INVALID_HANDLE;
    }
    const auto handle = handles_.insert(record.get(), resource_type::swapchain, state->domain());
    if (handle == GRANIT_NULL_HANDLE) {
      return GRANIT_ERROR_OUT_OF_MEMORY;
    }
    try {
      swapchains_.emplace(handle, std::move(record));
    } catch (...) {
      static_cast<void>(handles_.erase(handle, resource_type::swapchain, state->domain()));
      throw;
    }
    swapchain = handle;
    return GRANIT_SUCCESS;
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result renderer_registry::recreate_swapchain(granit_renderer renderer,
                                                    granit_swapchain swapchain,
                                                    const vulkan_swapchain_desc& desc) {
  std::shared_ptr<swapchain_record> record;
  {
    std::lock_guard lock{mutex_};
    const auto renderer_found = renderers_.find(renderer);
    if (renderer_found == renderers_.end() ||
        handles_.find(renderer, resource_type::renderer, 0) == nullptr) {
      return GRANIT_ERROR_INVALID_HANDLE;
    }
    const auto& state = renderer_found->second;
    if (handles_.find(swapchain, resource_type::swapchain, state->domain()) == nullptr) {
      return GRANIT_ERROR_INVALID_HANDLE;
    }
    const auto found = swapchains_.find(swapchain);
    if (found == swapchains_.end() || found->second->renderer != state) {
      return GRANIT_ERROR_INVALID_HANDLE;
    }
    record = found->second;
  }
  return record->renderer->recreate_swapchain(record->surface->native_handle, desc,
                                              *record->native);
}

granit_result renderer_registry::get_swapchain_info(granit_renderer renderer,
                                                    granit_swapchain swapchain,
                                                    vulkan_swapchain_info& info) {
  std::lock_guard lock{mutex_};
  const auto renderer_found = renderers_.find(renderer);
  if (renderer_found == renderers_.end() ||
      handles_.find(renderer, resource_type::renderer, 0) == nullptr) {
    return GRANIT_ERROR_INVALID_HANDLE;
  }
  const auto& state = renderer_found->second;
  if (handles_.find(swapchain, resource_type::swapchain, state->domain()) == nullptr) {
    return GRANIT_ERROR_INVALID_HANDLE;
  }
  const auto found = swapchains_.find(swapchain);
  if (found == swapchains_.end() || found->second->renderer != state) {
    return GRANIT_ERROR_INVALID_HANDLE;
  }
  info = state->get_swapchain_info(*found->second->native);
  return GRANIT_SUCCESS;
}

granit_result renderer_registry::destroy_swapchain(granit_renderer renderer,
                                                   granit_swapchain swapchain) {
  std::shared_ptr<swapchain_record> record;
  {
    std::lock_guard lock{mutex_};
    const auto renderer_found = renderers_.find(renderer);
    if (renderer_found == renderers_.end() ||
        handles_.find(renderer, resource_type::renderer, 0) == nullptr) {
      return GRANIT_ERROR_INVALID_HANDLE;
    }
    const auto& state = renderer_found->second;
    if (handles_.find(swapchain, resource_type::swapchain, state->domain()) == nullptr) {
      return GRANIT_ERROR_INVALID_HANDLE;
    }
    const auto found = swapchains_.find(swapchain);
    if (found == swapchains_.end() || found->second->renderer != state) {
      return GRANIT_ERROR_INVALID_HANDLE;
    }
    record = std::move(found->second);
    swapchains_.erase(found);
    const auto erase_result = handles_.erase(swapchain, resource_type::swapchain, state->domain());
    if (erase_result != GRANIT_SUCCESS) {
      return erase_result;
    }
  }
  record.reset();
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
