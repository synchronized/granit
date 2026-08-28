// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "renderer/renderer_registry.h"

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

granit_result renderer_registry::create(granit_diagnostic_callback diagnostic_callback,
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
      if (command->second->renderer == found->second) {
        static_cast<void>(handles_.erase(command->first, resource_type::command_recorder, 0));
        command_recorders.push_back(std::move(command->second));
        command = command_recorders_.erase(command);
      } else {
        ++command;
      }
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
    for (auto pipeline = graphics_pipelines_.begin(); pipeline != graphics_pipelines_.end();) {
      if (pipeline->second->renderer == found->second) {
        static_cast<void>(handles_.erase(pipeline->first, resource_type::pipeline, 0));
        graphics_pipelines.push_back(std::move(pipeline->second));
        pipeline = graphics_pipelines_.erase(pipeline);
      } else {
        ++pipeline;
      }
    }
    for (auto layout = pipeline_layouts_.begin(); layout != pipeline_layouts_.end();) {
      if (layout->second->renderer == found->second) {
        static_cast<void>(handles_.erase(layout->first, resource_type::pipeline_layout, 0));
        pipeline_layouts.push_back(std::move(layout->second));
        layout = pipeline_layouts_.erase(layout);
      } else {
        ++layout;
      }
    }
    for (auto shader = shaders_.begin(); shader != shaders_.end();) {
      if (shader->second->renderer == found->second) {
        static_cast<void>(handles_.erase(shader->first, resource_type::shader, 0));
        shaders.push_back(std::move(shader->second));
        shader = shaders_.erase(shader);
      } else {
        ++shader;
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

granit_result renderer_registry::create_canvas_surface(granit_renderer renderer,
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

granit_result renderer_registry::destroy_surface(granit_renderer renderer, granit_surface surface) {
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

granit_result renderer_registry::recreate_swapchain(granit_renderer renderer,
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

granit_result renderer_registry::get_swapchain_info(granit_renderer renderer,
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

granit_result renderer_registry::get_swapchain_backbuffer(granit_renderer renderer,
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

granit_result renderer_registry::acquire_swapchain(granit_renderer renderer,
                                                   granit_swapchain swapchain, granit_frame& frame,
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

void renderer_registry::erase_backbuffer(swapchain_record& swapchain) noexcept {
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

granit_result renderer_registry::get_frame_info(granit_renderer renderer,
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

granit_result renderer_registry::destroy_swapchain(granit_renderer renderer,
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

granit_result renderer_registry::create_shader(granit_renderer renderer, std::uint32_t stage,
                                               const char* wgsl, std::uint64_t wgsl_length,
                                               const char* entry_point,
                                               std::uint64_t entry_point_length,
                                               granit_shader& shader) {
  try {
    const auto state = acquire(renderer);
    if (!state || state->shaders() == nullptr) {
      return state ? GRANIT_ERROR_NOT_READY : GRANIT_ERROR_INVALID_HANDLE;
    }
    auto record = std::make_shared<shader_record>();
    record->renderer = state;
    record->native = state->shaders()->allocate_shader();
    if (!record->native) {
      return GRANIT_ERROR_OUT_OF_MEMORY;
    }
    const auto result = state->shaders()->create_shader(*record->native, stage, wgsl, wgsl_length,
                                                        entry_point, entry_point_length);
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
        found->second->renderer != state->second) {
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

granit_result renderer_registry::create_pipeline_layout(granit_renderer renderer,
                                                        granit_pipeline_layout& layout) {
  try {
    const auto state = acquire(renderer);
    if (!state || state->pipelines() == nullptr) {
      return state ? GRANIT_ERROR_NOT_READY : GRANIT_ERROR_INVALID_HANDLE;
    }
    auto record = std::make_shared<pipeline_layout_record>();
    record->renderer = state;
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
        found->second->renderer != state->second) {
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

granit_result renderer_registry::create_graphics_pipeline(granit_renderer renderer,
                                                          granit_pipeline_layout layout,
                                                          granit_shader vertex_shader,
                                                          granit_shader fragment_shader,
                                                          granit_texture_format color_format,
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
          fragment_found == shaders_.end() || layout_found->second->renderer != state ||
          vertex_found->second->renderer != state || fragment_found->second->renderer != state) {
        return GRANIT_ERROR_INVALID_HANDLE;
      }
      layout_record = layout_found->second;
      vertex_record = vertex_found->second;
      fragment_record = fragment_found->second;
    }
    auto record = std::make_shared<graphics_pipeline_record>();
    record->renderer = state;
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
        found->second->renderer != state->second) {
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
  return found == renderers_.end() ? std::shared_ptr<webgpu_renderer_state>{} : found->second;
}

granit_result renderer_registry::create_command_recorder(granit_renderer renderer,
                                                         granit_command_recorder& recorder) {
  try {
    const auto state = acquire(renderer);
    if (!state)
      return GRANIT_ERROR_INVALID_HANDLE;
    auto record = std::make_shared<command_recorder_record>();
    record->renderer = state;
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
      found->second->renderer != state->second)
    return GRANIT_ERROR_INVALID_HANDLE;
  if (found->second->state != command_state::initial)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const auto result = state->second->begin_command_recorder(*found->second->native);
  if (result == GRANIT_SUCCESS)
    found->second->state = command_state::recording;
  return result;
}

granit_result renderer_registry::begin_rendering(granit_renderer renderer,
                                                 granit_command_recorder recorder,
                                                 granit_texture_view view) {
  std::lock_guard lock{mutex_};
  const auto state = renderers_.find(renderer);
  const auto command = command_recorders_.find(recorder);
  if (state == renderers_.end() || command == command_recorders_.end() ||
      command->second->renderer != state->second)
    return GRANIT_ERROR_INVALID_HANDLE;
  if (command->second->state != command_state::recording)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const auto frame = std::find_if(frames_.begin(), frames_.end(), [&](const auto& entry) {
    return entry.second->swapchain->renderer == state->second &&
           entry.second->swapchain->view == view;
  });
  if (frame == frames_.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  command->second->frame = frame->second;
  command->second->state = command_state::rendering;
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
      found == graphics_pipelines_.end() || command->second->renderer != state->second ||
      found->second->renderer != state->second)
    return GRANIT_ERROR_INVALID_HANDLE;
  if (command->second->state != command_state::rendering)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  command->second->pipeline = found->second;
  return GRANIT_SUCCESS;
}

granit_result renderer_registry::draw(granit_renderer renderer, granit_command_recorder recorder) {
  std::lock_guard lock{mutex_};
  const auto state = renderers_.find(renderer);
  const auto command = command_recorders_.find(recorder);
  if (state == renderers_.end() || command == command_recorders_.end() ||
      command->second->renderer != state->second)
    return GRANIT_ERROR_INVALID_HANDLE;
  auto& record = *command->second;
  if (record.state != command_state::rendering || !record.frame || !record.pipeline || record.drew)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const auto target =
      state->second->presentation()->native_view(*record.frame->acquired.dynamic_backbuffer.view);
  const auto pipeline = state->second->pipelines()->native_handle(*record.pipeline->native);
  const auto result = state->second->commands()->draw(*record.native, target, pipeline);
  if (result == GRANIT_SUCCESS)
    record.drew = true;
  return result;
}

granit_result renderer_registry::end_rendering(granit_renderer renderer,
                                               granit_command_recorder recorder) {
  std::lock_guard lock{mutex_};
  const auto state = renderers_.find(renderer);
  const auto found = command_recorders_.find(recorder);
  if (state == renderers_.end() || found == command_recorders_.end() ||
      found->second->renderer != state->second)
    return GRANIT_ERROR_INVALID_HANDLE;
  if (found->second->state != command_state::rendering || !found->second->drew)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  found->second->state = command_state::recording;
  return GRANIT_SUCCESS;
}

granit_result renderer_registry::end_command_recorder(granit_renderer renderer,
                                                      granit_command_recorder recorder) {
  std::lock_guard lock{mutex_};
  const auto state = renderers_.find(renderer);
  const auto found = command_recorders_.find(recorder);
  if (state == renderers_.end() || found == command_recorders_.end() ||
      found->second->renderer != state->second)
    return GRANIT_ERROR_INVALID_HANDLE;
  if (found->second->state != command_state::recording || !found->second->drew)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const auto result = state->second->end_command_recorder(*found->second->native);
  if (result == GRANIT_SUCCESS)
    found->second->state = command_state::executable;
  return result;
}

granit_result renderer_registry::submit_command_recorder(granit_renderer renderer,
                                                         granit_command_recorder recorder,
                                                         granit_frame frame) {
  std::lock_guard lock{mutex_};
  const auto state = renderers_.find(renderer);
  const auto command = command_recorders_.find(recorder);
  if (state == renderers_.end() || command == command_recorders_.end() ||
      command->second->renderer != state->second)
    return GRANIT_ERROR_INVALID_HANDLE;
  if (command->second->state != command_state::executable)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  if (frame != GRANIT_NULL_HANDLE) {
    const auto found = frames_.find(frame);
    if (found == frames_.end() || found->second != command->second->frame)
      return GRANIT_ERROR_INVALID_HANDLE;
  }
  submission_serial serial{};
  const auto result = state->second->submit_command_recorder(*command->second->native, serial);
  if (result == GRANIT_SUCCESS)
    command->second->state = command_state::submitted;
  return result;
}

granit_result renderer_registry::reset_command_recorder(granit_renderer renderer,
                                                        granit_command_recorder recorder) {
  std::lock_guard lock{mutex_};
  const auto state = renderers_.find(renderer);
  const auto found = command_recorders_.find(recorder);
  if (state == renderers_.end() || found == command_recorders_.end() ||
      found->second->renderer != state->second)
    return GRANIT_ERROR_INVALID_HANDLE;
  if (found->second->state == command_state::recording ||
      found->second->state == command_state::rendering)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const auto result = state->second->reset_command_recorder(*found->second->native);
  if (result == GRANIT_SUCCESS) {
    found->second->frame.reset();
    found->second->pipeline.reset();
    found->second->state = command_state::initial;
    found->second->drew = false;
  }
  return result;
}

granit_result renderer_registry::destroy_command_recorder(granit_renderer renderer,
                                                          granit_command_recorder recorder) {
  std::shared_ptr<command_recorder_record> record;
  {
    std::lock_guard lock{mutex_};
    const auto state = renderers_.find(renderer);
    const auto found = command_recorders_.find(recorder);
    if (state == renderers_.end() || found == command_recorders_.end() ||
        found->second->renderer != state->second)
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
