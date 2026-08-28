// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "renderer_registry.h"

#include "backend/plugin_api.h"

#include <new>
#include <utility>
#include <vector>

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
  std::vector<std::shared_ptr<frame_record>> frames;
  std::vector<std::shared_ptr<swapchain_record>> swapchains;
  std::vector<std::shared_ptr<surface_record>> surfaces;
  {
    std::lock_guard lock{mutex_};
    if (handles_.find(renderer, resource_type::renderer, 0) == nullptr) {
      return GRANIT_ERROR_INVALID_HANDLE;
    }
    const auto found = renderers_.find(renderer);
    if (found == renderers_.end()) {
      return GRANIT_ERROR_INTERNAL;
    }
    for (auto frame = frames_.begin(); frame != frames_.end();) {
      if (frame->second->swapchain->renderer == found->second) {
        bool ignored{};
        static_cast<void>(found->second->presentation()->cancel_swapchain(
            *frame->second->swapchain->native, ignored));
        erase_backbuffer(*frame->second->swapchain);
        static_cast<void>(handles_.erase(frame->first, resource_type::frame, 0));
        frames.push_back(std::move(frame->second));
        frame = frames_.erase(frame);
      } else {
        ++frame;
      }
    }
    for (auto swapchain = swapchains_.begin(); swapchain != swapchains_.end();) {
      if (swapchain->second->renderer == found->second) {
        static_cast<void>(handles_.erase(swapchain->first, resource_type::swapchain, 0));
        swapchains.push_back(std::move(swapchain->second));
        swapchain = swapchains_.erase(swapchain);
      } else {
        ++swapchain;
      }
    }
    for (auto surface = surfaces_.begin(); surface != surfaces_.end();) {
      if (surface->second->renderer == found->second) {
        static_cast<void>(handles_.erase(surface->first, resource_type::surface, 0));
        surfaces.push_back(std::move(surface->second));
        surface = surfaces_.erase(surface);
      } else {
        ++surface;
      }
    }
    state = std::move(found->second);
    renderers_.erase(found);
    const auto result = handles_.erase(renderer, resource_type::renderer, 0);
    if (result != GRANIT_SUCCESS) {
      return result;
    }
  }
  frames.clear();
  swapchains.clear();
  surfaces.clear();
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

granit_result web_renderer_registry::create_canvas_surface(granit_renderer renderer,
                                                           const char* selector,
                                                           std::uint32_t selector_length,
                                                           granit_surface& surface) {
  try {
    const auto state = acquire(renderer);
    if (!state || state->presentation() == nullptr) {
      return state ? GRANIT_ERROR_NOT_READY : GRANIT_ERROR_INVALID_HANDLE;
    }
    auto record = std::make_shared<surface_record>();
    record->renderer = state;
    record->native = state->presentation()->allocate_surface();
    if (!record->native) {
      return GRANIT_ERROR_OUT_OF_MEMORY;
    }
    const auto result =
        state->presentation()->create_canvas_surface(*record->native, selector, selector_length);
    if (result != GRANIT_SUCCESS) {
      return result;
    }
    std::lock_guard lock{mutex_};
    if (renderers_.find(renderer) == renderers_.end()) {
      return GRANIT_ERROR_INVALID_HANDLE;
    }
    const auto handle = handles_.insert(record.get(), resource_type::surface, 0);
    if (handle == GRANIT_NULL_HANDLE) {
      return GRANIT_ERROR_OUT_OF_MEMORY;
    }
    try {
      surfaces_.emplace(handle, std::move(record));
    } catch (...) {
      static_cast<void>(handles_.erase(handle, resource_type::surface, 0));
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

granit_result web_renderer_registry::destroy_surface(granit_renderer renderer,
                                                     granit_surface surface) {
  std::shared_ptr<surface_record> record;
  {
    std::lock_guard lock{mutex_};
    const auto state = renderers_.find(renderer);
    const auto found = surfaces_.find(surface);
    if (state == renderers_.end() || found == surfaces_.end() ||
        found->second->renderer != state->second) {
      return GRANIT_ERROR_INVALID_HANDLE;
    }
    for (const auto& [handle, swapchain] : swapchains_) {
      static_cast<void>(handle);
      if (swapchain->surface == found->second) {
        return GRANIT_ERROR_NOT_READY;
      }
    }
    record = std::move(found->second);
    surfaces_.erase(found);
    const auto result = handles_.erase(surface, resource_type::surface, 0);
    if (result != GRANIT_SUCCESS) {
      return result;
    }
  }
  record.reset();
  return GRANIT_SUCCESS;
}

granit_result web_renderer_registry::create_swapchain(granit_renderer renderer,
                                                      granit_surface surface,
                                                      const backend_swapchain_desc& desc,
                                                      granit_swapchain& swapchain) {
  try {
    const auto state = acquire(renderer);
    if (!state || state->presentation() == nullptr) {
      return state ? GRANIT_ERROR_NOT_READY : GRANIT_ERROR_INVALID_HANDLE;
    }
    std::shared_ptr<surface_record> surface_record_ptr;
    {
      std::lock_guard lock{mutex_};
      const auto found = surfaces_.find(surface);
      if (found == surfaces_.end() || found->second->renderer != state) {
        return GRANIT_ERROR_INVALID_HANDLE;
      }
      surface_record_ptr = found->second;
    }
    auto record = std::make_shared<swapchain_record>();
    record->renderer = state;
    record->surface = surface_record_ptr;
    record->native = state->presentation()->allocate_swapchain();
    if (!record->native) {
      return GRANIT_ERROR_OUT_OF_MEMORY;
    }
    const auto result =
        state->presentation()->create_swapchain(*surface_record_ptr->native, desc, *record->native);
    if (result != GRANIT_SUCCESS) {
      return result;
    }
    std::lock_guard lock{mutex_};
    const auto surface_found = surfaces_.find(surface);
    if (renderers_.find(renderer) == renderers_.end() || surface_found == surfaces_.end() ||
        surface_found->second != surface_record_ptr) {
      return GRANIT_ERROR_INVALID_HANDLE;
    }
    const auto handle = handles_.insert(record.get(), resource_type::swapchain, 0);
    if (handle == GRANIT_NULL_HANDLE) {
      return GRANIT_ERROR_OUT_OF_MEMORY;
    }
    try {
      swapchains_.emplace(handle, std::move(record));
    } catch (...) {
      static_cast<void>(handles_.erase(handle, resource_type::swapchain, 0));
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

granit_result web_renderer_registry::recreate_swapchain(granit_renderer renderer,
                                                        granit_swapchain swapchain,
                                                        const backend_swapchain_desc& desc) {
  std::lock_guard lock{mutex_};
  const auto state = renderers_.find(renderer);
  const auto found = swapchains_.find(swapchain);
  if (state == renderers_.end() || found == swapchains_.end() ||
      found->second->renderer != state->second) {
    return GRANIT_ERROR_INVALID_HANDLE;
  }
  if (found->second->texture != GRANIT_NULL_HANDLE) {
    return GRANIT_ERROR_NOT_READY;
  }
  return state->second->presentation()->recreate_swapchain(*found->second->native, desc);
}

granit_result web_renderer_registry::get_swapchain_info(granit_renderer renderer,
                                                        granit_swapchain swapchain,
                                                        backend_swapchain_info& info) {
  std::lock_guard lock{mutex_};
  const auto state = renderers_.find(renderer);
  const auto found = swapchains_.find(swapchain);
  if (state == renderers_.end() || found == swapchains_.end() ||
      found->second->renderer != state->second) {
    return GRANIT_ERROR_INVALID_HANDLE;
  }
  return state->second->presentation()->get_swapchain_info(*found->second->native, info);
}

granit_result web_renderer_registry::get_swapchain_backbuffer(granit_renderer renderer,
                                                              granit_swapchain swapchain,
                                                              std::uint32_t index,
                                                              granit_texture& texture,
                                                              granit_texture_view& view) {
  std::lock_guard lock{mutex_};
  const auto state = renderers_.find(renderer);
  const auto found = swapchains_.find(swapchain);
  if (state == renderers_.end() || found == swapchains_.end() ||
      found->second->renderer != state->second) {
    return GRANIT_ERROR_INVALID_HANDLE;
  }
  if (found->second->texture == GRANIT_NULL_HANDLE || found->second->image_index != index) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  texture = found->second->texture;
  view = found->second->view;
  return GRANIT_SUCCESS;
}

granit_result web_renderer_registry::acquire_swapchain(granit_renderer renderer,
                                                       granit_swapchain swapchain,
                                                       granit_frame& frame,
                                                       std::uint32_t& image_index,
                                                       bool& needs_recreate) {
  try {
    std::lock_guard lock{mutex_};
    const auto state = renderers_.find(renderer);
    const auto found = swapchains_.find(swapchain);
    if (state == renderers_.end() || found == swapchains_.end() ||
        found->second->renderer != state->second) {
      return GRANIT_ERROR_INVALID_HANDLE;
    }
    if (found->second->texture != GRANIT_NULL_HANDLE) {
      return GRANIT_ERROR_NOT_READY;
    }
    auto record = std::make_shared<frame_record>();
    record->swapchain = found->second;
    const auto acquire_result =
        state->second->presentation()->acquire_swapchain(*found->second->native, record->acquired);
    if (acquire_result != GRANIT_SUCCESS) {
      return acquire_result;
    }
    auto cancel = [&]() noexcept {
      bool ignored{};
      static_cast<void>(
          state->second->presentation()->cancel_swapchain(*found->second->native, ignored));
    };
    const auto texture_handle = handles_.insert(record->acquired.dynamic_backbuffer.texture.get(),
                                                resource_type::texture, 0);
    if (texture_handle == GRANIT_NULL_HANDLE) {
      cancel();
      return GRANIT_ERROR_OUT_OF_MEMORY;
    }
    const auto view_handle = handles_.insert(record->acquired.dynamic_backbuffer.view.get(),
                                             resource_type::texture_view, 0);
    if (view_handle == GRANIT_NULL_HANDLE) {
      static_cast<void>(handles_.erase(texture_handle, resource_type::texture, 0));
      cancel();
      return GRANIT_ERROR_OUT_OF_MEMORY;
    }
    const auto frame_handle = handles_.insert(record.get(), resource_type::frame, 0);
    if (frame_handle == GRANIT_NULL_HANDLE) {
      static_cast<void>(handles_.erase(view_handle, resource_type::texture_view, 0));
      static_cast<void>(handles_.erase(texture_handle, resource_type::texture, 0));
      cancel();
      return GRANIT_ERROR_OUT_OF_MEMORY;
    }
    try {
      frames_.emplace(frame_handle, std::move(record));
    } catch (...) {
      static_cast<void>(handles_.erase(frame_handle, resource_type::frame, 0));
      static_cast<void>(handles_.erase(view_handle, resource_type::texture_view, 0));
      static_cast<void>(handles_.erase(texture_handle, resource_type::texture, 0));
      cancel();
      throw;
    }
    found->second->texture = texture_handle;
    found->second->view = view_handle;
    found->second->image_index = frames_.at(frame_handle)->acquired.image_index;
    frame = frame_handle;
    image_index = found->second->image_index;
    needs_recreate = frames_.at(frame_handle)->acquired.needs_recreate;
    return GRANIT_SUCCESS;
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

void web_renderer_registry::erase_backbuffer(swapchain_record& swapchain) noexcept {
  if (swapchain.view != GRANIT_NULL_HANDLE) {
    static_cast<void>(handles_.erase(swapchain.view, resource_type::texture_view, 0));
  }
  if (swapchain.texture != GRANIT_NULL_HANDLE) {
    static_cast<void>(handles_.erase(swapchain.texture, resource_type::texture, 0));
  }
  swapchain.texture = GRANIT_NULL_HANDLE;
  swapchain.view = GRANIT_NULL_HANDLE;
  swapchain.image_index = 0;
}

granit_result web_renderer_registry::finish_frame(granit_renderer renderer,
                                                  granit_swapchain swapchain, granit_frame frame,
                                                  bool present, bool& needs_recreate) {
  std::shared_ptr<frame_record> record;
  granit_result result{};
  {
    std::lock_guard lock{mutex_};
    const auto state = renderers_.find(renderer);
    const auto swapchain_found = swapchains_.find(swapchain);
    const auto frame_found = frames_.find(frame);
    if (state == renderers_.end() || swapchain_found == swapchains_.end() ||
        frame_found == frames_.end() || swapchain_found->second->renderer != state->second ||
        frame_found->second->swapchain != swapchain_found->second) {
      return GRANIT_ERROR_INVALID_HANDLE;
    }
    result = present ? state->second->presentation()->present_swapchain(
                           *swapchain_found->second->native, needs_recreate)
                     : state->second->presentation()->cancel_swapchain(
                           *swapchain_found->second->native, needs_recreate);
    if (result != GRANIT_SUCCESS) {
      return result;
    }
    erase_backbuffer(*swapchain_found->second);
    record = std::move(frame_found->second);
    frames_.erase(frame_found);
    static_cast<void>(handles_.erase(frame, resource_type::frame, 0));
  }
  record.reset();
  return GRANIT_SUCCESS;
}

granit_result web_renderer_registry::get_frame_info(granit_renderer renderer,
                                                    granit_swapchain swapchain, granit_frame frame,
                                                    std::uint32_t& frame_slot,
                                                    std::uint32_t& frame_slot_count) {
  std::lock_guard lock{mutex_};
  const auto state = renderers_.find(renderer);
  const auto swapchain_found = swapchains_.find(swapchain);
  const auto frame_found = frames_.find(frame);
  if (state == renderers_.end() || swapchain_found == swapchains_.end() ||
      frame_found == frames_.end() || swapchain_found->second->renderer != state->second ||
      frame_found->second->swapchain != swapchain_found->second) {
    return GRANIT_ERROR_INVALID_HANDLE;
  }
  backend_swapchain_info info{};
  const auto result =
      state->second->presentation()->get_swapchain_info(*swapchain_found->second->native, info);
  if (result == GRANIT_SUCCESS) {
    frame_slot = frame_found->second->acquired.image_index;
    frame_slot_count = info.image_count;
  }
  return result;
}

granit_result web_renderer_registry::destroy_swapchain(granit_renderer renderer,
                                                       granit_swapchain swapchain) {
  std::shared_ptr<swapchain_record> record;
  {
    std::lock_guard lock{mutex_};
    const auto state = renderers_.find(renderer);
    const auto found = swapchains_.find(swapchain);
    if (state == renderers_.end() || found == swapchains_.end() ||
        found->second->renderer != state->second) {
      return GRANIT_ERROR_INVALID_HANDLE;
    }
    if (found->second->texture != GRANIT_NULL_HANDLE) {
      return GRANIT_ERROR_NOT_READY;
    }
    record = std::move(found->second);
    swapchains_.erase(found);
    const auto result = handles_.erase(swapchain, resource_type::swapchain, 0);
    if (result != GRANIT_SUCCESS) {
      return result;
    }
  }
  record.reset();
  return GRANIT_SUCCESS;
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
