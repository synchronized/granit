// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "renderer/renderer_registry.h"
#include "renderer/renderer_registry_records.h"

#include "backend/diagnostics.h"
#include "backend/webgpu/renderer_state.h"
#include "core/texture_format.h"
#include "renderer/renderer_registry_helpers.h"
#include "renderer/renderer_state.h"

#include <algorithm>
#include <cstring>
#include <functional>
#include <limits>
#include <new>
#include <utility>
#include <vector>

namespace granit::detail {

renderer_registry& renderer_registry::instance() {
  static renderer_registry registry;
  return registry;
}

granit_result renderer_registry::create(std::string_view application_name, bool enable_validation,
                                        std::uint32_t surface_types, std::uint32_t frames_in_flight,
                                        granit_diagnostic_callback diagnostic_callback,
                                        void* diagnostic_user_data, granit_renderer& renderer) {
  try {
    auto state = std::make_shared<renderer_state>();
    const auto initialize_result =
        state->initialize(application_name, enable_validation, surface_types, frames_in_flight,
                          diagnostic_callback, diagnostic_user_data);
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
      backend_renderers_.emplace(handle, state);
    } catch (...) {
      backend_renderers_.erase(handle);
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

granit_result
renderer_registry::create_webgpu_static(const granit_backend_plugin_api* api,
                                        granit_diagnostic_callback diagnostic_callback,
                                        void* diagnostic_user_data, granit_renderer& renderer) {
  try {
    auto state = std::make_shared<webgpu_renderer_state>();
    const auto initialize_result =
        state->initialize_static(api, diagnostic_callback, diagnostic_user_data);
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
      backend_renderers_.emplace(handle, std::move(state));
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

granit_result renderer_registry::get_limits(granit_renderer renderer,
                                            granit_renderer_limits& limits) {
  const auto state = acquire_backend(renderer);
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
  const auto state = acquire_backend(renderer);
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
  const auto state = acquire_backend(renderer);
  return state ? state->process_backend_events() : GRANIT_ERROR_INVALID_HANDLE;
}

granit_result renderer_registry::import_pipeline_cache(granit_renderer renderer, const void* data,
                                                       std::uint64_t size) {
  const auto owner = acquire_backend(renderer);
  if (!owner)
    return GRANIT_ERROR_INVALID_HANDLE;
  const auto cache = std::dynamic_pointer_cast<backend_pipeline_cache_renderer>(owner);
  return cache ? cache->import_pipeline_cache(data, size) : GRANIT_ERROR_UNSUPPORTED;
}

granit_result renderer_registry::export_pipeline_cache(granit_renderer renderer, void* data,
                                                       std::uint64_t& size) {
  const auto owner = acquire_backend(renderer);
  if (!owner)
    return GRANIT_ERROR_INVALID_HANDLE;
  const auto cache = std::dynamic_pointer_cast<backend_pipeline_cache_renderer>(owner);
  return cache ? cache->export_pipeline_cache(data, size) : GRANIT_ERROR_UNSUPPORTED;
}

granit_result renderer_registry::set_object_name(granit_renderer renderer, granit_handle object,
                                                 std::string_view name) {
  std::unique_lock lock{mutex_};
  const auto renderer_it = backend_renderers_.find(renderer);
  if (renderer_it == backend_renderers_.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  const auto owner = renderer_it->second;
  const auto diagnostics = std::dynamic_pointer_cast<backend_diagnostic_renderer>(owner);
  if (!diagnostics)
    return GRANIT_ERROR_UNSUPPORTED;

#define GRANIT_NAME_OBJECT(map)                                                                    \
  if (const auto it = map.find(object); it != map.end()) {                                         \
    if (it->second->renderer != owner)                                                             \
      return GRANIT_ERROR_INVALID_HANDLE;                                                          \
    auto* native = it->second->native.get();                                                       \
    lock.unlock();                                                                                 \
    return diagnostics->set_backend_resource_name(*native, name);                                  \
  }
#define GRANIT_NAME_PRESENTATION_OBJECT(map)                                                       \
  if (const auto it = map.find(object); it != map.end()) {                                         \
    if (it->second->owner != owner)                                                                \
      return GRANIT_ERROR_INVALID_HANDLE;                                                          \
    auto* native = it->second->native.get();                                                       \
    lock.unlock();                                                                                 \
    return diagnostics->set_backend_resource_name(*native, name);                                  \
  }
  GRANIT_NAME_PRESENTATION_OBJECT(surfaces_)
  GRANIT_NAME_PRESENTATION_OBJECT(swapchains_)
  GRANIT_NAME_PRESENTATION_OBJECT(buffers_)
  GRANIT_NAME_PRESENTATION_OBJECT(textures_)
  GRANIT_NAME_PRESENTATION_OBJECT(texture_views_)
  GRANIT_NAME_PRESENTATION_OBJECT(samplers_)
  GRANIT_NAME_PRESENTATION_OBJECT(shaders_)
  GRANIT_NAME_PRESENTATION_OBJECT(bind_group_layouts_)
  GRANIT_NAME_PRESENTATION_OBJECT(pipeline_layouts_)
  GRANIT_NAME_PRESENTATION_OBJECT(bind_groups_)
  GRANIT_NAME_PRESENTATION_OBJECT(graphics_pipelines_)
  GRANIT_NAME_PRESENTATION_OBJECT(compute_pipelines_)
  GRANIT_NAME_PRESENTATION_OBJECT(command_recorders_)
  GRANIT_NAME_PRESENTATION_OBJECT(timestamp_query_pools_)
#undef GRANIT_NAME_OBJECT
#undef GRANIT_NAME_PRESENTATION_OBJECT

  if (frames_.contains(object) || upload_batches_.contains(object))
    return GRANIT_ERROR_UNSUPPORTED;
  return GRANIT_ERROR_INVALID_HANDLE;
}

void renderer_registry::emit_validation_diagnostic(granit_renderer renderer,
                                                   std::string_view message) noexcept {
  const auto diagnostics =
      std::dynamic_pointer_cast<backend_diagnostic_renderer>(acquire_backend(renderer));
  if (diagnostics) {
    diagnostics->diagnostics().emit(diagnostic_severity::error, diagnostic_category::validation,
                                    message);
  }
}

granit_result renderer_registry::destroy(granit_renderer renderer) {
  std::shared_ptr<backend_renderer> state;
  std::shared_ptr<backend_diagnostic_renderer> diagnostics;
  std::shared_ptr<backend_presentation_renderer> presentation;
  std::shared_ptr<backend_queue> queue;
  std::shared_ptr<backend_retirement_renderer> retirement;
  lifecycle_snapshot lifecycle;
  std::vector<std::shared_ptr<command_recorder_record>> native_command_recorders;
  std::vector<std::shared_ptr<swapchain_record>> native_swapchains;
  std::vector<std::shared_ptr<surface_record>> native_surfaces;
  std::vector<std::shared_ptr<buffer_record>> native_buffers;
  std::vector<std::shared_ptr<texture_view_record>> native_texture_views;
  std::vector<std::shared_ptr<texture_record>> native_textures;
  std::vector<std::shared_ptr<sampler_record>> native_samplers;
  std::vector<std::shared_ptr<shader_record>> native_shaders;
  std::vector<std::shared_ptr<pipeline_layout_record>> native_pipeline_layouts;
  std::vector<std::shared_ptr<bind_group_layout_record>> native_bind_group_layouts;
  std::vector<std::shared_ptr<bind_group_record>> native_bind_groups;
  std::vector<std::shared_ptr<graphics_pipeline_record>> native_graphics_pipelines;
  std::vector<std::shared_ptr<compute_pipeline_record>> native_compute_pipelines;
  std::vector<std::shared_ptr<upload_batch_record>> native_upload_batches;
  std::vector<std::shared_ptr<timestamp_query_pool_record>> native_timestamp_query_pools;
  {
    std::unique_lock lock{mutex_};
    if (handles_.find(renderer, resource_type::renderer, 0) == nullptr) {
      return GRANIT_ERROR_INVALID_HANDLE;
    }
    auto backend_found = backend_renderers_.find(renderer);
    if (backend_found == backend_renderers_.end()) {
      return GRANIT_ERROR_INTERNAL;
    }
    state = backend_found->second;
    diagnostics = std::dynamic_pointer_cast<backend_diagnostic_renderer>(state);
    presentation = std::dynamic_pointer_cast<backend_presentation_renderer>(state);
    queue = std::dynamic_pointer_cast<backend_queue>(state);
    retirement = std::dynamic_pointer_cast<backend_retirement_renderer>(state);
    backend_renderers_.erase(renderer);
    if (diagnostics && diagnostics->validation_enabled()) {
      for (const auto& [handle, record] : frame_contexts_) {
        if (record->owner == state)
          lifecycle.add(lifecycle_resource_type::frame_context, handle,
                        record->metadata.creation_sequence);
      }
      for (const auto& [handle, record] : buffers_) {
        if (record->owner == state) {
          lifecycle.add(lifecycle_resource_type::buffer, handle,
                        record->metadata.creation_sequence);
        }
      }
      for (const auto& [handle, record] : textures_) {
        if (record->owner == state && record->publicly_destroyable) {
          lifecycle.add(lifecycle_resource_type::texture, handle,
                        record->metadata.creation_sequence);
        }
      }
      for (const auto& [handle, record] : texture_views_) {
        if (record->owner == state && record->publicly_destroyable) {
          lifecycle.add(lifecycle_resource_type::texture_view, handle,
                        record->metadata.creation_sequence);
        }
      }
      for (const auto& [handle, record] : samplers_) {
        if (record->owner == state) {
          lifecycle.add(lifecycle_resource_type::sampler, handle,
                        record->metadata.creation_sequence);
        }
      }
      for (const auto& [handle, record] : shaders_) {
        if (record->owner == state) {
          lifecycle.add(lifecycle_resource_type::shader, handle,
                        record->metadata.creation_sequence);
        }
      }
      for (const auto& [handle, record] : pipeline_layouts_) {
        if (record->owner == state)
          lifecycle.add(lifecycle_resource_type::pipeline_layout, handle,
                        record->metadata.creation_sequence);
      }
      for (const auto& [handle, record] : bind_group_layouts_) {
        if (record->owner == state)
          lifecycle.add(lifecycle_resource_type::bind_group_layout, handle,
                        record->metadata.creation_sequence);
      }
      for (const auto& [handle, record] : bind_groups_) {
        if (record->owner == state)
          lifecycle.add(lifecycle_resource_type::bind_group, handle,
                        record->metadata.creation_sequence);
      }
      for (const auto& [handle, record] : graphics_pipelines_) {
        if (record->owner == state)
          lifecycle.add(lifecycle_resource_type::graphics_pipeline, handle,
                        record->metadata.creation_sequence);
      }
      for (const auto& [handle, record] : compute_pipelines_) {
        if (record->owner == state)
          lifecycle.add(lifecycle_resource_type::compute_pipeline, handle,
                        record->metadata.creation_sequence);
      }
      for (const auto& [handle, record] : surfaces_) {
        if (record->owner == state) {
          lifecycle.add(lifecycle_resource_type::surface, handle,
                        record->metadata.creation_sequence);
        }
      }
      for (const auto& [handle, record] : swapchains_) {
        if (record->owner == state) {
          lifecycle.add(lifecycle_resource_type::swapchain, handle,
                        record->metadata.creation_sequence);
        }
      }
      for (const auto& [handle, record] : command_recorders_) {
        if (record->owner == state) {
          lifecycle.add(lifecycle_resource_type::command_recorder, handle,
                        record->metadata.creation_sequence);
        }
      }
      for (const auto& [handle, record] : upload_batches_) {
        if (record->owner == state) {
          lifecycle.add(lifecycle_resource_type::upload_batch, handle,
                        record->metadata.creation_sequence);
        }
      }
      for (const auto& [handle, record] : timestamp_query_pools_) {
        if (record->owner == state)
          lifecycle.add(lifecycle_resource_type::timestamp_query_pool, handle,
                        record->metadata.creation_sequence);
      }
    }
    for (auto frame = frames_.begin(); frame != frames_.end();) {
      if (frame->second->owner == state) {
        static_cast<void>(handles_.erase(frame->first, resource_type::frame, state->domain()));
        frame = frames_.erase(frame);
      } else {
        ++frame;
      }
    }
    for (auto context = frame_contexts_.begin(); context != frame_contexts_.end();) {
      if (context->second->owner == state) {
        static_cast<void>(
            handles_.erase(context->first, resource_type::frame_context, state->domain()));
        context = frame_contexts_.erase(context);
      } else {
        ++context;
      }
    }
    for (auto recorder = command_recorders_.begin(); recorder != command_recorders_.end();) {
      if (recorder->second->owner == state) {
        native_command_recorders.push_back(std::move(recorder->second));
        static_cast<void>(
            handles_.erase(recorder->first, resource_type::command_recorder, state->domain()));
        recorder = command_recorders_.erase(recorder);
      } else {
        ++recorder;
      }
    }
    for (auto query = timestamp_query_pools_.begin(); query != timestamp_query_pools_.end();) {
      if (query->second->owner == state) {
        native_timestamp_query_pools.push_back(std::move(query->second));
        static_cast<void>(
            handles_.erase(query->first, resource_type::timestamp_query_pool, state->domain()));
        query = timestamp_query_pools_.erase(query);
      } else {
        ++query;
      }
    }
    for (auto batch = upload_batches_.begin(); batch != upload_batches_.end();) {
      if (batch->second->owner == state) {
        native_upload_batches.push_back(std::move(batch->second));
        static_cast<void>(
            handles_.erase(batch->first, resource_type::upload_batch, state->domain()));
        batch = upload_batches_.erase(batch);
      } else {
        ++batch;
      }
    }
    for (auto sampler = samplers_.begin(); sampler != samplers_.end();) {
      if (sampler->second->owner == state) {
        native_samplers.push_back(std::move(sampler->second));
        static_cast<void>(handles_.erase(sampler->first, resource_type::sampler, state->domain()));
        sampler = samplers_.erase(sampler);
      } else {
        ++sampler;
      }
    }
    for (auto pipeline = graphics_pipelines_.begin(); pipeline != graphics_pipelines_.end();) {
      if (pipeline->second->owner == state) {
        native_graphics_pipelines.push_back(std::move(pipeline->second));
        static_cast<void>(
            handles_.erase(pipeline->first, resource_type::pipeline, state->domain()));
        pipeline = graphics_pipelines_.erase(pipeline);
      } else {
        ++pipeline;
      }
    }
    for (auto pipeline = compute_pipelines_.begin(); pipeline != compute_pipelines_.end();) {
      if (pipeline->second->owner == state) {
        native_compute_pipelines.push_back(std::move(pipeline->second));
        static_cast<void>(
            handles_.erase(pipeline->first, resource_type::compute_pipeline, state->domain()));
        pipeline = compute_pipelines_.erase(pipeline);
      } else {
        ++pipeline;
      }
    }
    for (auto bind_group = bind_groups_.begin(); bind_group != bind_groups_.end();) {
      if (bind_group->second->owner == state) {
        native_bind_groups.push_back(std::move(bind_group->second));
        static_cast<void>(
            handles_.erase(bind_group->first, resource_type::bind_group, state->domain()));
        bind_group = bind_groups_.erase(bind_group);
      } else {
        ++bind_group;
      }
    }
    for (auto layout = pipeline_layouts_.begin(); layout != pipeline_layouts_.end();) {
      if (layout->second->owner == state) {
        native_pipeline_layouts.push_back(std::move(layout->second));
        static_cast<void>(
            handles_.erase(layout->first, resource_type::pipeline_layout, state->domain()));
        layout = pipeline_layouts_.erase(layout);
      } else {
        ++layout;
      }
    }
    for (auto layout = bind_group_layouts_.begin(); layout != bind_group_layouts_.end();) {
      if (layout->second->owner == state) {
        native_bind_group_layouts.push_back(std::move(layout->second));
        static_cast<void>(
            handles_.erase(layout->first, resource_type::bind_group_layout, state->domain()));
        layout = bind_group_layouts_.erase(layout);
      } else {
        ++layout;
      }
    }
    for (auto shader = shaders_.begin(); shader != shaders_.end();) {
      if (shader->second->owner == state) {
        native_shaders.push_back(std::move(shader->second));
        static_cast<void>(handles_.erase(shader->first, resource_type::shader, state->domain()));
        shader = shaders_.erase(shader);
      } else {
        ++shader;
      }
    }
    for (auto view = texture_views_.begin(); view != texture_views_.end();) {
      if (view->second->owner == state) {
        native_texture_views.push_back(std::move(view->second));
        static_cast<void>(
            handles_.erase(view->first, resource_type::texture_view, state->domain()));
        view = texture_views_.erase(view);
      } else {
        ++view;
      }
    }
    for (auto texture = textures_.begin(); texture != textures_.end();) {
      if (texture->second->owner == state) {
        native_textures.push_back(std::move(texture->second));
        static_cast<void>(handles_.erase(texture->first, resource_type::texture, state->domain()));
        texture = textures_.erase(texture);
      } else {
        ++texture;
      }
    }
    for (auto buffer = buffers_.begin(); buffer != buffers_.end();) {
      if (buffer->second->owner == state) {
        native_buffers.push_back(std::move(buffer->second));
        static_cast<void>(handles_.erase(buffer->first, resource_type::buffer, state->domain()));
        buffer = buffers_.erase(buffer);
      } else {
        ++buffer;
      }
    }
    for (auto swapchain = swapchains_.begin(); swapchain != swapchains_.end();) {
      if (swapchain->second->owner == state) {
        native_swapchains.push_back(std::move(swapchain->second));
        static_cast<void>(
            handles_.erase(swapchain->first, resource_type::swapchain, state->domain()));
        swapchain = swapchains_.erase(swapchain);
      } else {
        ++swapchain;
      }
    }
    for (auto surface = surfaces_.begin(); surface != surfaces_.end();) {
      if (surface->second->owner == state) {
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
  if (diagnostics)
    write_lifecycle_diagnostic(diagnostics->diagnostics(), renderer, state->domain(), lifecycle);
  if (presentation)
    static_cast<void>(presentation->wait_for_present_idle());
  if (queue)
    static_cast<void>(queue->wait_for_all_submissions());
  native_command_recorders.clear();
  native_timestamp_query_pools.clear();
  native_upload_batches.clear();
  if (retirement)
    static_cast<void>(retirement->collect_retired());
  native_swapchains.clear();
  native_surfaces.clear();
  native_buffers.clear();
  native_texture_views.clear();
  native_textures.clear();
  native_samplers.clear();
  native_graphics_pipelines.clear();
  native_compute_pipelines.clear();
  native_bind_groups.clear();
  native_pipeline_layouts.clear();
  native_bind_group_layouts.clear();
  native_shaders.clear();
  // 析构可能等待 GPU 空闲，不应占用全局 registry 锁。
  state.reset();
  return GRANIT_SUCCESS;
}

std::shared_ptr<renderer_registry::command_recorder_record>
renderer_registry::acquire_command_recorder(granit_renderer renderer,
                                            granit_command_recorder recorder) {
  std::lock_guard lock{mutex_};
  const auto found_renderer = backend_renderers_.find(renderer);
  if (found_renderer == backend_renderers_.end() ||
      handles_.find(renderer, resource_type::renderer, 0) == nullptr) {
    return {};
  }
  const auto& state = found_renderer->second;
  if (handles_.find(recorder, resource_type::command_recorder, state->domain()) == nullptr) {
    return {};
  }
  const auto found = command_recorders_.find(recorder);
  if (found == command_recorders_.end() || found->second->owner != state) {
    return {};
  }
  return found->second;
}

std::shared_ptr<backend_renderer> renderer_registry::acquire_backend(granit_renderer renderer) {
  std::lock_guard lock{mutex_};
  if (handles_.find(renderer, resource_type::renderer, 0) == nullptr) {
    return {};
  }
  const auto found = backend_renderers_.find(renderer);
  return found == backend_renderers_.end() ? std::shared_ptr<backend_renderer>{} : found->second;
}

std::uint32_t renderer_registry::allocate_domain() noexcept {
  const auto domain = next_domain_++;
  if (next_domain_ == 0) {
    next_domain_ = 1;
  }
  return domain == 0 ? allocate_domain() : domain;
}

} // namespace granit::detail
