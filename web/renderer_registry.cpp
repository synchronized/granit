// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "renderer/renderer_registry.h"

#include "backend/webgpu/renderer_state.h"

#include "backend/plugin_api.h"

#include <algorithm>
#include <new>
#include <utility>
#include <vector>

extern "C" const granit_backend_plugin_api*
granit_backend_plugin_query(std::uint32_t requested_abi) noexcept;

namespace granit::detail {

renderer_registry& renderer_registry::instance() {
  static renderer_registry registry;
  return registry;
}

std::uint32_t renderer_registry::allocate_domain() noexcept {
  const auto domain = next_domain_++;
  if (next_domain_ == 0)
    next_domain_ = 1;
  return domain == 0 ? allocate_domain() : domain;
}

granit_result
renderer_registry::create_webgpu_static(const granit_backend_plugin_api* api,
                                        granit_diagnostic_callback diagnostic_callback,
                                        void* diagnostic_user_data, granit_renderer& renderer) {
  try {
    auto state = std::make_shared<webgpu_renderer_state>();
    const auto result = state->initialize_static(api, diagnostic_callback, diagnostic_user_data);
    if (result != GRANIT_SUCCESS) {
      return result;
    }

    std::lock_guard lock{mutex_};
    const auto handle = handles_.insert(state.get(), resource_type::renderer, 0);
    if (handle == GRANIT_NULL_HANDLE) {
      return GRANIT_ERROR_OUT_OF_MEMORY;
    }
    try {
      state->set_domain(allocate_domain());
      backend_renderers_.emplace(handle, state);
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
  std::shared_ptr<webgpu_renderer_state> state;
  std::vector<std::shared_ptr<frame_record>> frames;
  std::vector<std::shared_ptr<swapchain_record>> swapchains;
  std::vector<std::shared_ptr<surface_record>> surfaces;
  std::vector<std::shared_ptr<shader_record>> shaders;
  std::vector<std::shared_ptr<graphics_pipeline_record>> graphics_pipelines;
  std::vector<std::shared_ptr<pipeline_layout_record>> pipeline_layouts;
  std::vector<std::shared_ptr<command_recorder_record>> command_recorders;
  {
    std::lock_guard lock{mutex_};
    if (handles_.find(renderer, resource_type::renderer, 0) == nullptr) {
      return GRANIT_ERROR_INVALID_HANDLE;
    }
    const auto found = renderers_.find(renderer);
    if (found == renderers_.end()) {
      return GRANIT_ERROR_INTERNAL;
    }
    for (auto command = command_recorders_.begin(); command != command_recorders_.end();) {
      if (command->second->owner == found->second) {
        static_cast<void>(handles_.erase(command->first, resource_type::command_recorder, 0));
        command_recorders.push_back(std::move(command->second));
        command = command_recorders_.erase(command);
      } else {
        ++command;
      }
    }
    for (auto frame = frames_.begin(); frame != frames_.end();) {
      if (frame->second->swapchain->owner == found->second) {
        bool ignored{};
        static_cast<void>(frame->second->presentation->cancel_swapchain_frame(
            *frame->second->swapchain->native, frame->second->image_index,
            frame->second->slot_index, ignored));
        erase_dynamic_backbuffer(*frame->second->swapchain);
        static_cast<void>(handles_.erase(frame->first, resource_type::frame, 0));
        frames.push_back(std::move(frame->second));
        frame = frames_.erase(frame);
      } else {
        ++frame;
      }
    }
    for (auto swapchain = swapchains_.begin(); swapchain != swapchains_.end();) {
      if (swapchain->second->owner == found->second) {
        static_cast<void>(handles_.erase(swapchain->first, resource_type::swapchain, 0));
        swapchains.push_back(std::move(swapchain->second));
        swapchain = swapchains_.erase(swapchain);
      } else {
        ++swapchain;
      }
    }
    for (auto surface = surfaces_.begin(); surface != surfaces_.end();) {
      if (surface->second->owner == found->second) {
        static_cast<void>(handles_.erase(surface->first, resource_type::surface, 0));
        surfaces.push_back(std::move(surface->second));
        surface = surfaces_.erase(surface);
      } else {
        ++surface;
      }
    }
    for (auto pipeline = graphics_pipelines_.begin(); pipeline != graphics_pipelines_.end();) {
      if (pipeline->second->owner == found->second) {
        static_cast<void>(handles_.erase(pipeline->first, resource_type::pipeline, 0));
        graphics_pipelines.push_back(std::move(pipeline->second));
        pipeline = graphics_pipelines_.erase(pipeline);
      } else {
        ++pipeline;
      }
    }
    for (auto layout = pipeline_layouts_.begin(); layout != pipeline_layouts_.end();) {
      if (layout->second->owner == found->second) {
        static_cast<void>(handles_.erase(layout->first, resource_type::pipeline_layout, 0));
        pipeline_layouts.push_back(std::move(layout->second));
        layout = pipeline_layouts_.erase(layout);
      } else {
        ++layout;
      }
    }
    for (auto shader = shaders_.begin(); shader != shaders_.end();) {
      if (shader->second->owner == found->second) {
        static_cast<void>(handles_.erase(shader->first, resource_type::shader, 0));
        shaders.push_back(std::move(shader->second));
        shader = shaders_.erase(shader);
      } else {
        ++shader;
      }
    }
    state = std::dynamic_pointer_cast<webgpu_renderer_state>(found->second);
    renderers_.erase(found);
    backend_renderers_.erase(renderer);
    const auto result = handles_.erase(renderer, resource_type::renderer, 0);
    if (result != GRANIT_SUCCESS) {
      return result;
    }
  }
  frames.clear();
  command_recorders.clear();
  swapchains.clear();
  surfaces.clear();
  graphics_pipelines.clear();
  pipeline_layouts.clear();
  shaders.clear();
  state.reset();
  return GRANIT_SUCCESS;
}

granit_result renderer_registry::get_limits(granit_renderer renderer,
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

granit_result renderer_registry::get_status(granit_renderer renderer,
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

granit_result renderer_registry::process_events(granit_renderer renderer) {
  const auto state = acquire(renderer);
  return state ? state->process_backend_events() : GRANIT_ERROR_INVALID_HANDLE;
}

granit_result renderer_registry::set_object_name(granit_renderer, granit_handle, std::string_view) {
  return GRANIT_ERROR_UNSUPPORTED;
}

granit_result renderer_registry::import_pipeline_cache(granit_renderer, const void*,
                                                       std::uint64_t) {
  return GRANIT_ERROR_UNSUPPORTED;
}

granit_result renderer_registry::export_pipeline_cache(granit_renderer, void*, std::uint64_t&) {
  return GRANIT_ERROR_UNSUPPORTED;
}

void renderer_registry::emit_validation_diagnostic(granit_renderer, std::string_view) noexcept {}

granit_result renderer_registry::create_win32_surface(granit_renderer, void*, void*,
                                                      granit_surface&) {
  return GRANIT_ERROR_UNSUPPORTED;
}

granit_result renderer_registry::create_xcb_surface(granit_renderer, void*, std::uint32_t,
                                                    granit_surface&) {
  return GRANIT_ERROR_UNSUPPORTED;
}

granit_result renderer_registry::create_wayland_surface(granit_renderer, void*, void*,
                                                        granit_surface&) {
  return GRANIT_ERROR_UNSUPPORTED;
}

granit_result renderer_registry::create_canvas_surface(granit_renderer renderer,
                                                       std::string_view selector,
                                                       granit_surface& surface) {
  try {
    const auto state = acquire(renderer);
    if (!state || state->presentation() == nullptr) {
      return state ? GRANIT_ERROR_NOT_READY : GRANIT_ERROR_INVALID_HANDLE;
    }
    auto record = std::make_shared<surface_record>();
    record->owner = state;
    record->renderer = state;
    record->native = state->presentation()->allocate_surface();
    if (!record->native) {
      return GRANIT_ERROR_OUT_OF_MEMORY;
    }
    const auto result = state->presentation()->create_canvas_surface(
        *record->native, selector.data(), selector.size());
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

granit_result renderer_registry::destroy_surface(granit_renderer renderer, granit_surface surface) {
  std::shared_ptr<surface_record> record;
  {
    std::lock_guard lock{mutex_};
    const auto state = renderers_.find(renderer);
    const auto found = surfaces_.find(surface);
    if (state == renderers_.end() || found == surfaces_.end() ||
        found->second->owner != state->second) {
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

granit_result renderer_registry::create_swapchain(granit_renderer renderer, granit_surface surface,
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
      if (found == surfaces_.end() || found->second->owner != state) {
        return GRANIT_ERROR_INVALID_HANDLE;
      }
      surface_record_ptr = found->second;
    }
    auto record = std::make_shared<swapchain_record>();
    record->owner = state;
    record->presentation = state;
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

granit_result renderer_registry::recreate_swapchain(granit_renderer renderer,
                                                    granit_swapchain swapchain,
                                                    const backend_swapchain_desc& desc) {
  std::lock_guard lock{mutex_};
  const auto state = renderers_.find(renderer);
  const auto found = swapchains_.find(swapchain);
  if (state == renderers_.end() || found == swapchains_.end() ||
      found->second->owner != state->second) {
    return GRANIT_ERROR_INVALID_HANDLE;
  }
  if (!found->second->textures.empty()) {
    return GRANIT_ERROR_NOT_READY;
  }
  return found->second->presentation->recreate_swapchain(
      *found->second->surface->native, desc, *found->second->native);
}

granit_result renderer_registry::get_swapchain_info(granit_renderer renderer,
                                                    granit_swapchain swapchain,
                                                    backend_swapchain_info& info) {
  std::lock_guard lock{mutex_};
  const auto state = renderers_.find(renderer);
  const auto found = swapchains_.find(swapchain);
  if (state == renderers_.end() || found == swapchains_.end() ||
      found->second->owner != state->second) {
    return GRANIT_ERROR_INVALID_HANDLE;
  }
  info = found->second->presentation->get_swapchain_info(*found->second->native);
  return GRANIT_SUCCESS;
}

granit_result renderer_registry::get_swapchain_backbuffer(granit_renderer renderer,
                                                          granit_swapchain swapchain,
                                                          std::uint32_t index,
                                                          granit_texture& texture,
                                                          granit_texture_view& view) {
  std::lock_guard lock{mutex_};
  const auto state = renderers_.find(renderer);
  const auto found = swapchains_.find(swapchain);
  if (state == renderers_.end() || found == swapchains_.end() ||
      found->second->owner != state->second) {
    return GRANIT_ERROR_INVALID_HANDLE;
  }
  if (index >= found->second->textures.size()) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  texture = found->second->textures[index];
  view = found->second->views[index];
  return GRANIT_SUCCESS;
}

granit_result renderer_registry::acquire_swapchain_frame(granit_renderer renderer,
                                                         granit_swapchain swapchain,
                                                         granit_frame& frame,
                                                         std::uint32_t& image_index,
                                                         bool& needs_recreate) {
  try {
    std::lock_guard lock{mutex_};
    const auto state = renderers_.find(renderer);
    const auto found = swapchains_.find(swapchain);
    if (state == renderers_.end() || found == swapchains_.end() ||
        found->second->owner != state->second) {
      return GRANIT_ERROR_INVALID_HANDLE;
    }
    if (!found->second->textures.empty()) {
      return GRANIT_ERROR_NOT_READY;
    }
    auto record = std::make_shared<frame_record>();
    record->owner = state->second;
    record->presentation = found->second->presentation;
    record->queue = std::dynamic_pointer_cast<backend_queue>(state->second);
    if (!record->queue)
      return GRANIT_ERROR_INTERNAL;
    record->swapchain = found->second;
    backend_acquired_swapchain_frame acquired{};
    const auto acquire_result =
        record->presentation->acquire_swapchain_frame(*found->second->native, acquired);
    if (acquire_result != GRANIT_SUCCESS) {
      return acquire_result;
    }
    auto cancel = [&]() noexcept {
      bool ignored{};
      static_cast<void>(
          record->presentation->cancel_swapchain_frame(*found->second->native,
                                                       acquired.image_index,
                                                       acquired.slot_index, ignored));
    };
    auto texture_record_state = std::make_shared<texture_record>();
    texture_record_state->owner = state->second;
    texture_record_state->native = std::move(acquired.dynamic_backbuffer.texture);
    texture_record_state->desc = acquired.dynamic_backbuffer.desc;
    texture_record_state->publicly_destroyable = false;
    auto view_record_state = std::make_shared<texture_view_record>();
    view_record_state->owner = state->second;
    view_record_state->texture = texture_record_state;
    view_record_state->native = std::move(acquired.dynamic_backbuffer.view);
    view_record_state->publicly_destroyable = false;
    const auto texture_handle = handles_.insert(texture_record_state.get(),
                                                resource_type::texture, 0);
    if (texture_handle == GRANIT_NULL_HANDLE) {
      cancel();
      return GRANIT_ERROR_OUT_OF_MEMORY;
    }
    const auto view_handle = handles_.insert(view_record_state.get(),
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
      found->second->textures.push_back(texture_handle);
      found->second->views.push_back(view_handle);
      textures_.emplace(texture_handle, texture_record_state);
      texture_views_.emplace(view_handle, view_record_state);
      frames_.emplace(frame_handle, std::move(record));
    } catch (...) {
      found->second->views.clear();
      found->second->textures.clear();
      texture_views_.erase(view_handle);
      textures_.erase(texture_handle);
      static_cast<void>(handles_.erase(frame_handle, resource_type::frame, 0));
      static_cast<void>(handles_.erase(view_handle, resource_type::texture_view, 0));
      static_cast<void>(handles_.erase(texture_handle, resource_type::texture, 0));
      cancel();
      throw;
    }
    frames_.at(frame_handle)->image_index = acquired.image_index;
    frames_.at(frame_handle)->slot_index = acquired.slot_index;
    frames_.at(frame_handle)->dynamic_backbuffer = true;
    frame = frame_handle;
    image_index = acquired.image_index;
    needs_recreate = acquired.needs_recreate;
    return GRANIT_SUCCESS;
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

void renderer_registry::erase_dynamic_backbuffer(swapchain_record& swapchain) noexcept {
  for (const auto view : swapchain.views) {
    texture_views_.erase(view);
    static_cast<void>(handles_.erase(view, resource_type::texture_view, 0));
  }
  for (const auto texture : swapchain.textures) {
    textures_.erase(texture);
    static_cast<void>(handles_.erase(texture, resource_type::texture, 0));
  }
  swapchain.views.clear();
  swapchain.textures.clear();
}

granit_result renderer_registry::finish_frame(granit_renderer renderer, granit_swapchain swapchain,
                                              granit_frame frame, bool present,
                                              bool& needs_recreate) {
  std::shared_ptr<frame_record> record;
  granit_result result{};
  {
    std::lock_guard lock{mutex_};
    const auto state = renderers_.find(renderer);
    const auto swapchain_found = swapchains_.find(swapchain);
    const auto frame_found = frames_.find(frame);
    if (state == renderers_.end() || swapchain_found == swapchains_.end() ||
        frame_found == frames_.end() || swapchain_found->second->owner != state->second ||
        frame_found->second->swapchain != swapchain_found->second) {
      return GRANIT_ERROR_INVALID_HANDLE;
    }
    result = present ? swapchain_found->second->presentation->present_swapchain_frame(
                           *swapchain_found->second->native, frame_found->second->image_index,
                           frame_found->second->slot_index, needs_recreate)
                     : swapchain_found->second->presentation->cancel_swapchain_frame(
                           *swapchain_found->second->native, frame_found->second->image_index,
                           frame_found->second->slot_index, needs_recreate);
    if (result != GRANIT_SUCCESS) {
      return result;
    }
    erase_dynamic_backbuffer(*swapchain_found->second);
    record = std::move(frame_found->second);
    frames_.erase(frame_found);
    static_cast<void>(handles_.erase(frame, resource_type::frame, 0));
  }
  record.reset();
  return GRANIT_SUCCESS;
}

granit_result renderer_registry::present_swapchain_frame(granit_renderer renderer,
                                                         granit_swapchain swapchain,
                                                         granit_frame frame, bool& needs_recreate) {
  return finish_frame(renderer, swapchain, frame, true, needs_recreate);
}

granit_result renderer_registry::cancel_swapchain_frame(granit_renderer renderer,
                                                        granit_swapchain swapchain,
                                                        granit_frame frame, bool& needs_recreate) {
  return finish_frame(renderer, swapchain, frame, false, needs_recreate);
}

granit_result renderer_registry::get_frame_info(granit_renderer renderer,
                                                granit_swapchain swapchain, granit_frame frame,
                                                std::uint32_t& frame_slot,
                                                std::uint32_t& frame_slot_count) {
  std::lock_guard lock{mutex_};
  const auto state = renderers_.find(renderer);
  const auto swapchain_found = swapchains_.find(swapchain);
  const auto frame_found = frames_.find(frame);
  if (state == renderers_.end() || swapchain_found == swapchains_.end() ||
      frame_found == frames_.end() || swapchain_found->second->owner != state->second ||
      frame_found->second->swapchain != swapchain_found->second) {
    return GRANIT_ERROR_INVALID_HANDLE;
  }
  const auto info =
      swapchain_found->second->presentation->get_swapchain_info(*swapchain_found->second->native);
  frame_slot = static_cast<std::uint32_t>(frame_found->second->slot_index);
  frame_slot_count = info.image_count;
  return GRANIT_SUCCESS;
}

granit_result renderer_registry::destroy_swapchain(granit_renderer renderer,
                                                   granit_swapchain swapchain) {
  std::shared_ptr<swapchain_record> record;
  {
    std::lock_guard lock{mutex_};
    const auto state = renderers_.find(renderer);
    const auto found = swapchains_.find(swapchain);
    if (state == renderers_.end() || found == swapchains_.end() ||
        found->second->owner != state->second) {
      return GRANIT_ERROR_INVALID_HANDLE;
    }
    if (!found->second->textures.empty()) {
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

granit_result renderer_registry::create_wgsl_shader(granit_renderer renderer,
                                                    granit_shader_stage stage,
                                                    std::string_view source,
                                                    std::string_view entry_point,
                                                    granit_shader& shader) {
  try {
    const auto state = acquire(renderer);
    if (!state || state->shaders() == nullptr) {
      return state ? GRANIT_ERROR_NOT_READY : GRANIT_ERROR_INVALID_HANDLE;
    }
    auto record = std::make_shared<shader_record>();
    record->owner = state;
    record->stage = stage;
    record->entry_point.assign(entry_point);
    record->native = state->shaders()->allocate_shader();
    if (!record->native) {
      return GRANIT_ERROR_OUT_OF_MEMORY;
    }
    const auto result = state->create_wgsl_shader(*record->native, stage, source, entry_point);
    if (result != GRANIT_SUCCESS) {
      return result;
    }
    std::lock_guard lock{mutex_};
    if (renderers_.find(renderer) == renderers_.end()) {
      return GRANIT_ERROR_INVALID_HANDLE;
    }
    const auto handle = handles_.insert(record.get(), resource_type::shader, 0);
    if (handle == GRANIT_NULL_HANDLE) {
      return GRANIT_ERROR_OUT_OF_MEMORY;
    }
    try {
      shaders_.emplace(handle, std::move(record));
    } catch (...) {
      static_cast<void>(handles_.erase(handle, resource_type::shader, 0));
      throw;
    }
    shader = handle;
    return GRANIT_SUCCESS;
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result renderer_registry::destroy_shader(granit_renderer renderer, granit_shader shader) {
  std::shared_ptr<shader_record> record;
  {
    std::lock_guard lock{mutex_};
    const auto state = renderers_.find(renderer);
    const auto found = shaders_.find(shader);
    if (state == renderers_.end() || found == shaders_.end() ||
        found->second->owner != state->second) {
      return GRANIT_ERROR_INVALID_HANDLE;
    }
    for (const auto& [handle, pipeline] : graphics_pipelines_) {
      static_cast<void>(handle);
      if (pipeline->vertex_shader == found->second || pipeline->fragment_shader == found->second) {
        return GRANIT_ERROR_INVALID_ARGUMENT;
      }
    }
    record = std::move(found->second);
    shaders_.erase(found);
    const auto result = handles_.erase(shader, resource_type::shader, 0);
    if (result != GRANIT_SUCCESS) {
      return result;
    }
  }
  record.reset();
  return GRANIT_SUCCESS;
}

granit_result renderer_registry::create_webgpu_pipeline_layout(granit_renderer renderer,
                                                               granit_pipeline_layout& layout) {
  try {
    const auto state = acquire(renderer);
    if (!state || state->pipelines() == nullptr) {
      return state ? GRANIT_ERROR_NOT_READY : GRANIT_ERROR_INVALID_HANDLE;
    }
    auto record = std::make_shared<pipeline_layout_record>();
    record->owner = state;
    record->native = state->pipelines()->allocate_pipeline_layout();
    if (!record->native) {
      return GRANIT_ERROR_OUT_OF_MEMORY;
    }
    const auto result = state->pipelines()->create_pipeline_layout(*record->native);
    if (result != GRANIT_SUCCESS) {
      return result;
    }
    std::lock_guard lock{mutex_};
    if (renderers_.find(renderer) == renderers_.end()) {
      return GRANIT_ERROR_INVALID_HANDLE;
    }
    const auto handle = handles_.insert(record.get(), resource_type::pipeline_layout, 0);
    if (handle == GRANIT_NULL_HANDLE) {
      return GRANIT_ERROR_OUT_OF_MEMORY;
    }
    try {
      pipeline_layouts_.emplace(handle, std::move(record));
    } catch (...) {
      static_cast<void>(handles_.erase(handle, resource_type::pipeline_layout, 0));
      throw;
    }
    layout = handle;
    return GRANIT_SUCCESS;
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result renderer_registry::destroy_pipeline_layout(granit_renderer renderer,
                                                         granit_pipeline_layout layout) {
  std::shared_ptr<pipeline_layout_record> record;
  {
    std::lock_guard lock{mutex_};
    const auto state = renderers_.find(renderer);
    const auto found = pipeline_layouts_.find(layout);
    if (state == renderers_.end() || found == pipeline_layouts_.end() ||
        found->second->owner != state->second) {
      return GRANIT_ERROR_INVALID_HANDLE;
    }
    for (const auto& [handle, pipeline] : graphics_pipelines_) {
      static_cast<void>(handle);
      if (pipeline->layout == found->second) {
        return GRANIT_ERROR_INVALID_ARGUMENT;
      }
    }
    record = std::move(found->second);
    pipeline_layouts_.erase(found);
    const auto result = handles_.erase(layout, resource_type::pipeline_layout, 0);
    if (result != GRANIT_SUCCESS) {
      return result;
    }
  }
  record.reset();
  return GRANIT_SUCCESS;
}

granit_result renderer_registry::create_webgpu_graphics_pipeline(
    granit_renderer renderer, granit_pipeline_layout layout, granit_shader vertex_shader,
    granit_shader fragment_shader, granit_texture_format color_format,
    granit_graphics_pipeline& pipeline) {
  try {
    const auto state = acquire(renderer);
    if (!state || state->pipelines() == nullptr || state->shaders() == nullptr) {
      return state ? GRANIT_ERROR_NOT_READY : GRANIT_ERROR_INVALID_HANDLE;
    }
    std::shared_ptr<pipeline_layout_record> layout_record;
    std::shared_ptr<shader_record> vertex_record;
    std::shared_ptr<shader_record> fragment_record;
    {
      std::lock_guard lock{mutex_};
      const auto layout_found = pipeline_layouts_.find(layout);
      const auto vertex_found = shaders_.find(vertex_shader);
      const auto fragment_found = shaders_.find(fragment_shader);
      if (layout_found == pipeline_layouts_.end() || vertex_found == shaders_.end() ||
          fragment_found == shaders_.end() || layout_found->second->owner != state ||
          vertex_found->second->owner != state || fragment_found->second->owner != state) {
        return GRANIT_ERROR_INVALID_HANDLE;
      }
      layout_record = layout_found->second;
      vertex_record = vertex_found->second;
      fragment_record = fragment_found->second;
    }
    auto record = std::make_shared<graphics_pipeline_record>();
    record->owner = state;
    record->layout = layout_record;
    record->vertex_shader = vertex_record;
    record->fragment_shader = fragment_record;
    record->native = state->pipelines()->allocate_graphics_pipeline();
    if (!record->native) {
      return GRANIT_ERROR_OUT_OF_MEMORY;
    }
    const auto result = state->pipelines()->create_graphics_pipeline(
        *record->native, *layout_record->native,
        state->shaders()->native_handle(*vertex_record->native),
        state->shaders()->native_handle(*fragment_record->native), color_format);
    if (result != GRANIT_SUCCESS) {
      return result;
    }
    std::lock_guard lock{mutex_};
    if (pipeline_layouts_.find(layout) == pipeline_layouts_.end() ||
        shaders_.find(vertex_shader) == shaders_.end() ||
        shaders_.find(fragment_shader) == shaders_.end()) {
      return GRANIT_ERROR_INVALID_HANDLE;
    }
    const auto handle = handles_.insert(record.get(), resource_type::pipeline, 0);
    if (handle == GRANIT_NULL_HANDLE) {
      return GRANIT_ERROR_OUT_OF_MEMORY;
    }
    try {
      graphics_pipelines_.emplace(handle, std::move(record));
    } catch (...) {
      static_cast<void>(handles_.erase(handle, resource_type::pipeline, 0));
      throw;
    }
    pipeline = handle;
    return GRANIT_SUCCESS;
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result renderer_registry::destroy_graphics_pipeline(granit_renderer renderer,
                                                           granit_graphics_pipeline pipeline) {
  std::shared_ptr<graphics_pipeline_record> record;
  {
    std::lock_guard lock{mutex_};
    const auto state = renderers_.find(renderer);
    const auto found = graphics_pipelines_.find(pipeline);
    if (state == renderers_.end() || found == graphics_pipelines_.end() ||
        found->second->owner != state->second) {
      return GRANIT_ERROR_INVALID_HANDLE;
    }
    record = std::move(found->second);
    graphics_pipelines_.erase(found);
    const auto result = handles_.erase(pipeline, resource_type::pipeline, 0);
    if (result != GRANIT_SUCCESS) {
      return result;
    }
  }
  record.reset();
  return GRANIT_SUCCESS;
}

std::shared_ptr<webgpu_renderer_state> renderer_registry::acquire(granit_renderer renderer) {
  std::lock_guard lock{mutex_};
  if (handles_.find(renderer, resource_type::renderer, 0) == nullptr) {
    return {};
  }
  const auto found = renderers_.find(renderer);
  return found == renderers_.end()
             ? std::shared_ptr<webgpu_renderer_state>{}
             : std::dynamic_pointer_cast<webgpu_renderer_state>(found->second);
}

std::shared_ptr<backend_renderer> renderer_registry::acquire_backend(granit_renderer renderer) {
  std::lock_guard lock{mutex_};
  if (handles_.find(renderer, resource_type::renderer, 0) == nullptr)
    return {};
  const auto found = backend_renderers_.find(renderer);
  return found == backend_renderers_.end() ? std::shared_ptr<backend_renderer>{} : found->second;
}

granit_result renderer_registry::create_command_recorder(granit_renderer renderer,
                                                         granit_command_recorder& recorder) {
  try {
    const auto state = acquire(renderer);
    if (!state)
      return GRANIT_ERROR_INVALID_HANDLE;
    auto record = std::make_shared<command_recorder_record>();
    record->owner = state;
    record->queue = state;
    record->commands = state;
    record->native = state->allocate_command_recorder_resource();
    if (!record->native)
      return GRANIT_ERROR_OUT_OF_MEMORY;
    const auto create_result = state->create_command_recorder(*record->native);
    if (create_result != GRANIT_SUCCESS)
      return create_result;
    std::lock_guard lock{mutex_};
    if (renderers_.find(renderer) == renderers_.end())
      return GRANIT_ERROR_INVALID_HANDLE;
    const auto handle = handles_.insert(record.get(), resource_type::command_recorder, 0);
    if (handle == GRANIT_NULL_HANDLE)
      return GRANIT_ERROR_OUT_OF_MEMORY;
    try {
      command_recorders_.emplace(handle, std::move(record));
    } catch (...) {
      static_cast<void>(handles_.erase(handle, resource_type::command_recorder, 0));
      throw;
    }
    recorder = handle;
    return GRANIT_SUCCESS;
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result renderer_registry::begin_command_recorder(granit_renderer renderer,
                                                        granit_command_recorder recorder) {
  std::lock_guard lock{mutex_};
  const auto state = renderers_.find(renderer);
  const auto found = command_recorders_.find(recorder);
  if (state == renderers_.end() || found == command_recorders_.end() ||
      found->second->owner != state->second)
    return GRANIT_ERROR_INVALID_HANDLE;
  if (found->second->web_status != command_recorder_record::web_state::initial)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const auto web_state = std::static_pointer_cast<webgpu_renderer_state>(state->second);
  const auto result = web_state->begin_command_recorder(*found->second->native);
  if (result == GRANIT_SUCCESS)
    found->second->web_status = command_recorder_record::web_state::recording;
  return result;
}

granit_result renderer_registry::begin_rendering(granit_renderer renderer,
                                                 granit_command_recorder recorder,
                                                 const granit_rendering_desc& desc) {
  std::lock_guard lock{mutex_};
  const auto state = renderers_.find(renderer);
  const auto command = command_recorders_.find(recorder);
  if (state == renderers_.end() || command == command_recorders_.end() ||
      command->second->owner != state->second)
    return GRANIT_ERROR_INVALID_HANDLE;
  if (command->second->web_status != command_recorder_record::web_state::recording)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const auto view = desc.color_attachments[0].view;
  const auto target = texture_views_.find(view);
  if (target == texture_views_.end() || target->second->owner != state->second)
    return GRANIT_ERROR_INVALID_HANDLE;
  command->second->web_target = target->second;
  command->second->web_status = command_recorder_record::web_state::rendering;
  return GRANIT_SUCCESS;
}

granit_result renderer_registry::bind_graphics_pipeline(granit_renderer renderer,
                                                        granit_command_recorder recorder,
                                                        granit_graphics_pipeline pipeline) {
  std::lock_guard lock{mutex_};
  const auto state = renderers_.find(renderer);
  const auto command = command_recorders_.find(recorder);
  const auto found = graphics_pipelines_.find(pipeline);
  if (state == renderers_.end() || command == command_recorders_.end() ||
      found == graphics_pipelines_.end() || command->second->owner != state->second ||
      found->second->owner != state->second)
    return GRANIT_ERROR_INVALID_HANDLE;
  if (command->second->web_status != command_recorder_record::web_state::rendering)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  command->second->web_pipeline = found->second;
  return GRANIT_SUCCESS;
}

granit_result renderer_registry::draw(granit_renderer renderer, granit_command_recorder recorder,
                                      std::uint32_t vertex_count, std::uint32_t instance_count,
                                      std::uint32_t first_vertex, std::uint32_t first_instance) {
  std::lock_guard lock{mutex_};
  const auto state = renderers_.find(renderer);
  const auto command = command_recorders_.find(recorder);
  if (state == renderers_.end() || command == command_recorders_.end() ||
      command->second->owner != state->second)
    return GRANIT_ERROR_INVALID_HANDLE;
  auto& record = *command->second;
  if (record.web_status != command_recorder_record::web_state::rendering || !record.web_target ||
      !record.web_pipeline || record.web_drew)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const auto web_state = std::static_pointer_cast<webgpu_renderer_state>(state->second);
  const auto result =
      web_state->draw(*record.native, record.web_target->native.get(),
                      record.web_pipeline->native.get(), vertex_count, instance_count,
                      first_vertex, first_instance);
  if (result == GRANIT_SUCCESS)
    record.web_drew = true;
  return result;
}

granit_result renderer_registry::end_rendering(granit_renderer renderer,
                                               granit_command_recorder recorder) {
  std::lock_guard lock{mutex_};
  const auto state = renderers_.find(renderer);
  const auto found = command_recorders_.find(recorder);
  if (state == renderers_.end() || found == command_recorders_.end() ||
      found->second->owner != state->second)
    return GRANIT_ERROR_INVALID_HANDLE;
  if (found->second->web_status != command_recorder_record::web_state::rendering ||
      !found->second->web_drew)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  found->second->web_status = command_recorder_record::web_state::recording;
  return GRANIT_SUCCESS;
}

granit_result renderer_registry::end_command_recorder(granit_renderer renderer,
                                                      granit_command_recorder recorder) {
  std::lock_guard lock{mutex_};
  const auto state = renderers_.find(renderer);
  const auto found = command_recorders_.find(recorder);
  if (state == renderers_.end() || found == command_recorders_.end() ||
      found->second->owner != state->second)
    return GRANIT_ERROR_INVALID_HANDLE;
  if (found->second->web_status != command_recorder_record::web_state::recording ||
      !found->second->web_drew)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const auto web_state = std::static_pointer_cast<webgpu_renderer_state>(state->second);
  const auto result = web_state->end_command_recorder(*found->second->native);
  if (result == GRANIT_SUCCESS)
    found->second->web_status = command_recorder_record::web_state::executable;
  return result;
}

granit_result renderer_registry::submit_command_recorder(granit_renderer renderer,
                                                         granit_command_recorder recorder) {
  return submit_command_recorder_frame(renderer, recorder, GRANIT_NULL_HANDLE);
}

granit_result renderer_registry::submit_command_recorder_frame(granit_renderer renderer,
                                                               granit_command_recorder recorder,
                                                               granit_frame frame) {
  std::lock_guard lock{mutex_};
  const auto state = renderers_.find(renderer);
  const auto command = command_recorders_.find(recorder);
  if (state == renderers_.end() || command == command_recorders_.end() ||
      command->second->owner != state->second)
    return GRANIT_ERROR_INVALID_HANDLE;
  if (command->second->web_status != command_recorder_record::web_state::executable)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  if (frame != GRANIT_NULL_HANDLE) {
    const auto found = frames_.find(frame);
    if (found == frames_.end() || !command->second->web_target)
      return GRANIT_ERROR_INVALID_HANDLE;
    const auto target_belongs_to_frame =
        std::any_of(found->second->swapchain->views.begin(), found->second->swapchain->views.end(),
                    [&](const auto handle) {
                      const auto view = texture_views_.find(handle);
                      return view != texture_views_.end() &&
                             view->second == command->second->web_target;
                    });
    if (!target_belongs_to_frame)
      return GRANIT_ERROR_INVALID_HANDLE;
  }
  submission_serial serial{};
  const auto web_state = std::static_pointer_cast<webgpu_renderer_state>(state->second);
  const auto result = web_state->submit_command_recorder(*command->second->native, serial);
  if (result == GRANIT_SUCCESS)
    command->second->web_status = command_recorder_record::web_state::submitted;
  return result;
}

granit_result renderer_registry::reset_command_recorder(granit_renderer renderer,
                                                        granit_command_recorder recorder) {
  std::lock_guard lock{mutex_};
  const auto state = renderers_.find(renderer);
  const auto found = command_recorders_.find(recorder);
  if (state == renderers_.end() || found == command_recorders_.end() ||
      found->second->owner != state->second)
    return GRANIT_ERROR_INVALID_HANDLE;
  if (found->second->web_status == command_recorder_record::web_state::recording ||
      found->second->web_status == command_recorder_record::web_state::rendering)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const auto web_state = std::static_pointer_cast<webgpu_renderer_state>(state->second);
  const auto result = web_state->reset_command_recorder(*found->second->native);
  if (result == GRANIT_SUCCESS) {
    found->second->web_target.reset();
    found->second->web_pipeline.reset();
    found->second->web_status = command_recorder_record::web_state::initial;
    found->second->web_drew = false;
  }
  return result;
}

granit_result
renderer_registry::submit_command_recorders(granit_renderer,
                                            std::span<const granit_command_recorder>) {
  return GRANIT_ERROR_UNSUPPORTED;
}

granit_result renderer_registry::copy_buffer(granit_renderer, granit_command_recorder,
                                             granit_buffer, granit_buffer,
                                             std::span<const granit_buffer_copy_region>) {
  return GRANIT_ERROR_UNSUPPORTED;
}

granit_result renderer_registry::copy_texture_to_buffer(granit_renderer, granit_command_recorder,
                                                        granit_texture, granit_buffer,
                                                        const granit_texture_data_layout&,
                                                        const granit_texture_write_region&) {
  return GRANIT_ERROR_UNSUPPORTED;
}

granit_result renderer_registry::copy_buffer_to_texture(granit_renderer, granit_command_recorder,
                                                        granit_buffer, granit_texture,
                                                        const granit_texture_data_layout&,
                                                        const granit_texture_write_region&) {
  return GRANIT_ERROR_UNSUPPORTED;
}

granit_result renderer_registry::copy_texture(granit_renderer, granit_command_recorder,
                                              granit_texture, granit_texture,
                                              const granit_texture_copy_region&) {
  return GRANIT_ERROR_UNSUPPORTED;
}

granit_result renderer_registry::generate_mipmaps(granit_renderer, granit_command_recorder,
                                                  granit_texture,
                                                  const granit_texture_mipmap_range&) {
  return GRANIT_ERROR_UNSUPPORTED;
}

granit_result renderer_registry::fill_buffer(granit_renderer, granit_command_recorder,
                                             granit_buffer, std::uint64_t, std::uint64_t,
                                             std::uint32_t) {
  return GRANIT_ERROR_UNSUPPORTED;
}

granit_result renderer_registry::bind_graphics_groups(granit_renderer, granit_command_recorder,
                                                      granit_pipeline_layout, std::uint32_t,
                                                      std::span<const granit_bind_group>,
                                                      std::span<const std::uint32_t>) {
  return GRANIT_ERROR_UNSUPPORTED;
}

granit_result renderer_registry::bind_compute_pipeline(granit_renderer, granit_command_recorder,
                                                       granit_compute_pipeline) {
  return GRANIT_ERROR_UNSUPPORTED;
}

granit_result renderer_registry::bind_compute_groups(granit_renderer, granit_command_recorder,
                                                     granit_pipeline_layout, std::uint32_t,
                                                     std::span<const granit_bind_group>,
                                                     std::span<const std::uint32_t>) {
  return GRANIT_ERROR_UNSUPPORTED;
}

granit_result renderer_registry::dispatch(granit_renderer, granit_command_recorder, std::uint32_t,
                                          std::uint32_t, std::uint32_t) {
  return GRANIT_ERROR_UNSUPPORTED;
}

granit_result renderer_registry::set_viewports(granit_renderer, granit_command_recorder,
                                               std::uint32_t, std::span<const granit_viewport>) {
  return GRANIT_ERROR_UNSUPPORTED;
}

granit_result renderer_registry::set_scissors(granit_renderer, granit_command_recorder,
                                              std::uint32_t, std::span<const granit_scissor>) {
  return GRANIT_ERROR_UNSUPPORTED;
}

granit_result
renderer_registry::bind_vertex_buffers(granit_renderer, granit_command_recorder, std::uint32_t,
                                       std::span<const granit_vertex_buffer_binding>) {
  return GRANIT_ERROR_UNSUPPORTED;
}

granit_result renderer_registry::bind_index_buffer(granit_renderer, granit_command_recorder,
                                                   granit_buffer, std::uint64_t,
                                                   granit_index_type) {
  return GRANIT_ERROR_UNSUPPORTED;
}

granit_result renderer_registry::draw_indexed(granit_renderer, granit_command_recorder,
                                              std::uint32_t, std::uint32_t, std::uint32_t,
                                              std::int32_t, std::uint32_t) {
  return GRANIT_ERROR_UNSUPPORTED;
}

granit_result renderer_registry::destroy_command_recorder(granit_renderer renderer,
                                                          granit_command_recorder recorder) {
  std::shared_ptr<command_recorder_record> record;
  {
    std::lock_guard lock{mutex_};
    const auto state = renderers_.find(renderer);
    const auto found = command_recorders_.find(recorder);
    if (state == renderers_.end() || found == command_recorders_.end() ||
        found->second->owner != state->second)
      return GRANIT_ERROR_INVALID_HANDLE;
    record = std::move(found->second);
    command_recorders_.erase(found);
    const auto result = handles_.erase(recorder, resource_type::command_recorder, 0);
    if (result != GRANIT_SUCCESS)
      return result;
  }
  record.reset();
  return GRANIT_SUCCESS;
}

} // namespace granit::detail
