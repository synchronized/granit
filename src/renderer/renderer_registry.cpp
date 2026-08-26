// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "renderer/renderer_registry.h"

#include "backend/vulkan/resources.h"
#include "core/texture_format.h"

#include <algorithm>
#include <cstring>
#include <functional>
#include <limits>
#include <new>
#include <type_traits>
#include <utility>
#include <vector>

namespace granit::detail {
namespace {

template <typename Handle> std::uint64_t object_handle_value(Handle handle) noexcept {
  if constexpr (std::is_pointer_v<Handle>) {
    return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(handle));
  } else {
    return static_cast<std::uint64_t>(handle);
  }
}

VkSampler& native_sampler(backend_sampler_resource& resource) noexcept {
  return static_cast<vulkan_sampler_resource&>(resource).native();
}

VkImageView& native_texture_view(backend_texture_view_resource& resource) noexcept {
  return static_cast<vulkan_texture_view_resource&>(resource).native();
}

vulkan_image_allocation& native_texture(backend_texture_resource& resource) noexcept {
  return static_cast<vulkan_texture_resource&>(resource).native();
}

vulkan_buffer_allocation& native_buffer(backend_buffer_resource& resource) noexcept {
  return static_cast<vulkan_buffer_resource&>(resource).native();
}

VkShaderModule& native_shader(backend_shader_resource& resource) noexcept {
  return static_cast<vulkan_shader_resource&>(resource).native();
}

VkDescriptorSetLayout&
native_bind_group_layout(backend_bind_group_layout_resource& resource) noexcept {
  return static_cast<vulkan_bind_group_layout_resource&>(resource).native();
}

vulkan_bind_group_resource& native_bind_group(backend_bind_group_resource& resource) noexcept {
  return static_cast<vulkan_bind_group_resource&>(resource);
}

VkPipelineLayout& native_pipeline_layout(backend_pipeline_layout_resource& resource) noexcept {
  return static_cast<vulkan_pipeline_layout_resource&>(resource).native();
}

VkPipeline& native_graphics_pipeline(backend_graphics_pipeline_resource& resource) noexcept {
  return static_cast<vulkan_graphics_pipeline_resource&>(resource).native();
}

VkPipeline& native_compute_pipeline(backend_compute_pipeline_resource& resource) noexcept {
  return static_cast<vulkan_compute_pipeline_resource&>(resource).native();
}

vulkan_command_recorder&
native_command_recorder(backend_command_recorder_resource& resource) noexcept {
  return static_cast<vulkan_command_recorder_resource&>(resource).native();
}

VkSurfaceKHR& native_surface_handle(backend_surface_resource& resource) noexcept {
  return static_cast<vulkan_surface_resource&>(resource).native();
}

granit_texture_format swapchain_format(VkFormat format) noexcept {
  switch (format) {
  case VK_FORMAT_B8G8R8A8_SRGB:
    return GRANIT_TEXTURE_FORMAT_BGRA8_SRGB;
  case VK_FORMAT_B8G8R8A8_UNORM:
    return GRANIT_TEXTURE_FORMAT_BGRA8_UNORM;
  case VK_FORMAT_R8G8B8A8_SRGB:
    return GRANIT_TEXTURE_FORMAT_RGBA8_SRGB;
  case VK_FORMAT_R8G8B8A8_UNORM:
    return GRANIT_TEXTURE_FORMAT_RGBA8_UNORM;
  default:
    return GRANIT_TEXTURE_FORMAT_UNDEFINED;
  }
}

template <typename Resources, typename Resource, typename Metadata>
void retain_resource(Resources& resources, const Resource& resource, Metadata& metadata) {
  for (const auto& retained : resources) {
    if (retained.resource.get() == resource.get()) {
      return;
    }
  }
  resources.push_back({.resource = resource, .metadata = &metadata});
}

template <typename Resources>
void mark_resources_used(Resources& resources, submission_serial serial) noexcept {
  for (auto& retained : resources) {
    auto current = retained.metadata->last_use_serial.load();
    while (current < serial &&
           !retained.metadata->last_use_serial.compare_exchange_weak(current, serial)) {
    }
  }
}

bool ranges_overlap(std::uint64_t left_offset, std::uint64_t left_size, std::uint64_t right_offset,
                    std::uint64_t right_size) noexcept {
  return left_offset < right_offset + right_size && right_offset < left_offset + left_size;
}

VkAttachmentLoadOp map_load(granit_attachment_load_operation value) noexcept {
  return value == GRANIT_ATTACHMENT_LOAD_OPERATION_LOAD
             ? VK_ATTACHMENT_LOAD_OP_LOAD
             : (value == GRANIT_ATTACHMENT_LOAD_OPERATION_CLEAR ? VK_ATTACHMENT_LOAD_OP_CLEAR
                                                                : VK_ATTACHMENT_LOAD_OP_DONT_CARE);
}

VkAttachmentStoreOp map_store(granit_attachment_store_operation value) noexcept {
  return value == GRANIT_ATTACHMENT_STORE_OPERATION_STORE ? VK_ATTACHMENT_STORE_OP_STORE
                                                          : VK_ATTACHMENT_STORE_OP_DONT_CARE;
}

bool depth_format(granit_texture_format format) noexcept {
  return format >= GRANIT_TEXTURE_FORMAT_D16_UNORM;
}

bool stencil_format(granit_texture_format format) noexcept {
  return format == GRANIT_TEXTURE_FORMAT_D24_UNORM_S8_UINT ||
         format == GRANIT_TEXTURE_FORMAT_D32_FLOAT_S8_UINT;
}

VkImageAspectFlags map_aspect(granit_texture_aspect aspect) noexcept {
  VkImageAspectFlags result{};
  if ((aspect & GRANIT_TEXTURE_ASPECT_COLOR_BIT) != 0)
    result |= VK_IMAGE_ASPECT_COLOR_BIT;
  if ((aspect & GRANIT_TEXTURE_ASPECT_DEPTH_BIT) != 0)
    result |= VK_IMAGE_ASPECT_DEPTH_BIT;
  if ((aspect & GRANIT_TEXTURE_ASPECT_STENCIL_BIT) != 0)
    result |= VK_IMAGE_ASPECT_STENCIL_BIT;
  return result;
}

} // namespace

renderer_registry& renderer_registry::instance() {
  static renderer_registry registry;
  return registry;
}

renderer_registry::swapchain_record::~swapchain_record() {
  if (renderer && native) {
    renderer->destroy_native_swapchain(*native);
  }
}

renderer_registry::timestamp_query_pool_record::~timestamp_query_pool_record() {
  if (renderer)
    native.destroy(renderer->device());
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

granit_result renderer_registry::import_pipeline_cache(granit_renderer renderer, const void* data,
                                                       std::uint64_t size) {
  const auto state = acquire(renderer);
  return state ? state->import_pipeline_cache(data, size) : GRANIT_ERROR_INVALID_HANDLE;
}

granit_result renderer_registry::export_pipeline_cache(granit_renderer renderer, void* data,
                                                       std::uint64_t& size) {
  const auto state = acquire(renderer);
  return state ? state->export_pipeline_cache(data, size) : GRANIT_ERROR_INVALID_HANDLE;
}

granit_result renderer_registry::set_object_name(granit_renderer renderer, granit_handle object,
                                                 std::string_view name) {
  std::unique_lock lock{mutex_};
  const auto renderer_it = renderers_.find(renderer);
  if (renderer_it == renderers_.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  const auto& state = renderer_it->second;

#define GRANIT_NAME_OBJECT(map, type, expression)                                                  \
  if (const auto it = map.find(object); it != map.end()) {                                         \
    if (it->second->renderer != state)                                                             \
      return GRANIT_ERROR_INVALID_HANDLE;                                                          \
    const auto native = object_handle_value(expression);                                           \
    lock.unlock();                                                                                 \
    return state->set_object_name(type, native, name);                                             \
  }
  GRANIT_NAME_OBJECT(surfaces_, VK_OBJECT_TYPE_SURFACE_KHR,
                     native_surface_handle(*it->second->native))
  GRANIT_NAME_OBJECT(swapchains_, VK_OBJECT_TYPE_SWAPCHAIN_KHR, it->second->native->native_handle())
  GRANIT_NAME_OBJECT(buffers_, VK_OBJECT_TYPE_BUFFER, native_buffer(*it->second->native).buffer)
  GRANIT_NAME_OBJECT(textures_, VK_OBJECT_TYPE_IMAGE, native_texture(*it->second->native).image)
  GRANIT_NAME_OBJECT(texture_views_, VK_OBJECT_TYPE_IMAGE_VIEW,
                     native_texture_view(*it->second->native))
  GRANIT_NAME_OBJECT(samplers_, VK_OBJECT_TYPE_SAMPLER, native_sampler(*it->second->native))
  GRANIT_NAME_OBJECT(shaders_, VK_OBJECT_TYPE_SHADER_MODULE, native_shader(*it->second->native))
  GRANIT_NAME_OBJECT(bind_group_layouts_, VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT,
                     native_bind_group_layout(*it->second->native))
  GRANIT_NAME_OBJECT(pipeline_layouts_, VK_OBJECT_TYPE_PIPELINE_LAYOUT,
                     native_pipeline_layout(*it->second->native))
  GRANIT_NAME_OBJECT(bind_groups_, VK_OBJECT_TYPE_DESCRIPTOR_SET,
                     native_bind_group(*it->second->native).set())
  GRANIT_NAME_OBJECT(graphics_pipelines_, VK_OBJECT_TYPE_PIPELINE,
                     native_graphics_pipeline(*it->second->native))
  GRANIT_NAME_OBJECT(compute_pipelines_, VK_OBJECT_TYPE_PIPELINE,
                     native_compute_pipeline(*it->second->native))
  GRANIT_NAME_OBJECT(command_recorders_, VK_OBJECT_TYPE_COMMAND_BUFFER,
                     native_command_recorder(*it->second->native).native_handle())
  GRANIT_NAME_OBJECT(timestamp_query_pools_, VK_OBJECT_TYPE_QUERY_POOL,
                     it->second->native.native_handle())
#undef GRANIT_NAME_OBJECT

  if (frames_.contains(object) || upload_batches_.contains(object))
    return GRANIT_ERROR_UNSUPPORTED;
  return GRANIT_ERROR_INVALID_HANDLE;
}

void renderer_registry::emit_validation_diagnostic(granit_renderer renderer,
                                                    std::string_view message) noexcept {
  const auto state = acquire(renderer);
  if (state) {
    state->diagnostics().emit(diagnostic_severity::error, diagnostic_category::validation,
                              message);
  }
}

granit_result renderer_registry::destroy(granit_renderer renderer) {
  std::shared_ptr<renderer_state> state;
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
    if (state->validation_enabled()) {
      for (const auto& [handle, record] : frame_contexts_) {
        if (record->renderer == state)
          lifecycle.add(lifecycle_resource_type::frame_context, handle,
                        record->metadata.creation_sequence);
      }
      for (const auto& [handle, record] : buffers_) {
        if (record->renderer == state) {
          lifecycle.add(lifecycle_resource_type::buffer, handle,
                        record->metadata.creation_sequence);
        }
      }
      for (const auto& [handle, record] : textures_) {
        if (record->renderer == state && record->publicly_destroyable) {
          lifecycle.add(lifecycle_resource_type::texture, handle,
                        record->metadata.creation_sequence);
        }
      }
      for (const auto& [handle, record] : texture_views_) {
        if (record->renderer == state && record->publicly_destroyable) {
          lifecycle.add(lifecycle_resource_type::texture_view, handle,
                        record->metadata.creation_sequence);
        }
      }
      for (const auto& [handle, record] : samplers_) {
        if (record->renderer == state) {
          lifecycle.add(lifecycle_resource_type::sampler, handle,
                        record->metadata.creation_sequence);
        }
      }
      for (const auto& [handle, record] : shaders_) {
        if (record->renderer == state) {
          lifecycle.add(lifecycle_resource_type::shader, handle,
                        record->metadata.creation_sequence);
        }
      }
      for (const auto& [handle, record] : pipeline_layouts_) {
        if (record->renderer == state)
          lifecycle.add(lifecycle_resource_type::pipeline_layout, handle,
                        record->metadata.creation_sequence);
      }
      for (const auto& [handle, record] : bind_group_layouts_) {
        if (record->renderer == state)
          lifecycle.add(lifecycle_resource_type::bind_group_layout, handle,
                        record->metadata.creation_sequence);
      }
      for (const auto& [handle, record] : bind_groups_) {
        if (record->renderer == state)
          lifecycle.add(lifecycle_resource_type::bind_group, handle,
                        record->metadata.creation_sequence);
      }
      for (const auto& [handle, record] : graphics_pipelines_) {
        if (record->renderer == state)
          lifecycle.add(lifecycle_resource_type::graphics_pipeline, handle,
                        record->metadata.creation_sequence);
      }
      for (const auto& [handle, record] : compute_pipelines_) {
        if (record->renderer == state)
          lifecycle.add(lifecycle_resource_type::compute_pipeline, handle,
                        record->metadata.creation_sequence);
      }
      for (const auto& [handle, record] : surfaces_) {
        if (record->renderer == state) {
          lifecycle.add(lifecycle_resource_type::surface, handle,
                        record->metadata.creation_sequence);
        }
      }
      for (const auto& [handle, record] : swapchains_) {
        if (record->renderer == state) {
          lifecycle.add(lifecycle_resource_type::swapchain, handle,
                        record->metadata.creation_sequence);
        }
      }
      for (const auto& [handle, record] : command_recorders_) {
        if (record->renderer == state) {
          lifecycle.add(lifecycle_resource_type::command_recorder, handle,
                        record->metadata.creation_sequence);
        }
      }
      for (const auto& [handle, record] : upload_batches_) {
        if (record->renderer == state) {
          lifecycle.add(lifecycle_resource_type::upload_batch, handle,
                        record->metadata.creation_sequence);
        }
      }
      for (const auto& [handle, record] : timestamp_query_pools_) {
        if (record->renderer == state)
          lifecycle.add(lifecycle_resource_type::timestamp_query_pool, handle,
                        record->metadata.creation_sequence);
      }
    }
    for (auto frame = frames_.begin(); frame != frames_.end();) {
      if (frame->second->renderer == state) {
        static_cast<void>(handles_.erase(frame->first, resource_type::frame, state->domain()));
        frame = frames_.erase(frame);
      } else {
        ++frame;
      }
    }
    for (auto context = frame_contexts_.begin(); context != frame_contexts_.end();) {
      if (context->second->renderer == state) {
        static_cast<void>(
            handles_.erase(context->first, resource_type::frame_context, state->domain()));
        context = frame_contexts_.erase(context);
      } else {
        ++context;
      }
    }
    for (auto recorder = command_recorders_.begin(); recorder != command_recorders_.end();) {
      if (recorder->second->renderer == state) {
        native_command_recorders.push_back(std::move(recorder->second));
        static_cast<void>(
            handles_.erase(recorder->first, resource_type::command_recorder, state->domain()));
        recorder = command_recorders_.erase(recorder);
      } else {
        ++recorder;
      }
    }
    for (auto query = timestamp_query_pools_.begin(); query != timestamp_query_pools_.end();) {
      if (query->second->renderer == state) {
        native_timestamp_query_pools.push_back(std::move(query->second));
        static_cast<void>(
            handles_.erase(query->first, resource_type::timestamp_query_pool, state->domain()));
        query = timestamp_query_pools_.erase(query);
      } else {
        ++query;
      }
    }
    for (auto batch = upload_batches_.begin(); batch != upload_batches_.end();) {
      if (batch->second->renderer == state) {
        native_upload_batches.push_back(std::move(batch->second));
        static_cast<void>(
            handles_.erase(batch->first, resource_type::upload_batch, state->domain()));
        batch = upload_batches_.erase(batch);
      } else {
        ++batch;
      }
    }
    for (auto sampler = samplers_.begin(); sampler != samplers_.end();) {
      if (sampler->second->renderer == state) {
        native_samplers.push_back(std::move(sampler->second));
        static_cast<void>(handles_.erase(sampler->first, resource_type::sampler, state->domain()));
        sampler = samplers_.erase(sampler);
      } else {
        ++sampler;
      }
    }
    for (auto pipeline = graphics_pipelines_.begin(); pipeline != graphics_pipelines_.end();) {
      if (pipeline->second->renderer == state) {
        native_graphics_pipelines.push_back(std::move(pipeline->second));
        static_cast<void>(
            handles_.erase(pipeline->first, resource_type::pipeline, state->domain()));
        pipeline = graphics_pipelines_.erase(pipeline);
      } else {
        ++pipeline;
      }
    }
    for (auto pipeline = compute_pipelines_.begin(); pipeline != compute_pipelines_.end();) {
      if (pipeline->second->renderer == state) {
        native_compute_pipelines.push_back(std::move(pipeline->second));
        static_cast<void>(
            handles_.erase(pipeline->first, resource_type::compute_pipeline, state->domain()));
        pipeline = compute_pipelines_.erase(pipeline);
      } else {
        ++pipeline;
      }
    }
    for (auto bind_group = bind_groups_.begin(); bind_group != bind_groups_.end();) {
      if (bind_group->second->renderer == state) {
        native_bind_groups.push_back(std::move(bind_group->second));
        static_cast<void>(
            handles_.erase(bind_group->first, resource_type::bind_group, state->domain()));
        bind_group = bind_groups_.erase(bind_group);
      } else {
        ++bind_group;
      }
    }
    for (auto layout = pipeline_layouts_.begin(); layout != pipeline_layouts_.end();) {
      if (layout->second->renderer == state) {
        native_pipeline_layouts.push_back(std::move(layout->second));
        static_cast<void>(
            handles_.erase(layout->first, resource_type::pipeline_layout, state->domain()));
        layout = pipeline_layouts_.erase(layout);
      } else {
        ++layout;
      }
    }
    for (auto layout = bind_group_layouts_.begin(); layout != bind_group_layouts_.end();) {
      if (layout->second->renderer == state) {
        native_bind_group_layouts.push_back(std::move(layout->second));
        static_cast<void>(
            handles_.erase(layout->first, resource_type::bind_group_layout, state->domain()));
        layout = bind_group_layouts_.erase(layout);
      } else {
        ++layout;
      }
    }
    for (auto shader = shaders_.begin(); shader != shaders_.end();) {
      if (shader->second->renderer == state) {
        native_shaders.push_back(std::move(shader->second));
        static_cast<void>(handles_.erase(shader->first, resource_type::shader, state->domain()));
        shader = shaders_.erase(shader);
      } else {
        ++shader;
      }
    }
    for (auto view = texture_views_.begin(); view != texture_views_.end();) {
      if (view->second->renderer == state) {
        native_texture_views.push_back(std::move(view->second));
        static_cast<void>(
            handles_.erase(view->first, resource_type::texture_view, state->domain()));
        view = texture_views_.erase(view);
      } else {
        ++view;
      }
    }
    for (auto texture = textures_.begin(); texture != textures_.end();) {
      if (texture->second->renderer == state) {
        native_textures.push_back(std::move(texture->second));
        static_cast<void>(handles_.erase(texture->first, resource_type::texture, state->domain()));
        texture = textures_.erase(texture);
      } else {
        ++texture;
      }
    }
    for (auto buffer = buffers_.begin(); buffer != buffers_.end();) {
      if (buffer->second->renderer == state) {
        native_buffers.push_back(std::move(buffer->second));
        static_cast<void>(handles_.erase(buffer->first, resource_type::buffer, state->domain()));
        buffer = buffers_.erase(buffer);
      } else {
        ++buffer;
      }
    }
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
  write_lifecycle_diagnostic(state->diagnostics(), renderer, state->domain(), lifecycle);
  static_cast<void>(state->wait_for_present_idle());
  static_cast<void>(state->wait_for_all_submissions());
  native_command_recorders.clear();
  native_timestamp_query_pools.clear();
  native_upload_batches.clear();
  static_cast<void>(state->drain_retired());
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
    record->native = std::make_unique<vulkan_surface_resource>(state);
    const auto create_result = state->create_win32_surface(native_instance, native_window,
                                                           native_surface_handle(*record->native));
    if (create_result != GRANIT_SUCCESS) {
      return create_result;
    }

    std::lock_guard lock{mutex_};
    const auto renderer_found = renderers_.find(renderer);
    if (renderer_found == renderers_.end() || renderer_found->second != state) {
      return GRANIT_ERROR_INVALID_HANDLE;
    }
    record->metadata.creation_sequence = next_creation_sequence_++;
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

granit_result renderer_registry::create_xcb_surface(granit_renderer renderer, void* connection,
                                                    std::uint32_t window, granit_surface& surface) {
  try {
    auto state = acquire(renderer);
    if (!state)
      return GRANIT_ERROR_INVALID_HANDLE;

    auto record = std::make_shared<surface_record>();
    record->renderer = state;
    record->native = std::make_unique<vulkan_surface_resource>(state);
    const auto create_result =
        state->create_xcb_surface(connection, window, native_surface_handle(*record->native));
    if (create_result != GRANIT_SUCCESS)
      return create_result;

    std::lock_guard lock{mutex_};
    const auto renderer_found = renderers_.find(renderer);
    if (renderer_found == renderers_.end() || renderer_found->second != state)
      return GRANIT_ERROR_INVALID_HANDLE;
    record->metadata.creation_sequence = next_creation_sequence_++;
    const auto handle = handles_.insert(record.get(), resource_type::surface, state->domain());
    if (handle == GRANIT_NULL_HANDLE)
      return GRANIT_ERROR_OUT_OF_MEMORY;
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

granit_result renderer_registry::create_wayland_surface(granit_renderer renderer, void* display,
                                                        void* native_surface,
                                                        granit_surface& surface) {
  try {
    auto state = acquire(renderer);
    if (!state)
      return GRANIT_ERROR_INVALID_HANDLE;

    auto record = std::make_shared<surface_record>();
    record->renderer = state;
    record->native = std::make_unique<vulkan_surface_resource>(state);
    const auto create_result = state->create_wayland_surface(
        display, native_surface, native_surface_handle(*record->native));
    if (create_result != GRANIT_SUCCESS)
      return create_result;

    std::lock_guard lock{mutex_};
    const auto renderer_found = renderers_.find(renderer);
    if (renderer_found == renderers_.end() || renderer_found->second != state)
      return GRANIT_ERROR_INVALID_HANDLE;
    record->metadata.creation_sequence = next_creation_sequence_++;
    const auto handle = handles_.insert(record.get(), resource_type::surface, state->domain());
    if (handle == GRANIT_NULL_HANDLE)
      return GRANIT_ERROR_OUT_OF_MEMORY;
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
  lifecycle_snapshot lifecycle;
  std::vector<std::shared_ptr<swapchain_record>> native_swapchains;
  std::vector<std::shared_ptr<texture_view_record>> native_views;
  std::vector<std::shared_ptr<texture_record>> native_textures;
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
    for (const auto& [frame_handle, frame] : frames_) {
      static_cast<void>(frame_handle);
      if (frame->swapchain->surface == found->second)
        return GRANIT_ERROR_INVALID_ARGUMENT;
    }
    native_surface = found->second;
  }
  const auto idle_result = state->wait_for_present_idle();
  if (idle_result != GRANIT_SUCCESS && idle_result != GRANIT_ERROR_DEVICE_LOST)
    return idle_result;
  static_cast<void>(state->collect_retired());
  {
    std::lock_guard lock{mutex_};
    const auto found = surfaces_.find(surface);
    if (found == surfaces_.end() || found->second != native_surface)
      return GRANIT_ERROR_INVALID_HANDLE;
    for (const auto& [frame_handle, frame] : frames_) {
      static_cast<void>(frame_handle);
      if (frame->swapchain->surface == native_surface)
        return GRANIT_ERROR_INVALID_ARGUMENT;
    }
    for (auto swapchain = swapchains_.begin(); swapchain != swapchains_.end();) {
      if (swapchain->second->surface == found->second) {
        if (state->validation_enabled()) {
          lifecycle.add(lifecycle_resource_type::swapchain, swapchain->first,
                        swapchain->second->metadata.creation_sequence);
        }
        for (const auto handle : swapchain->second->views) {
          const auto view = texture_views_.find(handle);
          if (view != texture_views_.end()) {
            native_views.push_back(std::move(view->second));
            texture_views_.erase(view);
          }
          static_cast<void>(handles_.erase(handle, resource_type::texture_view, state->domain()));
        }
        for (const auto handle : swapchain->second->textures) {
          const auto texture = textures_.find(handle);
          if (texture != textures_.end()) {
            native_textures.push_back(std::move(texture->second));
            textures_.erase(texture);
          }
          static_cast<void>(handles_.erase(handle, resource_type::texture, state->domain()));
        }
        native_swapchains.push_back(std::move(swapchain->second));
        static_cast<void>(
            handles_.erase(swapchain->first, resource_type::swapchain, state->domain()));
        swapchain = swapchains_.erase(swapchain);
      } else {
        ++swapchain;
      }
    }
    surfaces_.erase(found);
    const auto erase_result = handles_.erase(surface, resource_type::surface, state->domain());
    if (erase_result != GRANIT_SUCCESS) {
      return erase_result;
    }
  }
  write_child_lifecycle_diagnostic(state->diagnostics(), lifecycle_resource_type::surface, surface,
                                   lifecycle_resource_type::swapchain,
                                   lifecycle.summary(lifecycle_resource_type::swapchain));
  native_views.clear();
  native_textures.clear();
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
    const auto create_result = state->create_swapchain(
        native_surface_handle(*surface_state->native), desc, *record->native);
    if (create_result != GRANIT_SUCCESS) {
      return create_result;
    }

    granit_swapchain handle = GRANIT_NULL_HANDLE;
    {
      std::lock_guard lock{mutex_};
      const auto surface_found = surfaces_.find(surface);
      if (surface_found == surfaces_.end() || surface_found->second != surface_state) {
        return GRANIT_ERROR_INVALID_HANDLE;
      }
      record->metadata.creation_sequence = next_creation_sequence_++;
      handle = handles_.insert(record.get(), resource_type::swapchain, state->domain());
      if (handle == GRANIT_NULL_HANDLE)
        return GRANIT_ERROR_OUT_OF_MEMORY;
      try {
        swapchains_.emplace(handle, record);
      } catch (...) {
        static_cast<void>(handles_.erase(handle, resource_type::swapchain, state->domain()));
        throw;
      }
    }
    const auto backbuffer_result = install_swapchain_backbuffers(handle, record);
    if (backbuffer_result != GRANIT_SUCCESS) {
      static_cast<void>(destroy_swapchain(renderer, handle));
      return backbuffer_result;
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
  std::vector<std::shared_ptr<texture_view_record>> old_views;
  std::vector<std::shared_ptr<texture_record>> old_textures;
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
    if (state->device_lost())
      return GRANIT_ERROR_DEVICE_LOST;
    if (found->second->surface_lost)
      return GRANIT_ERROR_SURFACE_LOST;
    for (const auto& [frame_handle, frame] : frames_) {
      static_cast<void>(frame_handle);
      if (frame->swapchain == found->second)
        return GRANIT_ERROR_INVALID_ARGUMENT;
    }
    record = found->second;
  }
  const auto idle_result = record->renderer->wait_for_present_idle();
  if (idle_result != GRANIT_SUCCESS)
    return idle_result;
  static_cast<void>(record->renderer->collect_retired());
  {
    std::lock_guard lock{mutex_};
    const auto renderer_found = renderers_.find(renderer);
    const auto found = swapchains_.find(swapchain);
    if (renderer_found == renderers_.end() || found == swapchains_.end() ||
        renderer_found->second != record->renderer || found->second != record)
      return GRANIT_ERROR_INVALID_HANDLE;
    for (const auto& [frame_handle, frame] : frames_) {
      static_cast<void>(frame_handle);
      if (frame->swapchain == record)
        return GRANIT_ERROR_INVALID_ARGUMENT;
    }
    for (const auto handle : record->views) {
      const auto view = texture_views_.find(handle);
      if (view != texture_views_.end()) {
        old_views.push_back(std::move(view->second));
        texture_views_.erase(view);
      }
      static_cast<void>(
          handles_.erase(handle, resource_type::texture_view, record->renderer->domain()));
    }
    for (const auto handle : record->textures) {
      const auto texture = textures_.find(handle);
      if (texture != textures_.end()) {
        old_textures.push_back(std::move(texture->second));
        textures_.erase(texture);
      }
      static_cast<void>(handles_.erase(handle, resource_type::texture, record->renderer->domain()));
    }
    record->views.clear();
    record->textures.clear();
  }
  old_views.clear();
  old_textures.clear();
  const auto result = record->renderer->recreate_swapchain(
      native_surface_handle(*record->surface->native), desc, *record->native);
  const auto install_result = install_swapchain_backbuffers(swapchain, record);
  return result == GRANIT_SUCCESS ? install_result : result;
}

granit_result
renderer_registry::install_swapchain_backbuffers(granit_swapchain swapchain,
                                                 const std::shared_ptr<swapchain_record>& record) {
  try {
    const auto info = record->native->info();
    std::vector<std::shared_ptr<texture_record>> textures;
    std::vector<std::shared_ptr<texture_view_record>> views;
    textures.reserve(record->native->images().size());
    views.reserve(record->native->images().size());
    for (const auto image : record->native->images()) {
      auto texture = std::make_shared<texture_record>();
      texture->renderer = record->renderer;
      texture->native = std::make_unique<vulkan_texture_resource>(record->renderer, false);
      native_texture(*texture->native).image = image;
      texture->publicly_destroyable = false;
      texture->desc = GRANIT_TEXTURE_DESC_INIT;
      texture->desc.format = swapchain_format(record->native->format());
      texture->desc.usage = GRANIT_TEXTURE_USAGE_COLOR_ATTACHMENT_BIT;
      texture->desc.width = info.width;
      texture->desc.height = info.height;
      auto view = std::make_shared<texture_view_record>();
      view->renderer = record->renderer;
      view->texture = texture;
      view->publicly_destroyable = false;
      granit_texture_view_desc desc = GRANIT_TEXTURE_VIEW_DESC_INIT;
      view->desc = desc;
      view->native = std::make_unique<vulkan_texture_view_resource>(record->renderer);
      const auto result = record->renderer->create_native_texture_view(
          native_texture(*texture->native), texture->desc, desc,
          native_texture_view(*view->native));
      if (result != GRANIT_SUCCESS)
        return result;
      textures.push_back(std::move(texture));
      views.push_back(std::move(view));
    }

    std::lock_guard lock{mutex_};
    const auto found = swapchains_.find(swapchain);
    if (found == swapchains_.end() || found->second != record)
      return GRANIT_ERROR_INVALID_HANDLE;
    record->textures.reserve(textures.size());
    record->views.reserve(views.size());
    for (std::size_t index = 0; index < textures.size(); ++index) {
      textures[index]->metadata.creation_sequence = next_creation_sequence_++;
      views[index]->metadata.creation_sequence = next_creation_sequence_++;
      const auto texture_handle = handles_.insert(textures[index].get(), resource_type::texture,
                                                  record->renderer->domain());
      if (texture_handle == GRANIT_NULL_HANDLE)
        return GRANIT_ERROR_OUT_OF_MEMORY;
      const auto view_handle = handles_.insert(views[index].get(), resource_type::texture_view,
                                               record->renderer->domain());
      if (view_handle == GRANIT_NULL_HANDLE) {
        static_cast<void>(
            handles_.erase(texture_handle, resource_type::texture, record->renderer->domain()));
        return GRANIT_ERROR_OUT_OF_MEMORY;
      }
      record->textures.push_back(texture_handle);
      record->views.push_back(view_handle);
      textures_.emplace(texture_handle, textures[index]);
      texture_views_.emplace(view_handle, views[index]);
    }
    return GRANIT_SUCCESS;
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
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

granit_result renderer_registry::get_swapchain_backbuffer(granit_renderer renderer,
                                                          granit_swapchain swapchain,
                                                          std::uint32_t index,
                                                          granit_texture& texture,
                                                          granit_texture_view& view) {
  std::lock_guard lock{mutex_};
  const auto renderer_found = renderers_.find(renderer);
  if (renderer_found == renderers_.end() ||
      handles_.find(renderer, resource_type::renderer, 0) == nullptr)
    return GRANIT_ERROR_INVALID_HANDLE;
  const auto& state = renderer_found->second;
  if (handles_.find(swapchain, resource_type::swapchain, state->domain()) == nullptr)
    return GRANIT_ERROR_INVALID_HANDLE;
  const auto found = swapchains_.find(swapchain);
  if (found == swapchains_.end() || found->second->renderer != state)
    return GRANIT_ERROR_INVALID_HANDLE;
  if (state->device_lost())
    return GRANIT_ERROR_DEVICE_LOST;
  if (found->second->surface_lost)
    return GRANIT_ERROR_SURFACE_LOST;
  if (index >= found->second->textures.size())
    return GRANIT_ERROR_INVALID_ARGUMENT;
  texture = found->second->textures[index];
  view = found->second->views[index];
  return GRANIT_SUCCESS;
}

granit_result renderer_registry::acquire_swapchain_frame(granit_renderer renderer,
                                                         granit_swapchain swapchain,
                                                         granit_frame& frame,
                                                         std::uint32_t& image_index,
                                                         bool& needs_recreate) {
  std::shared_ptr<swapchain_record> swapchain_record_state;
  {
    std::lock_guard lock{mutex_};
    const auto renderer_found = renderers_.find(renderer);
    if (renderer_found == renderers_.end())
      return GRANIT_ERROR_INVALID_HANDLE;
    const auto found = swapchains_.find(swapchain);
    if (found == swapchains_.end() || found->second->renderer != renderer_found->second)
      return GRANIT_ERROR_INVALID_HANDLE;
    if (renderer_found->second->device_lost())
      return GRANIT_ERROR_DEVICE_LOST;
    if (found->second->surface_lost)
      return GRANIT_ERROR_SURFACE_LOST;
    swapchain_record_state = found->second;
  }
  auto record = std::make_shared<frame_record>();
  record->renderer = swapchain_record_state->renderer;
  record->swapchain = swapchain_record_state;
  granit_frame handle{};
  {
    std::lock_guard lock{mutex_};
    const auto found = swapchains_.find(swapchain);
    if (found == swapchains_.end() || found->second != swapchain_record_state)
      return GRANIT_ERROR_INVALID_HANDLE;
    handle = handles_.insert(record.get(), resource_type::frame, record->renderer->domain());
    if (handle == GRANIT_NULL_HANDLE)
      return GRANIT_ERROR_OUT_OF_MEMORY;
    try {
      frames_.emplace(handle, record);
    } catch (...) {
      static_cast<void>(handles_.erase(handle, resource_type::frame, record->renderer->domain()));
      throw;
    }
  }
  const auto result = record->renderer->acquire_swapchain_frame(
      *swapchain_record_state->native, record->image_index, record->slot_index, needs_recreate);
  static_cast<void>(record->renderer->collect_retired());
  if (result != GRANIT_SUCCESS) {
    std::lock_guard lock{mutex_};
    if (result == GRANIT_ERROR_SURFACE_LOST)
      swapchain_record_state->surface_lost = true;
    frames_.erase(handle);
    static_cast<void>(handles_.erase(handle, resource_type::frame, record->renderer->domain()));
    return result;
  }
  frame = handle;
  image_index = record->image_index;
  return GRANIT_SUCCESS;
}

granit_result renderer_registry::get_frame_info(granit_renderer renderer,
                                                granit_swapchain swapchain, granit_frame frame,
                                                std::uint32_t& frame_slot,
                                                std::uint32_t& frame_slot_count) {
  std::shared_ptr<frame_record> record;
  {
    std::lock_guard lock{mutex_};
    const auto found_renderer = renderers_.find(renderer);
    const auto found_swapchain = swapchains_.find(swapchain);
    const auto found_frame = frames_.find(frame);
    if (found_renderer == renderers_.end() || found_swapchain == swapchains_.end() ||
        found_frame == frames_.end() || found_frame->second->renderer != found_renderer->second ||
        found_frame->second->swapchain != found_swapchain->second ||
        handles_.find(frame, resource_type::frame, found_renderer->second->domain()) == nullptr)
      return GRANIT_ERROR_INVALID_HANDLE;
    record = found_frame->second;
  }
  std::lock_guard frame_lock{record->mutex};
  const auto slot_count = record->renderer->frame_slot_count();
  if (record->slot_index >= slot_count || slot_count > UINT32_MAX)
    return GRANIT_ERROR_INTERNAL;
  frame_slot = static_cast<std::uint32_t>(record->slot_index);
  frame_slot_count = static_cast<std::uint32_t>(slot_count);
  return GRANIT_SUCCESS;
}

granit_result renderer_registry::submit_command_recorder_frame(granit_renderer renderer,
                                                               granit_command_recorder recorder,
                                                               granit_frame frame) {
  std::shared_ptr<frame_record> frame_state;
  auto command = acquire_command_recorder(renderer, recorder);
  if (!command)
    return GRANIT_ERROR_INVALID_HANDLE;
  {
    std::lock_guard lock{mutex_};
    const auto found = frames_.find(frame);
    if (found == frames_.end() || found->second->renderer != command->renderer)
      return GRANIT_ERROR_INVALID_HANDLE;
    frame_state = found->second;
  }
  std::scoped_lock locks{command->mutex, frame_state->mutex};
  if (frame_state->submitted)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  submission_serial serial{};
  const auto result = command->renderer->submit_swapchain_frame(
      native_command_recorder(*command->native), *frame_state->swapchain->native,
      frame_state->image_index, frame_state->slot_index, serial);
  if (result == GRANIT_SUCCESS) {
    mark_resources_used(command->retained_resources, serial);
    frame_state->submitted = true;
  }
  return result;
}

granit_result renderer_registry::present_swapchain_frame(granit_renderer renderer,
                                                         granit_swapchain swapchain,
                                                         granit_frame frame, bool& needs_recreate) {
  std::shared_ptr<frame_record> record;
  {
    std::lock_guard lock{mutex_};
    const auto found = frames_.find(frame);
    const auto found_swapchain = swapchains_.find(swapchain);
    const auto found_renderer = renderers_.find(renderer);
    if (found_renderer == renderers_.end() || found == frames_.end() ||
        found_swapchain == swapchains_.end() ||
        found->second->swapchain != found_swapchain->second ||
        found->second->renderer != found_swapchain->second->renderer ||
        found->second->renderer != found_renderer->second)
      return GRANIT_ERROR_INVALID_HANDLE;
    record = found->second;
  }
  std::lock_guard frame_lock{record->mutex};
  if (!record->submitted)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const auto result = record->renderer->present_swapchain_frame(
      *record->swapchain->native, record->image_index, record->slot_index, needs_recreate);
  static_cast<void>(record->renderer->collect_retired());
  if (result == GRANIT_ERROR_SURFACE_LOST) {
    std::lock_guard lock{mutex_};
    record->swapchain->surface_lost = true;
  }
  {
    std::lock_guard lock{mutex_};
    const auto found = frames_.find(frame);
    if (found != frames_.end() && found->second == record) {
      frames_.erase(found);
      static_cast<void>(handles_.erase(frame, resource_type::frame, record->renderer->domain()));
    }
  }
  return result;
}

granit_result renderer_registry::cancel_swapchain_frame(granit_renderer renderer,
                                                        granit_swapchain swapchain,
                                                        granit_frame frame, bool& needs_recreate) {
  std::shared_ptr<frame_record> record;
  {
    std::lock_guard lock{mutex_};
    const auto found_renderer = renderers_.find(renderer);
    const auto found_swapchain = swapchains_.find(swapchain);
    const auto found = frames_.find(frame);
    if (found_renderer == renderers_.end() || found_swapchain == swapchains_.end() ||
        found == frames_.end() || found->second->renderer != found_renderer->second ||
        found->second->swapchain != found_swapchain->second)
      return GRANIT_ERROR_INVALID_HANDLE;
    record = found->second;
  }
  std::lock_guard frame_lock{record->mutex};
  if (record->submitted)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const auto result = record->renderer->cancel_swapchain_frame(
      *record->swapchain->native, record->image_index, record->slot_index, needs_recreate);
  if (result == GRANIT_ERROR_SURFACE_LOST) {
    std::lock_guard lock{mutex_};
    record->swapchain->surface_lost = true;
  }
  if (result != GRANIT_SUCCESS && result != GRANIT_ERROR_OUT_OF_DATE &&
      result != GRANIT_ERROR_SURFACE_LOST && result != GRANIT_ERROR_DEVICE_LOST)
    return result;
  {
    std::lock_guard lock{mutex_};
    const auto found = frames_.find(frame);
    if (found != frames_.end() && found->second == record) {
      frames_.erase(found);
      static_cast<void>(handles_.erase(frame, resource_type::frame, record->renderer->domain()));
    }
  }
  return result;
}

granit_result renderer_registry::destroy_swapchain(granit_renderer renderer,
                                                   granit_swapchain swapchain) {
  std::shared_ptr<swapchain_record> record;
  std::vector<std::shared_ptr<texture_view_record>> views;
  std::vector<std::shared_ptr<texture_record>> textures;
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
    for (const auto& [frame_handle, frame] : frames_) {
      static_cast<void>(frame_handle);
      if (frame->swapchain == found->second)
        return GRANIT_ERROR_INVALID_ARGUMENT;
    }
    record = found->second;
  }
  const auto idle_result = record->renderer->wait_for_present_idle();
  if (idle_result != GRANIT_SUCCESS && idle_result != GRANIT_ERROR_DEVICE_LOST)
    return idle_result;
  static_cast<void>(record->renderer->collect_retired());
  {
    std::lock_guard lock{mutex_};
    const auto renderer_found = renderers_.find(renderer);
    const auto found = swapchains_.find(swapchain);
    if (renderer_found == renderers_.end() || found == swapchains_.end() ||
        renderer_found->second != record->renderer || found->second != record)
      return GRANIT_ERROR_INVALID_HANDLE;
    for (const auto& [frame_handle, frame] : frames_) {
      static_cast<void>(frame_handle);
      if (frame->swapchain == record)
        return GRANIT_ERROR_INVALID_ARGUMENT;
    }
    for (const auto handle : record->views) {
      const auto item = texture_views_.find(handle);
      if (item != texture_views_.end()) {
        views.push_back(std::move(item->second));
        texture_views_.erase(item);
      }
      static_cast<void>(
          handles_.erase(handle, resource_type::texture_view, record->renderer->domain()));
    }
    for (const auto handle : record->textures) {
      const auto item = textures_.find(handle);
      if (item != textures_.end()) {
        textures.push_back(std::move(item->second));
        textures_.erase(item);
      }
      static_cast<void>(handles_.erase(handle, resource_type::texture, record->renderer->domain()));
    }
    swapchains_.erase(found);
    const auto erase_result =
        handles_.erase(swapchain, resource_type::swapchain, record->renderer->domain());
    if (erase_result != GRANIT_SUCCESS) {
      return erase_result;
    }
  }
  views.clear();
  textures.clear();
  record.reset();
  return GRANIT_SUCCESS;
}

granit_result renderer_registry::create_buffer(granit_renderer renderer,
                                               const granit_buffer_desc& desc,
                                               granit_buffer& buffer) {
  try {
    auto state = acquire(renderer);
    if (!state) {
      return GRANIT_ERROR_INVALID_HANDLE;
    }
    auto record = std::make_shared<buffer_record>();
    record->renderer = state;
    record->desc = desc;
    record->native = std::make_unique<vulkan_buffer_resource>(state);
    const auto create_result = state->create_native_buffer(desc, native_buffer(*record->native));
    if (create_result != GRANIT_SUCCESS) {
      return create_result;
    }

    std::lock_guard lock{mutex_};
    const auto renderer_found = renderers_.find(renderer);
    if (renderer_found == renderers_.end() || renderer_found->second != state) {
      return GRANIT_ERROR_INVALID_HANDLE;
    }
    record->metadata.creation_sequence = next_creation_sequence_++;
    const auto handle = handles_.insert(record.get(), resource_type::buffer, state->domain());
    if (handle == GRANIT_NULL_HANDLE) {
      return GRANIT_ERROR_OUT_OF_MEMORY;
    }
    try {
      buffers_.emplace(handle, std::move(record));
    } catch (...) {
      static_cast<void>(handles_.erase(handle, resource_type::buffer, state->domain()));
      throw;
    }
    buffer = handle;
    return GRANIT_SUCCESS;
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result renderer_registry::map_buffer(granit_renderer renderer, granit_buffer buffer,
                                            std::uint64_t offset, std::uint64_t size, void*& data) {
  std::shared_ptr<buffer_record> record;
  {
    std::lock_guard lock{mutex_};
    const auto renderer_found = renderers_.find(renderer);
    if (renderer_found == renderers_.end() ||
        handles_.find(renderer, resource_type::renderer, 0) == nullptr) {
      return GRANIT_ERROR_INVALID_HANDLE;
    }
    const auto& state = renderer_found->second;
    if (handles_.find(buffer, resource_type::buffer, state->domain()) == nullptr) {
      return GRANIT_ERROR_INVALID_HANDLE;
    }
    const auto found = buffers_.find(buffer);
    if (found == buffers_.end() || found->second->renderer != state) {
      return GRANIT_ERROR_INVALID_HANDLE;
    }
    record = found->second;
  }

  std::lock_guard record_lock{record->mutex};
  if (record->mapped) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  if (record->desc.memory_location != GRANIT_MEMORY_LOCATION_UPLOAD &&
      record->desc.memory_location != GRANIT_MEMORY_LOCATION_READBACK) {
    return GRANIT_ERROR_UNSUPPORTED;
  }
  if (offset >= record->desc.size || size == 0 || size > record->desc.size - offset) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  if (record->desc.memory_location == GRANIT_MEMORY_LOCATION_READBACK) {
    const auto result =
        record->renderer->invalidate_buffer(native_buffer(*record->native), offset, size);
    if (result != GRANIT_SUCCESS) {
      return result;
    }
  }
  record->mapped = true;
  record->mapped_offset = offset;
  record->mapped_size = size;
  data = static_cast<unsigned char*>(native_buffer(*record->native).mapped_data) + offset;
  return GRANIT_SUCCESS;
}

granit_result renderer_registry::get_buffer_desc(granit_renderer renderer, granit_buffer buffer,
                                                 granit_buffer_desc& desc) {
  std::lock_guard lock{mutex_};
  const auto renderer_found = renderers_.find(renderer);
  if (renderer_found == renderers_.end() ||
      handles_.find(renderer, resource_type::renderer, 0) == nullptr) {
    return GRANIT_ERROR_INVALID_HANDLE;
  }
  const auto& state = renderer_found->second;
  if (handles_.find(buffer, resource_type::buffer, state->domain()) == nullptr)
    return GRANIT_ERROR_INVALID_HANDLE;
  const auto found = buffers_.find(buffer);
  if (found == buffers_.end() || found->second->renderer != state)
    return GRANIT_ERROR_INVALID_HANDLE;
  desc = found->second->desc;
  return GRANIT_SUCCESS;
}

granit_result renderer_registry::unmap_buffer(granit_renderer renderer, granit_buffer buffer) {
  std::shared_ptr<buffer_record> record;
  {
    std::lock_guard lock{mutex_};
    const auto renderer_found = renderers_.find(renderer);
    if (renderer_found == renderers_.end() ||
        handles_.find(renderer, resource_type::renderer, 0) == nullptr) {
      return GRANIT_ERROR_INVALID_HANDLE;
    }
    const auto& state = renderer_found->second;
    if (handles_.find(buffer, resource_type::buffer, state->domain()) == nullptr) {
      return GRANIT_ERROR_INVALID_HANDLE;
    }
    const auto found = buffers_.find(buffer);
    if (found == buffers_.end() || found->second->renderer != state) {
      return GRANIT_ERROR_INVALID_HANDLE;
    }
    record = found->second;
  }

  std::lock_guard record_lock{record->mutex};
  if (!record->mapped) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  granit_result result = GRANIT_SUCCESS;
  if (record->desc.memory_location == GRANIT_MEMORY_LOCATION_UPLOAD) {
    result = record->renderer->flush_buffer(native_buffer(*record->native), record->mapped_offset,
                                            record->mapped_size);
  }
  record->mapped = false;
  record->mapped_offset = 0;
  record->mapped_size = 0;
  return result;
}

granit_result renderer_registry::flush_mapped_buffer(granit_renderer renderer, granit_buffer buffer,
                                                     std::uint64_t offset, std::uint64_t size) {
  std::shared_ptr<buffer_record> record;
  {
    std::lock_guard lock{mutex_};
    const auto renderer_found = renderers_.find(renderer);
    if (renderer_found == renderers_.end() ||
        handles_.find(renderer, resource_type::renderer, 0) == nullptr) {
      return GRANIT_ERROR_INVALID_HANDLE;
    }
    const auto& state = renderer_found->second;
    if (handles_.find(buffer, resource_type::buffer, state->domain()) == nullptr)
      return GRANIT_ERROR_INVALID_HANDLE;
    const auto found = buffers_.find(buffer);
    if (found == buffers_.end() || found->second->renderer != state)
      return GRANIT_ERROR_INVALID_HANDLE;
    record = found->second;
  }

  std::lock_guard record_lock{record->mutex};
  if (!record->mapped || record->desc.memory_location != GRANIT_MEMORY_LOCATION_UPLOAD ||
      size == 0 || offset < record->mapped_offset ||
      offset > record->mapped_offset + record->mapped_size ||
      size > record->mapped_offset + record->mapped_size - offset) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  return record->renderer->flush_buffer(native_buffer(*record->native), offset, size);
}

granit_result renderer_registry::destroy_buffer(granit_renderer renderer, granit_buffer buffer) {
  std::shared_ptr<buffer_record> record;
  {
    std::lock_guard lock{mutex_};
    const auto renderer_found = renderers_.find(renderer);
    if (renderer_found == renderers_.end() ||
        handles_.find(renderer, resource_type::renderer, 0) == nullptr) {
      return GRANIT_ERROR_INVALID_HANDLE;
    }
    const auto& state = renderer_found->second;
    if (handles_.find(buffer, resource_type::buffer, state->domain()) == nullptr) {
      return GRANIT_ERROR_INVALID_HANDLE;
    }
    const auto found = buffers_.find(buffer);
    if (found == buffers_.end() || found->second->renderer != state) {
      return GRANIT_ERROR_INVALID_HANDLE;
    }
    {
      std::lock_guard record_lock{found->second->mutex};
      if (found->second->mapped) {
        return GRANIT_ERROR_INVALID_ARGUMENT;
      }
    }
    record = std::move(found->second);
    buffers_.erase(found);
    const auto erase_result = handles_.erase(buffer, resource_type::buffer, state->domain());
    if (erase_result != GRANIT_SUCCESS) {
      return erase_result;
    }
  }
  auto state = record->renderer;
  state->retire_resource(record->metadata.last_use_serial.load(), retirement_order::resource,
                         record);
  record.reset();
  static_cast<void>(state->collect_retired());
  return GRANIT_SUCCESS;
}

granit_result renderer_registry::write_buffer(granit_renderer renderer, granit_buffer buffer,
                                              std::uint64_t offset, const void* data,
                                              std::uint64_t size) {
  std::shared_ptr<buffer_record> record;
  {
    std::lock_guard lock{mutex_};
    const auto renderer_found = renderers_.find(renderer);
    if (renderer_found == renderers_.end() ||
        handles_.find(renderer, resource_type::renderer, 0) == nullptr) {
      return GRANIT_ERROR_INVALID_HANDLE;
    }
    const auto& state = renderer_found->second;
    if (handles_.find(buffer, resource_type::buffer, state->domain()) == nullptr) {
      return GRANIT_ERROR_INVALID_HANDLE;
    }
    const auto found = buffers_.find(buffer);
    if (found == buffers_.end() || found->second->renderer != state) {
      return GRANIT_ERROR_INVALID_HANDLE;
    }
    record = found->second;
  }

  std::lock_guard record_lock{record->mutex};
  if (record->mapped || size == 0 || offset >= record->desc.size ||
      size > record->desc.size - offset) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  if (record->desc.memory_location == GRANIT_MEMORY_LOCATION_READBACK) {
    return GRANIT_ERROR_UNSUPPORTED;
  }
  if (record->desc.memory_location == GRANIT_MEMORY_LOCATION_UPLOAD) {
    std::memcpy(static_cast<unsigned char*>(native_buffer(*record->native).mapped_data) + offset,
                data, static_cast<std::size_t>(size));
    return record->renderer->flush_buffer(native_buffer(*record->native), offset, size);
  }
  return record->renderer->upload_buffer(native_buffer(*record->native), offset, data, size);
}

granit_result renderer_registry::create_upload_batch(granit_renderer renderer,
                                                     granit_upload_batch& batch) {
  try {
    auto state = acquire(renderer);
    if (!state)
      return GRANIT_ERROR_INVALID_HANDLE;
    auto record = std::make_shared<upload_batch_record>();
    record->renderer = state;
    std::lock_guard lock{mutex_};
    const auto found = renderers_.find(renderer);
    if (found == renderers_.end() || found->second != state)
      return GRANIT_ERROR_INVALID_HANDLE;
    record->metadata.creation_sequence = next_creation_sequence_++;
    const auto handle = handles_.insert(record.get(), resource_type::upload_batch, state->domain());
    if (handle == GRANIT_NULL_HANDLE)
      return GRANIT_ERROR_OUT_OF_MEMORY;
    try {
      upload_batches_.emplace(handle, std::move(record));
    } catch (...) {
      static_cast<void>(handles_.erase(handle, resource_type::upload_batch, state->domain()));
      throw;
    }
    batch = handle;
    return GRANIT_SUCCESS;
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result renderer_registry::upload_batch_write_buffer(granit_renderer renderer,
                                                           granit_upload_batch batch,
                                                           granit_buffer buffer,
                                                           std::uint64_t offset, const void* data,
                                                           std::uint64_t size) {
  std::shared_ptr<upload_batch_record> batch_record;
  std::shared_ptr<buffer_record> buffer_record;
  {
    std::lock_guard lock{mutex_};
    const auto found_renderer = renderers_.find(renderer);
    if (found_renderer == renderers_.end() ||
        handles_.find(renderer, resource_type::renderer, 0) == nullptr)
      return GRANIT_ERROR_INVALID_HANDLE;
    const auto& state = found_renderer->second;
    if (handles_.find(batch, resource_type::upload_batch, state->domain()) == nullptr ||
        handles_.find(buffer, resource_type::buffer, state->domain()) == nullptr)
      return GRANIT_ERROR_INVALID_HANDLE;
    const auto found_batch = upload_batches_.find(batch);
    const auto found_buffer = buffers_.find(buffer);
    if (found_batch == upload_batches_.end() || found_buffer == buffers_.end() ||
        found_batch->second->renderer != state || found_buffer->second->renderer != state)
      return GRANIT_ERROR_INVALID_HANDLE;
    batch_record = found_batch->second;
    buffer_record = found_buffer->second;
  }

  std::scoped_lock record_locks{batch_record->mutex, buffer_record->mutex};
  if (batch_record->failed || size > SIZE_MAX || buffer_record->mapped ||
      offset >= buffer_record->desc.size || size > buffer_record->desc.size - offset ||
      (offset & 3) != 0 || (size & 3) != 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  if (buffer_record->desc.memory_location != GRANIT_MEMORY_LOCATION_DEVICE &&
      buffer_record->desc.memory_location != GRANIT_MEMORY_LOCATION_AUTOMATIC)
    return GRANIT_ERROR_UNSUPPORTED;
  try {
    upload_entry entry{.type = backend_upload_type::buffer,
                       .buffer = buffer_record,
                       .texture = {},
                       .offset = offset,
                       .data = {},
                       .texture_copy = {}};
    entry.data.resize(static_cast<std::size_t>(size));
    std::memcpy(entry.data.data(), data, static_cast<std::size_t>(size));
    batch_record->uploads.push_back(std::move(entry));
    return GRANIT_SUCCESS;
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  }
}

granit_result renderer_registry::upload_batch_write_texture(
    granit_renderer renderer, granit_upload_batch batch, granit_texture texture, const void* data,
    std::uint64_t size, const granit_texture_data_layout& layout,
    const granit_texture_write_region& region) {
  std::shared_ptr<upload_batch_record> batch_record;
  std::shared_ptr<texture_record> texture_record;
  {
    std::lock_guard lock{mutex_};
    const auto found_renderer = renderers_.find(renderer);
    if (found_renderer == renderers_.end() ||
        handles_.find(renderer, resource_type::renderer, 0) == nullptr)
      return GRANIT_ERROR_INVALID_HANDLE;
    const auto& state = found_renderer->second;
    if (handles_.find(batch, resource_type::upload_batch, state->domain()) == nullptr ||
        handles_.find(texture, resource_type::texture, state->domain()) == nullptr)
      return GRANIT_ERROR_INVALID_HANDLE;
    const auto found_batch = upload_batches_.find(batch);
    const auto found_texture = textures_.find(texture);
    if (found_batch == upload_batches_.end() || found_texture == textures_.end() ||
        found_batch->second->renderer != state || found_texture->second->renderer != state)
      return GRANIT_ERROR_INVALID_HANDLE;
    batch_record = found_batch->second;
    texture_record = found_texture->second;
  }

  std::scoped_lock record_locks{batch_record->mutex, texture_record->mutex};
  if (batch_record->failed || size > SIZE_MAX)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const auto& desc = texture_record->desc;
  const auto bytes_per_pixel =
      depth_format(desc.format) ? 0 : texture_format_bytes_per_block(desc.format);
  if ((desc.usage & GRANIT_TEXTURE_USAGE_TRANSFER_DESTINATION_BIT) == 0 ||
      desc.sample_count != GRANIT_SAMPLE_COUNT_1 || bytes_per_pixel == 0)
    return GRANIT_ERROR_UNSUPPORTED;
  if (region.aspect != GRANIT_TEXTURE_ASPECT_COLOR_BIT || region.width == 0 || region.height == 0 ||
      region.depth == 0 || region.array_layer_count == 0 || region.mip_level >= desc.mip_levels ||
      region.base_array_layer >= desc.array_layers ||
      region.array_layer_count > desc.array_layers - region.base_array_layer)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const auto mip_width = std::max(UINT32_C(1), desc.width >> region.mip_level);
  const auto mip_height = std::max(UINT32_C(1), desc.height >> region.mip_level);
  const auto mip_depth = std::max(UINT32_C(1), desc.depth >> region.mip_level);
  if (region.x >= mip_width || region.width > mip_width - region.x || region.y >= mip_height ||
      region.height > mip_height - region.y || region.z >= mip_depth ||
      region.depth > mip_depth - region.z)
    return GRANIT_ERROR_INVALID_ARGUMENT;

  const std::uint64_t tight_row = std::uint64_t{region.width} * bytes_per_pixel;
  const std::uint64_t row_pitch = layout.bytes_per_row == 0 ? tight_row : layout.bytes_per_row;
  const std::uint64_t image_rows =
      layout.rows_per_image == 0 ? region.height : layout.rows_per_image;
  if (row_pitch < tight_row || row_pitch % bytes_per_pixel != 0 || image_rows < region.height)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const std::uint64_t image_count =
      desc.dimension == GRANIT_TEXTURE_DIMENSION_3D ? region.depth : region.array_layer_count;
  const auto max = std::numeric_limits<std::uint64_t>::max();
  if (image_rows > max / row_pitch || image_count - 1 > max / (image_rows * row_pitch) ||
      region.height - 1 > max / row_pitch)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const std::uint64_t required =
      (image_count - 1) * image_rows * row_pitch + (region.height - 1) * row_pitch + tight_row;
  if (layout.offset > size || required > size - layout.offset || required > SIZE_MAX)
    return GRANIT_ERROR_INVALID_ARGUMENT;

  const backend_texture_copy copy{
      .buffer_row_length = layout.bytes_per_row == 0 ? 0 : layout.bytes_per_row / bytes_per_pixel,
      .buffer_image_height = layout.rows_per_image,
      .aspect = region.aspect,
      .mip_level = region.mip_level,
      .base_array_layer = region.base_array_layer,
      .array_layer_count = region.array_layer_count,
      .x = static_cast<std::int32_t>(region.x),
      .y = static_cast<std::int32_t>(region.y),
      .z = static_cast<std::int32_t>(region.z),
      .width = region.width,
      .height = region.height,
      .depth = region.depth,
  };
  try {
    upload_entry entry{.type = backend_upload_type::texture,
                       .buffer = {},
                       .texture = texture_record,
                       .offset = 0,
                       .data = {},
                       .texture_copy = copy};
    entry.data.resize(static_cast<std::size_t>(required));
    std::memcpy(entry.data.data(), static_cast<const std::byte*>(data) + layout.offset,
                static_cast<std::size_t>(required));
    batch_record->uploads.push_back(std::move(entry));
    return GRANIT_SUCCESS;
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  }
}

granit_result renderer_registry::submit_upload_batch(granit_renderer renderer,
                                                     granit_upload_batch batch) {
  std::shared_ptr<upload_batch_record> record;
  {
    std::lock_guard lock{mutex_};
    const auto found_renderer = renderers_.find(renderer);
    if (found_renderer == renderers_.end() ||
        handles_.find(renderer, resource_type::renderer, 0) == nullptr ||
        handles_.find(batch, resource_type::upload_batch, found_renderer->second->domain()) ==
            nullptr)
      return GRANIT_ERROR_INVALID_HANDLE;
    const auto found = upload_batches_.find(batch);
    if (found == upload_batches_.end() || found->second->renderer != found_renderer->second)
      return GRANIT_ERROR_INVALID_HANDLE;
    record = found->second;
  }
  std::lock_guard batch_lock{record->mutex};
  if (record->failed)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  if (record->uploads.empty())
    return GRANIT_ERROR_INVALID_ARGUMENT;

  std::vector<backend_upload_operation> uploads;
  uploads.reserve(record->uploads.size());
  for (const auto& upload : record->uploads) {
    uploads.push_back({.type = upload.type,
                       .buffer = upload.buffer ? upload.buffer->native.get() : nullptr,
                       .texture = upload.texture ? upload.texture->native.get() : nullptr,
                       .destination_offset = upload.offset,
                       .data = upload.data.data(),
                       .size = upload.data.size(),
                       .texture_copy = upload.texture_copy});
  }
  const auto result = record->renderer->upload_batch(uploads);
  if (result == GRANIT_SUCCESS)
    record->uploads.clear();
  else
    record->failed = true;
  return result;
}

granit_result renderer_registry::reset_upload_batch(granit_renderer renderer,
                                                    granit_upload_batch batch) {
  std::shared_ptr<upload_batch_record> record;
  {
    std::lock_guard lock{mutex_};
    const auto found_renderer = renderers_.find(renderer);
    if (found_renderer == renderers_.end() ||
        handles_.find(renderer, resource_type::renderer, 0) == nullptr ||
        handles_.find(batch, resource_type::upload_batch, found_renderer->second->domain()) ==
            nullptr)
      return GRANIT_ERROR_INVALID_HANDLE;
    const auto found = upload_batches_.find(batch);
    if (found == upload_batches_.end() || found->second->renderer != found_renderer->second)
      return GRANIT_ERROR_INVALID_HANDLE;
    record = found->second;
  }
  std::lock_guard lock{record->mutex};
  record->uploads.clear();
  record->failed = false;
  return GRANIT_SUCCESS;
}

granit_result renderer_registry::destroy_upload_batch(granit_renderer renderer,
                                                      granit_upload_batch batch) {
  std::lock_guard lock{mutex_};
  const auto found_renderer = renderers_.find(renderer);
  if (found_renderer == renderers_.end() ||
      handles_.find(renderer, resource_type::renderer, 0) == nullptr ||
      handles_.find(batch, resource_type::upload_batch, found_renderer->second->domain()) ==
          nullptr)
    return GRANIT_ERROR_INVALID_HANDLE;
  const auto found = upload_batches_.find(batch);
  if (found == upload_batches_.end() || found->second->renderer != found_renderer->second)
    return GRANIT_ERROR_INVALID_HANDLE;
  const auto result =
      handles_.erase(batch, resource_type::upload_batch, found_renderer->second->domain());
  if (result != GRANIT_SUCCESS)
    return result;
  upload_batches_.erase(found);
  return GRANIT_SUCCESS;
}

granit_result renderer_registry::create_texture(granit_renderer renderer,
                                                const granit_texture_desc& desc,
                                                granit_texture& texture) {
  try {
    auto state = acquire(renderer);
    if (!state)
      return GRANIT_ERROR_INVALID_HANDLE;
    auto record = std::make_shared<texture_record>();
    record->renderer = state;
    record->desc = desc;
    record->native = std::make_unique<vulkan_texture_resource>(state);
    const auto result = state->create_native_texture(desc, native_texture(*record->native));
    if (result != GRANIT_SUCCESS)
      return result;
    std::lock_guard lock{mutex_};
    const auto found = renderers_.find(renderer);
    if (found == renderers_.end() || found->second != state)
      return GRANIT_ERROR_INVALID_HANDLE;
    record->metadata.creation_sequence = next_creation_sequence_++;
    const auto handle = handles_.insert(record.get(), resource_type::texture, state->domain());
    if (handle == GRANIT_NULL_HANDLE)
      return GRANIT_ERROR_OUT_OF_MEMORY;
    try {
      textures_.emplace(handle, std::move(record));
    } catch (...) {
      static_cast<void>(handles_.erase(handle, resource_type::texture, state->domain()));
      throw;
    }
    texture = handle;
    return GRANIT_SUCCESS;
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result renderer_registry::write_texture(granit_renderer renderer, granit_texture texture,
                                               const void* data, std::uint64_t size,
                                               const granit_texture_data_layout& layout,
                                               const granit_texture_write_region& region) {
  std::shared_ptr<texture_record> record;
  {
    std::lock_guard lock{mutex_};
    const auto renderer_found = renderers_.find(renderer);
    if (renderer_found == renderers_.end() ||
        handles_.find(renderer, resource_type::renderer, 0) == nullptr)
      return GRANIT_ERROR_INVALID_HANDLE;
    const auto& state = renderer_found->second;
    if (handles_.find(texture, resource_type::texture, state->domain()) == nullptr)
      return GRANIT_ERROR_INVALID_HANDLE;
    const auto found = textures_.find(texture);
    if (found == textures_.end() || found->second->renderer != state)
      return GRANIT_ERROR_INVALID_HANDLE;
    record = found->second;
  }

  std::lock_guard record_lock{record->mutex};
  const auto& desc = record->desc;
  const auto bytes_per_pixel =
      depth_format(desc.format) ? 0 : texture_format_bytes_per_block(desc.format);
  if ((desc.usage & GRANIT_TEXTURE_USAGE_TRANSFER_DESTINATION_BIT) == 0 ||
      desc.sample_count != GRANIT_SAMPLE_COUNT_1 || bytes_per_pixel == 0) {
    return GRANIT_ERROR_UNSUPPORTED;
  }
  if (region.aspect != GRANIT_TEXTURE_ASPECT_COLOR_BIT || region.width == 0 || region.height == 0 ||
      region.depth == 0 || region.array_layer_count == 0 || region.mip_level >= desc.mip_levels ||
      region.base_array_layer >= desc.array_layers ||
      region.array_layer_count > desc.array_layers - region.base_array_layer) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  const auto mip_width = std::max(UINT32_C(1), desc.width >> region.mip_level);
  const auto mip_height = std::max(UINT32_C(1), desc.height >> region.mip_level);
  const auto mip_depth = std::max(UINT32_C(1), desc.depth >> region.mip_level);
  if (region.x >= mip_width || region.width > mip_width - region.x || region.y >= mip_height ||
      region.height > mip_height - region.y || region.z >= mip_depth ||
      region.depth > mip_depth - region.z) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }

  const std::uint64_t tight_row = std::uint64_t{region.width} * bytes_per_pixel;
  const std::uint64_t row_pitch = layout.bytes_per_row == 0 ? tight_row : layout.bytes_per_row;
  const std::uint64_t image_rows =
      layout.rows_per_image == 0 ? region.height : layout.rows_per_image;
  if (row_pitch < tight_row || row_pitch % bytes_per_pixel != 0 || image_rows < region.height) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  const std::uint64_t image_count =
      desc.dimension == GRANIT_TEXTURE_DIMENSION_3D ? region.depth : region.array_layer_count;
  const auto max = std::numeric_limits<std::uint64_t>::max();
  if (image_rows > max / row_pitch || image_count - 1 > max / (image_rows * row_pitch) ||
      region.height - 1 > max / row_pitch) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  const std::uint64_t required =
      (image_count - 1) * image_rows * row_pitch + (region.height - 1) * row_pitch + tight_row;
  if (layout.offset > size || required > size - layout.offset) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }

  VkBufferImageCopy copy{};
  copy.bufferRowLength = layout.bytes_per_row == 0 ? 0 : layout.bytes_per_row / bytes_per_pixel;
  copy.bufferImageHeight = layout.rows_per_image;
  copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  copy.imageSubresource.mipLevel = region.mip_level;
  copy.imageSubresource.baseArrayLayer = region.base_array_layer;
  copy.imageSubresource.layerCount = region.array_layer_count;
  copy.imageOffset = {static_cast<std::int32_t>(region.x), static_cast<std::int32_t>(region.y),
                      static_cast<std::int32_t>(region.z)};
  copy.imageExtent = {region.width, region.height, region.depth};
  return record->renderer->upload_texture(native_texture(*record->native),
                                          static_cast<const unsigned char*>(data) + layout.offset,
                                          required, copy);
}

granit_result
renderer_registry::get_texture_readback_info(granit_renderer renderer, granit_texture texture,
                                             const granit_texture_write_region& region,
                                             granit_texture_readback_info& info) {
  std::shared_ptr<texture_record> record;
  {
    std::lock_guard lock{mutex_};
    const auto found_renderer = renderers_.find(renderer);
    if (found_renderer == renderers_.end() ||
        handles_.find(renderer, resource_type::renderer, 0) == nullptr ||
        handles_.find(texture, resource_type::texture, found_renderer->second->domain()) == nullptr)
      return GRANIT_ERROR_INVALID_HANDLE;
    const auto found = textures_.find(texture);
    if (found == textures_.end() || found->second->renderer != found_renderer->second)
      return GRANIT_ERROR_INVALID_HANDLE;
    record = found->second;
  }
  std::lock_guard lock{record->mutex};
  const auto& desc = record->desc;
  const auto bytes = depth_format(desc.format) ? 0 : texture_format_bytes_per_block(desc.format);
  if ((desc.usage & GRANIT_TEXTURE_USAGE_TRANSFER_SOURCE_BIT) == 0 ||
      desc.sample_count != GRANIT_SAMPLE_COUNT_1 || bytes == 0)
    return GRANIT_ERROR_UNSUPPORTED;
  if (region.aspect != GRANIT_TEXTURE_ASPECT_COLOR_BIT || region.width == 0 || region.height == 0 ||
      region.depth == 0 || region.array_layer_count == 0 || region.mip_level >= desc.mip_levels ||
      region.base_array_layer >= desc.array_layers ||
      region.array_layer_count > desc.array_layers - region.base_array_layer)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const auto mip_width = std::max(UINT32_C(1), desc.width >> region.mip_level);
  const auto mip_height = std::max(UINT32_C(1), desc.height >> region.mip_level);
  const auto mip_depth = std::max(UINT32_C(1), desc.depth >> region.mip_level);
  if (region.x >= mip_width || region.width > mip_width - region.x || region.y >= mip_height ||
      region.height > mip_height - region.y || region.z >= mip_depth ||
      region.depth > mip_depth - region.z)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const auto row = uint64_t{region.width} * bytes;
  const auto images =
      desc.dimension == GRANIT_TEXTURE_DIMENSION_3D ? region.depth : region.array_layer_count;
  if (row > UINT32_MAX || region.height > UINT32_MAX || images > UINT64_MAX / region.height ||
      images * region.height > UINT64_MAX / row)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  info.format = desc.format;
  info.width = region.width;
  info.height = region.height;
  info.depth = region.depth;
  info.array_layer_count = region.array_layer_count;
  info.bytes_per_row = static_cast<uint32_t>(row);
  info.rows_per_image = region.height;
  info.required_size = images * region.height * row;
  info.reserved[0] = 0;
  info.reserved[1] = 0;
  return GRANIT_SUCCESS;
}

granit_result renderer_registry::create_texture_view(granit_renderer renderer,
                                                     granit_texture texture,
                                                     const granit_texture_view_desc& desc,
                                                     granit_texture_view& view) {
  try {
    std::shared_ptr<renderer_state> state;
    std::shared_ptr<texture_record> parent;
    {
      std::lock_guard lock{mutex_};
      const auto renderer_found = renderers_.find(renderer);
      if (renderer_found == renderers_.end() ||
          handles_.find(renderer, resource_type::renderer, 0) == nullptr) {
        return GRANIT_ERROR_INVALID_HANDLE;
      }
      state = renderer_found->second;
      if (handles_.find(texture, resource_type::texture, state->domain()) == nullptr) {
        return GRANIT_ERROR_INVALID_HANDLE;
      }
      const auto found = textures_.find(texture);
      if (found == textures_.end() || found->second->renderer != state) {
        return GRANIT_ERROR_INVALID_HANDLE;
      }
      parent = found->second;
    }
    if (desc.dimension != parent->desc.dimension ||
        (desc.format != GRANIT_TEXTURE_FORMAT_UNDEFINED && desc.format != parent->desc.format)) {
      return GRANIT_ERROR_UNSUPPORTED;
    }
    if (desc.range.base_mip_level >= parent->desc.mip_levels ||
        desc.range.mip_level_count > parent->desc.mip_levels - desc.range.base_mip_level ||
        desc.range.base_array_layer >= parent->desc.array_layers ||
        desc.range.array_layer_count > parent->desc.array_layers - desc.range.base_array_layer) {
      return GRANIT_ERROR_INVALID_ARGUMENT;
    }
    const auto aspect = desc.range.aspect;
    const auto depth = parent->desc.format >= GRANIT_TEXTURE_FORMAT_D16_UNORM;
    const auto stencil = parent->desc.format == GRANIT_TEXTURE_FORMAT_D24_UNORM_S8_UINT ||
                         parent->desc.format == GRANIT_TEXTURE_FORMAT_D32_FLOAT_S8_UINT;
    if (aspect != GRANIT_TEXTURE_ASPECT_AUTOMATIC &&
        ((!depth && aspect != GRANIT_TEXTURE_ASPECT_COLOR_BIT) ||
         (depth && (aspect & GRANIT_TEXTURE_ASPECT_COLOR_BIT) != 0) ||
         (depth && !stencil && aspect != GRANIT_TEXTURE_ASPECT_DEPTH_BIT) ||
         (depth && stencil && (aspect & GRANIT_TEXTURE_ASPECT_DEPTH_BIT) == 0 &&
          (aspect & GRANIT_TEXTURE_ASPECT_STENCIL_BIT) == 0))) {
      return GRANIT_ERROR_INVALID_ARGUMENT;
    }
    auto record = std::make_shared<texture_view_record>();
    record->renderer = state;
    record->texture = parent;
    record->desc = desc;
    record->native = std::make_unique<vulkan_texture_view_resource>(state);
    const auto result = state->create_native_texture_view(
        native_texture(*parent->native), parent->desc, desc, native_texture_view(*record->native));
    if (result != GRANIT_SUCCESS)
      return result;
    std::lock_guard lock{mutex_};
    const auto parent_found = textures_.find(texture);
    if (parent_found == textures_.end() || parent_found->second != parent) {
      return GRANIT_ERROR_INVALID_HANDLE;
    }
    record->metadata.creation_sequence = next_creation_sequence_++;
    const auto handle = handles_.insert(record.get(), resource_type::texture_view, state->domain());
    if (handle == GRANIT_NULL_HANDLE)
      return GRANIT_ERROR_OUT_OF_MEMORY;
    try {
      texture_views_.emplace(handle, std::move(record));
    } catch (...) {
      static_cast<void>(handles_.erase(handle, resource_type::texture_view, state->domain()));
      throw;
    }
    view = handle;
    return GRANIT_SUCCESS;
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result renderer_registry::destroy_texture_view(granit_renderer renderer,
                                                      granit_texture_view view) {
  std::shared_ptr<texture_view_record> record;
  {
    std::lock_guard lock{mutex_};
    const auto renderer_found = renderers_.find(renderer);
    if (renderer_found == renderers_.end() ||
        handles_.find(renderer, resource_type::renderer, 0) == nullptr)
      return GRANIT_ERROR_INVALID_HANDLE;
    const auto& state = renderer_found->second;
    if (handles_.find(view, resource_type::texture_view, state->domain()) == nullptr)
      return GRANIT_ERROR_INVALID_HANDLE;
    const auto found = texture_views_.find(view);
    if (found == texture_views_.end() || found->second->renderer != state)
      return GRANIT_ERROR_INVALID_HANDLE;
    if (!found->second->publicly_destroyable)
      return GRANIT_ERROR_UNSUPPORTED;
    record = std::move(found->second);
    texture_views_.erase(found);
    static_cast<void>(handles_.erase(view, resource_type::texture_view, state->domain()));
  }
  auto state = record->renderer;
  state->retire_resource(record->metadata.last_use_serial.load(), retirement_order::dependent,
                         record);
  record.reset();
  static_cast<void>(state->collect_retired());
  return GRANIT_SUCCESS;
}

granit_result renderer_registry::destroy_texture(granit_renderer renderer, granit_texture texture) {
  std::shared_ptr<texture_record> record;
  std::vector<std::shared_ptr<texture_view_record>> views;
  lifecycle_snapshot lifecycle;
  {
    std::lock_guard lock{mutex_};
    const auto renderer_found = renderers_.find(renderer);
    if (renderer_found == renderers_.end() ||
        handles_.find(renderer, resource_type::renderer, 0) == nullptr)
      return GRANIT_ERROR_INVALID_HANDLE;
    const auto& state = renderer_found->second;
    if (handles_.find(texture, resource_type::texture, state->domain()) == nullptr)
      return GRANIT_ERROR_INVALID_HANDLE;
    const auto found = textures_.find(texture);
    if (found == textures_.end() || found->second->renderer != state)
      return GRANIT_ERROR_INVALID_HANDLE;
    if (!found->second->publicly_destroyable)
      return GRANIT_ERROR_UNSUPPORTED;
    for (auto view = texture_views_.begin(); view != texture_views_.end();) {
      if (view->second->texture == found->second) {
        if (state->validation_enabled() && view->second->publicly_destroyable) {
          lifecycle.add(lifecycle_resource_type::texture_view, view->first,
                        view->second->metadata.creation_sequence);
        }
        views.push_back(std::move(view->second));
        static_cast<void>(
            handles_.erase(view->first, resource_type::texture_view, state->domain()));
        view = texture_views_.erase(view);
      } else
        ++view;
    }
    record = std::move(found->second);
    textures_.erase(found);
    static_cast<void>(handles_.erase(texture, resource_type::texture, state->domain()));
  }
  auto state = record->renderer;
  write_child_lifecycle_diagnostic(state->diagnostics(), lifecycle_resource_type::texture, texture,
                                   lifecycle_resource_type::texture_view,
                                   lifecycle.summary(lifecycle_resource_type::texture_view));
  for (auto& view : views) {
    state->retire_resource(view->metadata.last_use_serial.load(), retirement_order::dependent,
                           view);
  }
  views.clear();
  state->retire_resource(record->metadata.last_use_serial.load(), retirement_order::resource,
                         record);
  record.reset();
  static_cast<void>(state->collect_retired());
  return GRANIT_SUCCESS;
}

granit_result renderer_registry::create_sampler(granit_renderer renderer,
                                                const granit_sampler_desc& desc,
                                                granit_sampler& sampler) {
  try {
    auto state = acquire(renderer);
    if (!state)
      return GRANIT_ERROR_INVALID_HANDLE;
    auto record = std::make_shared<sampler_record>();
    record->renderer = state;
    record->native = std::make_unique<vulkan_sampler_resource>(state);
    const auto result = state->create_native_sampler(desc, native_sampler(*record->native));
    if (result != GRANIT_SUCCESS)
      return result;
    std::lock_guard lock{mutex_};
    const auto found = renderers_.find(renderer);
    if (found == renderers_.end() || found->second != state)
      return GRANIT_ERROR_INVALID_HANDLE;
    record->metadata.creation_sequence = next_creation_sequence_++;
    const auto handle = handles_.insert(record.get(), resource_type::sampler, state->domain());
    if (handle == GRANIT_NULL_HANDLE)
      return GRANIT_ERROR_OUT_OF_MEMORY;
    try {
      samplers_.emplace(handle, std::move(record));
    } catch (...) {
      static_cast<void>(handles_.erase(handle, resource_type::sampler, state->domain()));
      throw;
    }
    sampler = handle;
    return GRANIT_SUCCESS;
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result renderer_registry::destroy_sampler(granit_renderer renderer, granit_sampler sampler) {
  std::shared_ptr<sampler_record> record;
  {
    std::lock_guard lock{mutex_};
    const auto found_renderer = renderers_.find(renderer);
    if (found_renderer == renderers_.end() ||
        handles_.find(renderer, resource_type::renderer, 0) == nullptr)
      return GRANIT_ERROR_INVALID_HANDLE;
    const auto& state = found_renderer->second;
    if (handles_.find(sampler, resource_type::sampler, state->domain()) == nullptr)
      return GRANIT_ERROR_INVALID_HANDLE;
    const auto found = samplers_.find(sampler);
    if (found == samplers_.end() || found->second->renderer != state)
      return GRANIT_ERROR_INVALID_HANDLE;
    record = std::move(found->second);
    samplers_.erase(found);
    static_cast<void>(handles_.erase(sampler, resource_type::sampler, state->domain()));
  }
  auto state = record->renderer;
  state->retire_resource(record->metadata.last_use_serial.load(), retirement_order::resource,
                         record);
  record.reset();
  static_cast<void>(state->collect_retired());
  return GRANIT_SUCCESS;
}

granit_result renderer_registry::create_shader(granit_renderer renderer, granit_shader_stage stage,
                                               std::span<const std::uint32_t> code,
                                               std::string_view entry_point,
                                               granit_shader& shader) {
  try {
    auto state = acquire(renderer);
    if (!state)
      return GRANIT_ERROR_INVALID_HANDLE;
    auto record = std::make_shared<shader_record>();
    record->renderer = state;
    record->stage = stage;
    record->entry_point.assign(entry_point);
    record->native = std::make_unique<vulkan_shader_resource>(state);
    const auto result = state->create_native_shader(code, native_shader(*record->native));
    if (result != GRANIT_SUCCESS)
      return result;
    std::lock_guard lock{mutex_};
    const auto found = renderers_.find(renderer);
    if (found == renderers_.end() || found->second != state)
      return GRANIT_ERROR_INVALID_HANDLE;
    record->metadata.creation_sequence = next_creation_sequence_++;
    const auto handle = handles_.insert(record.get(), resource_type::shader, state->domain());
    if (handle == GRANIT_NULL_HANDLE)
      return GRANIT_ERROR_OUT_OF_MEMORY;
    try {
      shaders_.emplace(handle, std::move(record));
    } catch (...) {
      static_cast<void>(handles_.erase(handle, resource_type::shader, state->domain()));
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
    const auto found_renderer = renderers_.find(renderer);
    if (found_renderer == renderers_.end() ||
        handles_.find(renderer, resource_type::renderer, 0) == nullptr)
      return GRANIT_ERROR_INVALID_HANDLE;
    const auto& state = found_renderer->second;
    if (handles_.find(shader, resource_type::shader, state->domain()) == nullptr)
      return GRANIT_ERROR_INVALID_HANDLE;
    const auto found = shaders_.find(shader);
    if (found == shaders_.end() || found->second->renderer != state)
      return GRANIT_ERROR_INVALID_HANDLE;
    record = std::move(found->second);
    shaders_.erase(found);
    static_cast<void>(handles_.erase(shader, resource_type::shader, state->domain()));
  }
  auto state = record->renderer;
  state->retire_resource(record->metadata.last_use_serial.load(), retirement_order::resource,
                         record);
  record.reset();
  static_cast<void>(state->collect_retired());
  return GRANIT_SUCCESS;
}

granit_result
renderer_registry::create_bind_group_layout(granit_renderer renderer,
                                            std::span<const granit_bind_group_layout_entry> entries,
                                            granit_bind_group_layout& layout) {
  try {
    auto state = acquire(renderer);
    if (!state)
      return GRANIT_ERROR_INVALID_HANDLE;
    auto record = std::make_shared<bind_group_layout_record>();
    record->renderer = state;
    record->entries.assign(entries.begin(), entries.end());
    record->native = std::make_unique<vulkan_bind_group_layout_resource>(state);
    const auto result =
        state->create_native_bind_group_layout(entries, native_bind_group_layout(*record->native));
    if (result != GRANIT_SUCCESS)
      return result;
    std::lock_guard lock{mutex_};
    if (renderers_.find(renderer) == renderers_.end())
      return GRANIT_ERROR_INVALID_HANDLE;
    record->metadata.creation_sequence = next_creation_sequence_++;
    const auto handle =
        handles_.insert(record.get(), resource_type::bind_group_layout, state->domain());
    if (handle == GRANIT_NULL_HANDLE)
      return GRANIT_ERROR_OUT_OF_MEMORY;
    try {
      bind_group_layouts_.emplace(handle, std::move(record));
    } catch (...) {
      static_cast<void>(handles_.erase(handle, resource_type::bind_group_layout, state->domain()));
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

granit_result renderer_registry::destroy_bind_group_layout(granit_renderer renderer,
                                                           granit_bind_group_layout layout) {
  std::shared_ptr<bind_group_layout_record> record;
  {
    std::lock_guard lock{mutex_};
    const auto state = renderers_.find(renderer);
    if (state == renderers_.end() ||
        handles_.find(layout, resource_type::bind_group_layout, state->second->domain()) == nullptr)
      return GRANIT_ERROR_INVALID_HANDLE;
    const auto found = bind_group_layouts_.find(layout);
    if (found == bind_group_layouts_.end() || found->second->renderer != state->second)
      return GRANIT_ERROR_INVALID_HANDLE;
    record = std::move(found->second);
    bind_group_layouts_.erase(found);
    static_cast<void>(
        handles_.erase(layout, resource_type::bind_group_layout, state->second->domain()));
  }
  const auto state = record->renderer;
  const auto serial = record->metadata.last_use_serial.load();
  state->retire_resource(serial, retirement_order::dependent, std::move(record));
  static_cast<void>(state->collect_retired());
  return GRANIT_SUCCESS;
}

granit_result renderer_registry::create_bind_group(granit_renderer renderer,
                                                   const granit_bind_group_desc& desc,
                                                   granit_bind_group& bind_group) {
  try {
    std::shared_ptr<renderer_state> state;
    std::shared_ptr<bind_group_layout_record> layout;
    auto record = std::make_shared<bind_group_record>();
    std::vector<vulkan_bind_group_write> writes;
    {
      std::lock_guard lock{mutex_};
      const auto renderer_found = renderers_.find(renderer);
      const auto layout_found = bind_group_layouts_.find(desc.layout);
      if (renderer_found == renderers_.end() || layout_found == bind_group_layouts_.end() ||
          layout_found->second->renderer != renderer_found->second)
        return GRANIT_ERROR_INVALID_HANDLE;
      state = renderer_found->second;
      layout = layout_found->second;
      std::uint64_t required_count{};
      for (const auto& declaration : layout->entries)
        required_count += declaration.array_count;
      if (required_count != desc.entry_count)
        return GRANIT_ERROR_INVALID_ARGUMENT;
      writes.reserve(desc.entry_count);
      record->resources.reserve(desc.entry_count);
      for (std::uint32_t index = 0; index < desc.entry_count; ++index) {
        const auto& entry = desc.entries[index];
        const auto declaration =
            std::find_if(layout->entries.begin(), layout->entries.end(),
                         [&](const auto& value) { return value.binding == entry.binding; });
        if (declaration == layout->entries.end() || entry.array_element >= declaration->array_count)
          return GRANIT_ERROR_INVALID_ARGUMENT;
        vulkan_bind_group_write write{.binding = entry.binding,
                                      .array_element = entry.array_element};
        if (declaration->type == GRANIT_BINDING_TYPE_UNIFORM_BUFFER ||
            declaration->type == GRANIT_BINDING_TYPE_STORAGE_BUFFER) {
          const auto found = buffers_.find(entry.resource);
          if (found == buffers_.end() || found->second->renderer != state ||
              entry.offset >= found->second->desc.size || entry.size == 0 ||
              (entry.size != GRANIT_WHOLE_SIZE &&
               entry.size > found->second->desc.size - entry.offset))
            return GRANIT_ERROR_INVALID_ARGUMENT;
          const auto required_usage = declaration->type == GRANIT_BINDING_TYPE_UNIFORM_BUFFER
                                          ? GRANIT_BUFFER_USAGE_UNIFORM_BIT
                                          : GRANIT_BUFFER_USAGE_STORAGE_BIT;
          if ((found->second->desc.usage & required_usage) == 0)
            return GRANIT_ERROR_INVALID_ARGUMENT;
          const auto range = entry.size == GRANIT_WHOLE_SIZE
                                 ? found->second->desc.size - entry.offset
                                 : entry.size;
          const auto binding_type = declaration->type == GRANIT_BINDING_TYPE_UNIFORM_BUFFER
                                        ? backend_buffer_binding_type::uniform
                                        : backend_buffer_binding_type::storage;
          if (!state->capabilities().supports_buffer_binding(binding_type, entry.offset, range))
            return GRANIT_ERROR_INVALID_ARGUMENT;
          write.type = declaration->type == GRANIT_BINDING_TYPE_UNIFORM_BUFFER
                           ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER
                           : VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
          write.buffer = native_buffer(*found->second->native).buffer;
          write.offset = entry.offset;
          write.range = range;
          record->resources.push_back(found->second);
          if ((declaration->visibility &
               (GRANIT_SHADER_STAGE_VERTEX_BIT | GRANIT_SHADER_STAGE_FRAGMENT_BIT)) != 0) {
            const auto access = declaration->type == GRANIT_BINDING_TYPE_UNIFORM_BUFFER
                                    ? VkAccessFlags2{VK_ACCESS_2_UNIFORM_READ_BIT}
                                    : VkAccessFlags2{VK_ACCESS_2_SHADER_STORAGE_READ_BIT |
                                                     VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT};
            record->graphics_buffer_accesses.emplace_back(
                native_buffer(*found->second->native).buffer, access);
          }
          if ((declaration->visibility & GRANIT_SHADER_STAGE_COMPUTE_BIT) != 0) {
            const auto access = declaration->type == GRANIT_BINDING_TYPE_UNIFORM_BUFFER
                                    ? VkAccessFlags2{VK_ACCESS_2_UNIFORM_READ_BIT}
                                    : VkAccessFlags2{VK_ACCESS_2_SHADER_STORAGE_READ_BIT |
                                                     VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT};
            record->compute_buffer_accesses.emplace_back(
                native_buffer(*found->second->native).buffer, access);
          }
        } else if (declaration->type == GRANIT_BINDING_TYPE_SAMPLER) {
          const auto found = samplers_.find(entry.resource);
          if (found == samplers_.end() || found->second->renderer != state || entry.offset != 0 ||
              entry.size != GRANIT_WHOLE_SIZE)
            return GRANIT_ERROR_INVALID_ARGUMENT;
          write.type = VK_DESCRIPTOR_TYPE_SAMPLER;
          write.sampler = native_sampler(*found->second->native);
          record->resources.push_back(found->second);
        } else {
          const auto found = texture_views_.find(entry.resource);
          if (found == texture_views_.end() || found->second->renderer != state ||
              entry.offset != 0 || entry.size != GRANIT_WHOLE_SIZE)
            return GRANIT_ERROR_INVALID_ARGUMENT;
          const auto required_usage = declaration->type == GRANIT_BINDING_TYPE_SAMPLED_TEXTURE
                                          ? GRANIT_TEXTURE_USAGE_SAMPLED_BIT
                                          : GRANIT_TEXTURE_USAGE_STORAGE_BIT;
          if ((found->second->texture->desc.usage & required_usage) == 0)
            return GRANIT_ERROR_INVALID_ARGUMENT;
          write.type = declaration->type == GRANIT_BINDING_TYPE_SAMPLED_TEXTURE
                           ? VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE
                           : VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
          write.image_view = native_texture_view(*found->second->native);
          record->resources.push_back(found->second);
          const auto make_access = [&] {
            auto aspect = map_aspect(found->second->desc.range.aspect);
            if (found->second->desc.range.aspect == GRANIT_TEXTURE_ASPECT_AUTOMATIC) {
              aspect = depth_format(found->second->texture->desc.format)
                           ? VK_IMAGE_ASPECT_DEPTH_BIT
                           : VK_IMAGE_ASPECT_COLOR_BIT;
              if (stencil_format(found->second->texture->desc.format))
                aspect |= VK_IMAGE_ASPECT_STENCIL_BIT;
            }
            const bool storage = declaration->type == GRANIT_BINDING_TYPE_STORAGE_TEXTURE;
            return vulkan_image_access{
                .image = native_texture(*found->second->texture->native).image,
                .range = {.aspectMask = aspect,
                          .baseMipLevel = found->second->desc.range.base_mip_level,
                          .levelCount = found->second->desc.range.mip_level_count,
                          .baseArrayLayer = found->second->desc.range.base_array_layer,
                          .layerCount = found->second->desc.range.array_layer_count},
                .layout =
                    storage ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                .access = storage ? VkAccessFlags2{VK_ACCESS_2_SHADER_STORAGE_READ_BIT |
                                                   VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT}
                                  : VkAccessFlags2{VK_ACCESS_2_SHADER_SAMPLED_READ_BIT},
                .preserve_content = false};
          };
          if ((declaration->visibility &
               (GRANIT_SHADER_STAGE_VERTEX_BIT | GRANIT_SHADER_STAGE_FRAGMENT_BIT)) != 0) {
            auto access = make_access();
            access.stages =
                VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
            record->graphics_image_accesses.push_back(access);
          }
          if ((declaration->visibility & GRANIT_SHADER_STAGE_COMPUTE_BIT) != 0) {
            auto access = make_access();
            access.stages = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
            record->compute_image_accesses.push_back(access);
          }
        }
        writes.push_back(write);
      }
    }
    record->renderer = state;
    record->layout = layout;
    record->native = std::make_unique<vulkan_bind_group_resource>(state);
    auto& native = native_bind_group(*record->native);
    const auto result = state->create_native_bind_group(native_bind_group_layout(*layout->native),
                                                        writes, native.pool(), native.set());
    if (result != GRANIT_SUCCESS)
      return result;
    std::lock_guard lock{mutex_};
    if (bind_group_layouts_.find(desc.layout) == bind_group_layouts_.end())
      return GRANIT_ERROR_INVALID_HANDLE;
    record->metadata.creation_sequence = next_creation_sequence_++;
    const auto handle = handles_.insert(record.get(), resource_type::bind_group, state->domain());
    if (handle == GRANIT_NULL_HANDLE)
      return GRANIT_ERROR_OUT_OF_MEMORY;
    try {
      bind_groups_.emplace(handle, std::move(record));
    } catch (...) {
      static_cast<void>(handles_.erase(handle, resource_type::bind_group, state->domain()));
      throw;
    }
    bind_group = handle;
    return GRANIT_SUCCESS;
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result renderer_registry::destroy_bind_group(granit_renderer renderer,
                                                    granit_bind_group bind_group) {
  std::shared_ptr<bind_group_record> record;
  {
    std::lock_guard lock{mutex_};
    const auto state = renderers_.find(renderer);
    if (state == renderers_.end() ||
        handles_.find(bind_group, resource_type::bind_group, state->second->domain()) == nullptr)
      return GRANIT_ERROR_INVALID_HANDLE;
    const auto found = bind_groups_.find(bind_group);
    if (found == bind_groups_.end() || found->second->renderer != state->second)
      return GRANIT_ERROR_INVALID_HANDLE;
    record = std::move(found->second);
    bind_groups_.erase(found);
    static_cast<void>(
        handles_.erase(bind_group, resource_type::bind_group, state->second->domain()));
  }
  const auto state = record->renderer;
  const auto serial = record->metadata.last_use_serial.load();
  state->retire_resource(serial, retirement_order::dependent, std::move(record));
  static_cast<void>(state->collect_retired());
  return GRANIT_SUCCESS;
}

granit_result renderer_registry::create_pipeline_layout(
    granit_renderer renderer, std::span<const granit_bind_group_layout> bind_group_layouts,
    granit_pipeline_layout& layout) {
  try {
    auto state = acquire(renderer);
    if (!state)
      return GRANIT_ERROR_INVALID_HANDLE;
    auto record = std::make_shared<pipeline_layout_record>();
    record->renderer = state;
    std::vector<VkDescriptorSetLayout> native_layouts;
    {
      std::lock_guard lock{mutex_};
      for (const auto handle : bind_group_layouts) {
        const auto found = bind_group_layouts_.find(handle);
        if (found == bind_group_layouts_.end() || found->second->renderer != state)
          return GRANIT_ERROR_INVALID_HANDLE;
        record->bind_group_layouts.push_back(found->second);
        native_layouts.push_back(native_bind_group_layout(*found->second->native));
      }
    }
    record->native = std::make_unique<vulkan_pipeline_layout_resource>(state);
    const auto result = state->create_native_pipeline_layout(
        native_layouts, native_pipeline_layout(*record->native));
    if (result != GRANIT_SUCCESS)
      return result;
    std::lock_guard lock{mutex_};
    if (renderers_.find(renderer) == renderers_.end())
      return GRANIT_ERROR_INVALID_HANDLE;
    record->metadata.creation_sequence = next_creation_sequence_++;
    const auto handle =
        handles_.insert(record.get(), resource_type::pipeline_layout, state->domain());
    if (handle == GRANIT_NULL_HANDLE)
      return GRANIT_ERROR_OUT_OF_MEMORY;
    try {
      pipeline_layouts_.emplace(handle, std::move(record));
    } catch (...) {
      static_cast<void>(handles_.erase(handle, resource_type::pipeline_layout, state->domain()));
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
    if (state == renderers_.end() ||
        handles_.find(layout, resource_type::pipeline_layout, state->second->domain()) == nullptr)
      return GRANIT_ERROR_INVALID_HANDLE;
    const auto found = pipeline_layouts_.find(layout);
    if (found == pipeline_layouts_.end() || found->second->renderer != state->second)
      return GRANIT_ERROR_INVALID_HANDLE;
    record = std::move(found->second);
    pipeline_layouts_.erase(found);
    static_cast<void>(
        handles_.erase(layout, resource_type::pipeline_layout, state->second->domain()));
  }
  const auto state = record->renderer;
  const auto serial = record->metadata.last_use_serial.load();
  state->retire_resource(serial, retirement_order::dependent, std::move(record));
  static_cast<void>(state->collect_retired());
  return GRANIT_SUCCESS;
}

granit_result renderer_registry::create_graphics_pipeline(granit_renderer renderer,
                                                          const granit_graphics_pipeline_desc& desc,
                                                          granit_graphics_pipeline& pipeline) {
  try {
    std::shared_ptr<renderer_state> state;
    std::shared_ptr<pipeline_layout_record> layout;
    std::shared_ptr<shader_record> vertex;
    std::shared_ptr<shader_record> fragment;
    {
      std::lock_guard lock{mutex_};
      const auto found = renderers_.find(renderer);
      if (found == renderers_.end())
        return GRANIT_ERROR_INVALID_HANDLE;
      state = found->second;
      const auto layout_found = pipeline_layouts_.find(desc.layout);
      const auto vertex_found = shaders_.find(desc.vertex_shader);
      const auto fragment_found = shaders_.find(desc.fragment_shader);
      if (layout_found == pipeline_layouts_.end() || vertex_found == shaders_.end() ||
          fragment_found == shaders_.end() || layout_found->second->renderer != state ||
          vertex_found->second->renderer != state || fragment_found->second->renderer != state ||
          vertex_found->second->stage != GRANIT_SHADER_STAGE_VERTEX ||
          fragment_found->second->stage != GRANIT_SHADER_STAGE_FRAGMENT)
        return GRANIT_ERROR_INVALID_HANDLE;
      layout = layout_found->second;
      vertex = vertex_found->second;
      fragment = fragment_found->second;
    }
    auto record = std::make_shared<graphics_pipeline_record>();
    record->renderer = state;
    record->layout = layout;
    record->vertex_shader = vertex;
    record->fragment_shader = fragment;
    record->native = std::make_unique<vulkan_graphics_pipeline_resource>(state);
    const auto vertex_buffers =
        desc.struct_size >= GRANIT_GRAPHICS_PIPELINE_DESC_VERSION_2_SIZE
            ? std::span<const granit_vertex_buffer_layout>{desc.vertex_buffer_layouts,
                                                           desc.vertex_buffer_layout_count}
            : std::span<const granit_vertex_buffer_layout>{};
    granit_depth_state depth{desc.depth_stencil_format != GRANIT_TEXTURE_FORMAT_UNDEFINED,
                             desc.depth_stencil_format != GRANIT_TEXTURE_FORMAT_UNDEFINED,
                             GRANIT_COMPARE_OPERATION_LESS_EQUAL, 0};
    std::array<granit_color_blend_state, 8> default_blends{};
    for (std::size_t index = 0; index < desc.color_format_count; ++index)
      default_blends[index] = {0,
                               GRANIT_BLEND_FACTOR_ONE,
                               GRANIT_BLEND_FACTOR_ZERO,
                               GRANIT_BLEND_OPERATION_ADD,
                               GRANIT_BLEND_FACTOR_ONE,
                               GRANIT_BLEND_FACTOR_ZERO,
                               GRANIT_BLEND_OPERATION_ADD,
                               GRANIT_COLOR_WRITE_ALL_BITS};
    std::span<const granit_color_blend_state> color_blends{default_blends.data(),
                                                           desc.color_format_count};
    if (desc.struct_size >= GRANIT_GRAPHICS_PIPELINE_DESC_VERSION_4_SIZE) {
      if (desc.depth)
        depth = *desc.depth;
      if (desc.color_blend_count != 0)
        color_blends = {desc.color_blends, desc.color_blend_count};
    }
    const auto result = state->create_native_graphics_pipeline(
        native_pipeline_layout(*layout->native), native_shader(*vertex->native),
        vertex->entry_point.c_str(), native_shader(*fragment->native),
        fragment->entry_point.c_str(), vertex_buffers,
        desc.struct_size >= GRANIT_GRAPHICS_PIPELINE_DESC_VERSION_3_SIZE
            ? desc.primitive
            : granit_primitive_state{GRANIT_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
                                     GRANIT_FRONT_FACE_COUNTER_CLOCKWISE, GRANIT_CULL_MODE_NONE,
                                     GRANIT_POLYGON_MODE_FILL},
        depth,
        desc.struct_size >= GRANIT_GRAPHICS_PIPELINE_DESC_VERSION_5_SIZE ? desc.depth_bias
                                                                         : nullptr,
        color_blends, {desc.color_formats, static_cast<std::size_t>(desc.color_format_count)},
        desc.depth_stencil_format, desc.sample_count, native_graphics_pipeline(*record->native));
    if (result != GRANIT_SUCCESS)
      return result;
    std::lock_guard lock{mutex_};
    if (pipeline_layouts_.find(desc.layout) == pipeline_layouts_.end() ||
        shaders_.find(desc.vertex_shader) == shaders_.end() ||
        shaders_.find(desc.fragment_shader) == shaders_.end())
      return GRANIT_ERROR_INVALID_HANDLE;
    record->metadata.creation_sequence = next_creation_sequence_++;
    const auto handle = handles_.insert(record.get(), resource_type::pipeline, state->domain());
    if (handle == GRANIT_NULL_HANDLE)
      return GRANIT_ERROR_OUT_OF_MEMORY;
    try {
      graphics_pipelines_.emplace(handle, std::move(record));
    } catch (...) {
      static_cast<void>(handles_.erase(handle, resource_type::pipeline, state->domain()));
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
    if (state == renderers_.end() ||
        handles_.find(pipeline, resource_type::pipeline, state->second->domain()) == nullptr)
      return GRANIT_ERROR_INVALID_HANDLE;
    const auto found = graphics_pipelines_.find(pipeline);
    if (found == graphics_pipelines_.end() || found->second->renderer != state->second)
      return GRANIT_ERROR_INVALID_HANDLE;
    record = std::move(found->second);
    graphics_pipelines_.erase(found);
    static_cast<void>(handles_.erase(pipeline, resource_type::pipeline, state->second->domain()));
  }
  const auto state = record->renderer;
  const auto serial = record->metadata.last_use_serial.load();
  state->retire_resource(serial, retirement_order::dependent, std::move(record));
  static_cast<void>(state->collect_retired());
  return GRANIT_SUCCESS;
}

granit_result renderer_registry::create_compute_pipeline(granit_renderer renderer,
                                                         const granit_compute_pipeline_desc& desc,
                                                         granit_compute_pipeline& pipeline) {
  try {
    std::shared_ptr<renderer_state> state;
    std::shared_ptr<pipeline_layout_record> layout;
    std::shared_ptr<shader_record> compute;
    {
      std::lock_guard lock{mutex_};
      const auto found = renderers_.find(renderer);
      if (found == renderers_.end())
        return GRANIT_ERROR_INVALID_HANDLE;
      state = found->second;
      const auto layout_found = pipeline_layouts_.find(desc.layout);
      const auto compute_found = shaders_.find(desc.compute_shader);
      if (layout_found == pipeline_layouts_.end() || compute_found == shaders_.end() ||
          layout_found->second->renderer != state || compute_found->second->renderer != state ||
          compute_found->second->stage != GRANIT_SHADER_STAGE_COMPUTE)
        return GRANIT_ERROR_INVALID_HANDLE;
      layout = layout_found->second;
      compute = compute_found->second;
    }
    auto record = std::make_shared<compute_pipeline_record>();
    record->renderer = state;
    record->layout = layout;
    record->compute_shader = compute;
    record->native = std::make_unique<vulkan_compute_pipeline_resource>(state);
    const auto result = state->create_native_compute_pipeline(
        native_pipeline_layout(*layout->native), native_shader(*compute->native),
        compute->entry_point.c_str(), native_compute_pipeline(*record->native));
    if (result != GRANIT_SUCCESS)
      return result;
    std::lock_guard lock{mutex_};
    if (pipeline_layouts_.find(desc.layout) == pipeline_layouts_.end() ||
        shaders_.find(desc.compute_shader) == shaders_.end())
      return GRANIT_ERROR_INVALID_HANDLE;
    record->metadata.creation_sequence = next_creation_sequence_++;
    const auto handle =
        handles_.insert(record.get(), resource_type::compute_pipeline, state->domain());
    if (handle == GRANIT_NULL_HANDLE)
      return GRANIT_ERROR_OUT_OF_MEMORY;
    try {
      compute_pipelines_.emplace(handle, std::move(record));
    } catch (...) {
      static_cast<void>(handles_.erase(handle, resource_type::compute_pipeline, state->domain()));
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

granit_result renderer_registry::destroy_compute_pipeline(granit_renderer renderer,
                                                          granit_compute_pipeline pipeline) {
  std::shared_ptr<compute_pipeline_record> record;
  {
    std::lock_guard lock{mutex_};
    const auto state = renderers_.find(renderer);
    if (state == renderers_.end() || handles_.find(pipeline, resource_type::compute_pipeline,
                                                   state->second->domain()) == nullptr)
      return GRANIT_ERROR_INVALID_HANDLE;
    const auto found = compute_pipelines_.find(pipeline);
    if (found == compute_pipelines_.end() || found->second->renderer != state->second)
      return GRANIT_ERROR_INVALID_HANDLE;
    record = std::move(found->second);
    compute_pipelines_.erase(found);
    static_cast<void>(
        handles_.erase(pipeline, resource_type::compute_pipeline, state->second->domain()));
  }
  const auto state = record->renderer;
  const auto serial = record->metadata.last_use_serial.load();
  state->retire_resource(serial, retirement_order::dependent, std::move(record));
  static_cast<void>(state->collect_retired());
  return GRANIT_SUCCESS;
}

granit_result renderer_registry::create_command_recorder(granit_renderer renderer,
                                                         granit_command_recorder& recorder) {
  try {
    auto state = acquire(renderer);
    if (!state) {
      return GRANIT_ERROR_INVALID_HANDLE;
    }
    auto record = std::make_shared<command_recorder_record>();
    record->renderer = state;
    record->native = std::make_unique<vulkan_command_recorder_resource>(state);
    const auto result =
        state->create_native_command_recorder(native_command_recorder(*record->native));
    if (result != GRANIT_SUCCESS) {
      return result;
    }
    std::lock_guard lock{mutex_};
    const auto found = renderers_.find(renderer);
    if (found == renderers_.end() || found->second != state) {
      return GRANIT_ERROR_INVALID_HANDLE;
    }
    record->metadata.creation_sequence = next_creation_sequence_++;
    const auto handle =
        handles_.insert(record.get(), resource_type::command_recorder, state->domain());
    if (handle == GRANIT_NULL_HANDLE) {
      return GRANIT_ERROR_OUT_OF_MEMORY;
    }
    try {
      command_recorders_.emplace(handle, std::move(record));
    } catch (...) {
      static_cast<void>(handles_.erase(handle, resource_type::command_recorder, state->domain()));
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
  auto record = acquire_command_recorder(renderer, recorder);
  if (!record) {
    return GRANIT_ERROR_INVALID_HANDLE;
  }
  std::lock_guard record_lock{record->mutex};
  return record->renderer->begin_command_recorder(native_command_recorder(*record->native));
}

granit_result renderer_registry::end_command_recorder(granit_renderer renderer,
                                                      granit_command_recorder recorder) {
  auto record = acquire_command_recorder(renderer, recorder);
  if (!record) {
    return GRANIT_ERROR_INVALID_HANDLE;
  }
  std::lock_guard record_lock{record->mutex};
  return record->renderer->end_command_recorder(native_command_recorder(*record->native));
}

granit_result renderer_registry::submit_command_recorder(granit_renderer renderer,
                                                         granit_command_recorder recorder) {
  auto record = acquire_command_recorder(renderer, recorder);
  if (!record)
    return GRANIT_ERROR_INVALID_HANDLE;
  std::lock_guard record_lock{record->mutex};
  submission_serial serial{};
  const auto result = record->renderer->submit_command_recorder(*record->native, serial);
  if (result == GRANIT_SUCCESS)
    mark_resources_used(record->retained_resources, serial);
  return result;
}

granit_result
renderer_registry::submit_command_recorders(granit_renderer renderer,
                                            std::span<const granit_command_recorder> recorders) {
  if (recorders.empty())
    return GRANIT_ERROR_INVALID_ARGUMENT;
  std::vector<std::shared_ptr<command_recorder_record>> records;
  records.reserve(recorders.size());
  std::shared_ptr<renderer_state> state;
  {
    std::lock_guard lock{mutex_};
    const auto found_renderer = renderers_.find(renderer);
    if (found_renderer == renderers_.end() ||
        handles_.find(renderer, resource_type::renderer, 0) == nullptr) {
      return GRANIT_ERROR_INVALID_HANDLE;
    }
    state = found_renderer->second;
    for (const auto handle : recorders) {
      if (handle == GRANIT_NULL_HANDLE ||
          handles_.find(handle, resource_type::command_recorder, state->domain()) == nullptr) {
        return GRANIT_ERROR_INVALID_HANDLE;
      }
      const auto found = command_recorders_.find(handle);
      if (found == command_recorders_.end() || found->second->renderer != state)
        return GRANIT_ERROR_INVALID_HANDLE;
      if (std::find(records.begin(), records.end(), found->second) != records.end())
        return GRANIT_ERROR_INVALID_ARGUMENT;
      records.push_back(found->second);
    }
  }
  std::vector<command_recorder_record*> lock_order;
  lock_order.reserve(records.size());
  for (const auto& record : records)
    lock_order.push_back(record.get());
  std::sort(lock_order.begin(), lock_order.end(), std::less<>{});
  std::vector<std::unique_lock<std::mutex>> record_locks;
  record_locks.reserve(lock_order.size());
  for (auto* record : lock_order)
    record_locks.emplace_back(record->mutex);
  std::vector<backend_command_recorder_resource*> native_recorders;
  native_recorders.reserve(records.size());
  for (const auto& record : records)
    native_recorders.push_back(record->native.get());
  submission_serial serial{};
  const auto result = state->submit_command_recorders(native_recorders, serial);
  if (result == GRANIT_SUCCESS) {
    for (const auto& record : records)
      mark_resources_used(record->retained_resources, serial);
  }
  return result;
}

granit_result renderer_registry::reset_command_recorder(granit_renderer renderer,
                                                        granit_command_recorder recorder) {
  auto record = acquire_command_recorder(renderer, recorder);
  if (!record) {
    return GRANIT_ERROR_INVALID_HANDLE;
  }
  std::lock_guard record_lock{record->mutex};
  const auto wait_result = record->renderer->wait_command_recorder(*record->native);
  if (wait_result != GRANIT_SUCCESS) {
    return wait_result;
  }
  const auto result =
      record->renderer->reset_command_recorder(native_command_recorder(*record->native));
  if (result == GRANIT_SUCCESS) {
    record->retained_resources.clear();
    static_cast<void>(record->renderer->collect_retired());
  }
  return result;
}

granit_result renderer_registry::create_frame_context(granit_renderer renderer,
                                                      granit_frame_context& context) {
  auto state = acquire(renderer);
  if (!state)
    return GRANIT_ERROR_INVALID_HANDLE;
  auto record = std::make_shared<frame_context_record>();
  record->renderer = state;
  record->slots.resize(state->frame_slot_count());
  for (auto& slot : record->slots) {
    const auto result = create_command_recorder(renderer, slot.recorder);
    if (result != GRANIT_SUCCESS) {
      for (auto& created : record->slots) {
        if (created.recorder != GRANIT_NULL_HANDLE)
          static_cast<void>(destroy_command_recorder(renderer, created.recorder));
      }
      return result;
    }
  }
  granit_result install_result{GRANIT_SUCCESS};
  {
    std::lock_guard lock{mutex_};
    const auto found = renderers_.find(renderer);
    if (found == renderers_.end() || found->second != state) {
      install_result = GRANIT_ERROR_INVALID_HANDLE;
    } else {
      record->metadata.creation_sequence = next_creation_sequence_++;
      const auto handle =
          handles_.insert(record.get(), resource_type::frame_context, state->domain());
      if (handle == GRANIT_NULL_HANDLE) {
        install_result = GRANIT_ERROR_OUT_OF_MEMORY;
      } else {
        for (const auto& slot : record->slots)
          command_recorders_.at(slot.recorder)->owned_by_frame_context = true;
        try {
          frame_contexts_.emplace(handle, record);
          context = handle;
        } catch (...) {
          static_cast<void>(handles_.erase(handle, resource_type::frame_context, state->domain()));
          for (const auto& slot : record->slots)
            command_recorders_.at(slot.recorder)->owned_by_frame_context = false;
          install_result = GRANIT_ERROR_OUT_OF_MEMORY;
        }
      }
    }
  }
  if (install_result != GRANIT_SUCCESS) {
    for (const auto& slot : record->slots)
      static_cast<void>(destroy_command_recorder(renderer, slot.recorder));
  }
  return install_result;
}

granit_result renderer_registry::begin_frame_context(granit_renderer renderer,
                                                     granit_frame_context context,
                                                     granit_frame frame,
                                                     granit_command_recorder& recorder,
                                                     std::uint32_t& frame_slot) {
  std::shared_ptr<frame_context_record> context_record;
  std::shared_ptr<frame_record> frame_record_state;
  {
    std::lock_guard lock{mutex_};
    const auto renderer_found = renderers_.find(renderer);
    const auto context_found = frame_contexts_.find(context);
    const auto frame_found = frames_.find(frame);
    if (renderer_found == renderers_.end() || context_found == frame_contexts_.end() ||
        frame_found == frames_.end() || context_found->second->renderer != renderer_found->second ||
        frame_found->second->renderer != renderer_found->second)
      return GRANIT_ERROR_INVALID_HANDLE;
    context_record = context_found->second;
    frame_record_state = frame_found->second;
  }
  std::size_t slot_index{};
  {
    std::lock_guard frame_lock{frame_record_state->mutex};
    if (frame_record_state->submitted)
      return GRANIT_ERROR_INVALID_ARGUMENT;
    slot_index = frame_record_state->slot_index;
  }
  std::lock_guard context_lock{context_record->mutex};
  if (slot_index >= context_record->slots.size())
    return GRANIT_ERROR_INVALID_ARGUMENT;
  auto& slot = context_record->slots[slot_index];
  if (slot.state == frame_context_slot_state::recording)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  if (slot.state == frame_context_slot_state::submitted) {
    const auto reset_result = reset_command_recorder(renderer, slot.recorder);
    if (reset_result != GRANIT_SUCCESS)
      return reset_result;
    slot.state = frame_context_slot_state::idle;
  }
  const auto begin_result = begin_command_recorder(renderer, slot.recorder);
  if (begin_result != GRANIT_SUCCESS)
    return begin_result;
  slot.frame = frame;
  slot.state = frame_context_slot_state::recording;
  recorder = slot.recorder;
  frame_slot = static_cast<std::uint32_t>(slot_index);
  return GRANIT_SUCCESS;
}

granit_result renderer_registry::submit_frame_context(granit_renderer renderer,
                                                      granit_frame_context context,
                                                      granit_frame frame) {
  std::shared_ptr<frame_context_record> record;
  {
    std::lock_guard lock{mutex_};
    const auto found = frame_contexts_.find(context);
    const auto renderer_found = renderers_.find(renderer);
    if (found == frame_contexts_.end() || renderer_found == renderers_.end() ||
        found->second->renderer != renderer_found->second)
      return GRANIT_ERROR_INVALID_HANDLE;
    record = found->second;
  }
  std::lock_guard lock{record->mutex};
  const auto slot = std::find_if(record->slots.begin(), record->slots.end(), [&](const auto& item) {
    return item.state == frame_context_slot_state::recording && item.frame == frame;
  });
  if (slot == record->slots.end())
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const auto end_result = end_command_recorder(renderer, slot->recorder);
  if (end_result != GRANIT_SUCCESS)
    return end_result;
  const auto submit_result = submit_command_recorder_frame(renderer, slot->recorder, frame);
  if (submit_result != GRANIT_SUCCESS)
    return submit_result;
  slot->state = frame_context_slot_state::submitted;
  return GRANIT_SUCCESS;
}

granit_result renderer_registry::abort_frame_context(granit_renderer renderer,
                                                     granit_frame_context context,
                                                     granit_frame frame) {
  std::shared_ptr<frame_context_record> context_record;
  std::shared_ptr<command_recorder_record> command;
  frame_context_slot* slot{};
  {
    std::lock_guard lock{mutex_};
    const auto found = frame_contexts_.find(context);
    const auto renderer_found = renderers_.find(renderer);
    if (found == frame_contexts_.end() || renderer_found == renderers_.end() ||
        found->second->renderer != renderer_found->second)
      return GRANIT_ERROR_INVALID_HANDLE;
    context_record = found->second;
  }
  std::lock_guard context_lock{context_record->mutex};
  const auto found_slot = std::find_if(
      context_record->slots.begin(), context_record->slots.end(), [&](const auto& item) {
        return item.state == frame_context_slot_state::recording && item.frame == frame;
      });
  if (found_slot == context_record->slots.end())
    return GRANIT_ERROR_INVALID_ARGUMENT;
  slot = &*found_slot;
  command = acquire_command_recorder(renderer, slot->recorder);
  if (!command)
    return GRANIT_ERROR_INVALID_HANDLE;
  std::lock_guard command_lock{command->mutex};
  command->renderer->destroy_native_command_recorder(native_command_recorder(*command->native));
  command->retained_resources.clear();
  const auto result =
      command->renderer->create_native_command_recorder(native_command_recorder(*command->native));
  if (result == GRANIT_SUCCESS) {
    slot->frame = GRANIT_NULL_HANDLE;
    slot->state = frame_context_slot_state::idle;
  }
  return result;
}

granit_result renderer_registry::destroy_frame_context(granit_renderer renderer,
                                                       granit_frame_context context) {
  std::shared_ptr<frame_context_record> record;
  {
    std::lock_guard lock{mutex_};
    const auto found = frame_contexts_.find(context);
    const auto renderer_found = renderers_.find(renderer);
    if (found == frame_contexts_.end() || renderer_found == renderers_.end() ||
        found->second->renderer != renderer_found->second)
      return GRANIT_ERROR_INVALID_HANDLE;
    record = std::move(found->second);
    frame_contexts_.erase(found);
    static_cast<void>(
        handles_.erase(context, resource_type::frame_context, renderer_found->second->domain()));
    for (const auto& slot : record->slots) {
      const auto command = command_recorders_.find(slot.recorder);
      if (command != command_recorders_.end())
        command->second->owned_by_frame_context = false;
    }
  }
  granit_result result = GRANIT_SUCCESS;
  for (const auto& slot : record->slots) {
    const auto destroy_result = destroy_command_recorder(renderer, slot.recorder);
    if (result == GRANIT_SUCCESS && destroy_result != GRANIT_SUCCESS)
      result = destroy_result;
  }
  return result;
}

granit_result renderer_registry::copy_buffer(granit_renderer renderer,
                                             granit_command_recorder recorder, granit_buffer source,
                                             granit_buffer destination,
                                             std::span<const granit_buffer_copy_region> regions) {
  auto recorder_record = acquire_command_recorder(renderer, recorder);
  if (!recorder_record) {
    return GRANIT_ERROR_INVALID_HANDLE;
  }
  std::shared_ptr<buffer_record> source_record;
  std::shared_ptr<buffer_record> destination_record;
  {
    std::lock_guard lock{mutex_};
    const auto& state = recorder_record->renderer;
    if (handles_.find(source, resource_type::buffer, state->domain()) == nullptr ||
        handles_.find(destination, resource_type::buffer, state->domain()) == nullptr) {
      return GRANIT_ERROR_INVALID_HANDLE;
    }
    const auto found_source = buffers_.find(source);
    const auto found_destination = buffers_.find(destination);
    if (found_source == buffers_.end() || found_destination == buffers_.end() ||
        found_source->second->renderer != state || found_destination->second->renderer != state) {
      return GRANIT_ERROR_INVALID_HANDLE;
    }
    source_record = found_source->second;
    destination_record = found_destination->second;
  }
  if ((source_record->desc.usage & GRANIT_BUFFER_USAGE_TRANSFER_SOURCE_BIT) == 0 ||
      (destination_record->desc.usage & GRANIT_BUFFER_USAGE_TRANSFER_DESTINATION_BIT) == 0) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  std::vector<VkBufferCopy> native_regions;
  native_regions.reserve(regions.size());
  for (const auto& region : regions) {
    if (region.size == 0 || region.source_offset >= source_record->desc.size ||
        region.size > source_record->desc.size - region.source_offset ||
        region.destination_offset >= destination_record->desc.size ||
        region.size > destination_record->desc.size - region.destination_offset) {
      return GRANIT_ERROR_INVALID_ARGUMENT;
    }
    native_regions.push_back({.srcOffset = region.source_offset,
                              .dstOffset = region.destination_offset,
                              .size = region.size});
  }
  if (source_record == destination_record) {
    for (const auto& source_region : regions) {
      for (const auto& destination_region : regions) {
        if (ranges_overlap(source_region.source_offset, source_region.size,
                           destination_region.destination_offset, destination_region.size)) {
          return GRANIT_ERROR_INVALID_ARGUMENT;
        }
      }
    }
  }

  std::lock_guard record_lock{recorder_record->mutex};
  if (native_command_recorder(*recorder_record->native).state() !=
      command_recorder_state::recording) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  retain_resource(recorder_record->retained_resources, source_record, source_record->metadata);
  retain_resource(recorder_record->retained_resources, destination_record,
                  destination_record->metadata);
  return recorder_record->renderer->copy_buffer(native_command_recorder(*recorder_record->native),
                                                native_buffer(*source_record->native).buffer,
                                                native_buffer(*destination_record->native).buffer,
                                                native_regions);
}

granit_result renderer_registry::copy_texture_to_buffer(granit_renderer renderer,
                                                        granit_command_recorder recorder,
                                                        granit_texture source,
                                                        granit_buffer destination,
                                                        const granit_texture_data_layout& layout,
                                                        const granit_texture_write_region& region) {
  auto recorder_record = acquire_command_recorder(renderer, recorder);
  if (!recorder_record)
    return GRANIT_ERROR_INVALID_HANDLE;
  std::shared_ptr<texture_record> source_record;
  std::shared_ptr<buffer_record> destination_record;
  {
    std::lock_guard lock{mutex_};
    const auto& state = recorder_record->renderer;
    if (handles_.find(source, resource_type::texture, state->domain()) == nullptr ||
        handles_.find(destination, resource_type::buffer, state->domain()) == nullptr) {
      return GRANIT_ERROR_INVALID_HANDLE;
    }
    const auto found_source = textures_.find(source);
    const auto found_destination = buffers_.find(destination);
    if (found_source == textures_.end() || found_destination == buffers_.end() ||
        found_source->second->renderer != state || found_destination->second->renderer != state) {
      return GRANIT_ERROR_INVALID_HANDLE;
    }
    source_record = found_source->second;
    destination_record = found_destination->second;
  }

  const auto& desc = source_record->desc;
  const auto bytes_per_pixel =
      depth_format(desc.format) ? 0 : texture_format_bytes_per_block(desc.format);
  if ((desc.usage & GRANIT_TEXTURE_USAGE_TRANSFER_SOURCE_BIT) == 0 ||
      (destination_record->desc.usage & GRANIT_BUFFER_USAGE_TRANSFER_DESTINATION_BIT) == 0 ||
      desc.sample_count != GRANIT_SAMPLE_COUNT_1 || bytes_per_pixel == 0) {
    return GRANIT_ERROR_UNSUPPORTED;
  }
  if (region.aspect != GRANIT_TEXTURE_ASPECT_COLOR_BIT || region.width == 0 || region.height == 0 ||
      region.depth == 0 || region.array_layer_count == 0 || region.mip_level >= desc.mip_levels ||
      region.base_array_layer >= desc.array_layers ||
      region.array_layer_count > desc.array_layers - region.base_array_layer ||
      layout.offset % 4 != 0) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  const auto mip_width = std::max(UINT32_C(1), desc.width >> region.mip_level);
  const auto mip_height = std::max(UINT32_C(1), desc.height >> region.mip_level);
  const auto mip_depth = std::max(UINT32_C(1), desc.depth >> region.mip_level);
  if (region.x >= mip_width || region.width > mip_width - region.x || region.y >= mip_height ||
      region.height > mip_height - region.y || region.z >= mip_depth ||
      region.depth > mip_depth - region.z) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  const std::uint64_t tight_row = std::uint64_t{region.width} * bytes_per_pixel;
  const std::uint64_t row_pitch = layout.bytes_per_row == 0 ? tight_row : layout.bytes_per_row;
  const std::uint64_t image_rows =
      layout.rows_per_image == 0 ? region.height : layout.rows_per_image;
  if (row_pitch < tight_row || row_pitch % bytes_per_pixel != 0 || image_rows < region.height)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const std::uint64_t image_count =
      desc.dimension == GRANIT_TEXTURE_DIMENSION_3D ? region.depth : region.array_layer_count;
  const auto max = std::numeric_limits<std::uint64_t>::max();
  if (image_rows > max / row_pitch || image_count - 1 > max / (image_rows * row_pitch) ||
      region.height - 1 > max / row_pitch) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  const std::uint64_t required =
      (image_count - 1) * image_rows * row_pitch + (region.height - 1) * row_pitch + tight_row;
  if (layout.offset > destination_record->desc.size ||
      required > destination_record->desc.size - layout.offset) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }

  VkBufferImageCopy copy{};
  copy.bufferOffset = layout.offset;
  copy.bufferRowLength = layout.bytes_per_row == 0 ? 0 : layout.bytes_per_row / bytes_per_pixel;
  copy.bufferImageHeight = layout.rows_per_image;
  copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, region.mip_level, region.base_array_layer,
                           region.array_layer_count};
  copy.imageOffset = {static_cast<std::int32_t>(region.x), static_cast<std::int32_t>(region.y),
                      static_cast<std::int32_t>(region.z)};
  copy.imageExtent = {region.width, region.height, region.depth};

  std::lock_guard record_lock{recorder_record->mutex};
  if (native_command_recorder(*recorder_record->native).state() !=
      command_recorder_state::recording)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  retain_resource(recorder_record->retained_resources, source_record, source_record->metadata);
  retain_resource(recorder_record->retained_resources, destination_record,
                  destination_record->metadata);
  return recorder_record->renderer->copy_texture_to_buffer(
      native_command_recorder(*recorder_record->native),
      native_texture(*source_record->native).image,
      native_buffer(*destination_record->native).buffer, copy);
}

granit_result renderer_registry::copy_buffer_to_texture(granit_renderer renderer,
                                                        granit_command_recorder recorder,
                                                        granit_buffer source,
                                                        granit_texture destination,
                                                        const granit_texture_data_layout& layout,
                                                        const granit_texture_write_region& region) {
  auto recorder_record = acquire_command_recorder(renderer, recorder);
  if (!recorder_record)
    return GRANIT_ERROR_INVALID_HANDLE;
  std::shared_ptr<buffer_record> source_record;
  std::shared_ptr<texture_record> destination_record;
  {
    std::lock_guard lock{mutex_};
    const auto& state = recorder_record->renderer;
    if (handles_.find(source, resource_type::buffer, state->domain()) == nullptr ||
        handles_.find(destination, resource_type::texture, state->domain()) == nullptr) {
      return GRANIT_ERROR_INVALID_HANDLE;
    }
    const auto found_source = buffers_.find(source);
    const auto found_destination = textures_.find(destination);
    if (found_source == buffers_.end() || found_destination == textures_.end() ||
        found_source->second->renderer != state || found_destination->second->renderer != state) {
      return GRANIT_ERROR_INVALID_HANDLE;
    }
    source_record = found_source->second;
    destination_record = found_destination->second;
  }

  const auto& desc = destination_record->desc;
  const auto bytes_per_pixel =
      depth_format(desc.format) ? 0 : texture_format_bytes_per_block(desc.format);
  if ((source_record->desc.usage & GRANIT_BUFFER_USAGE_TRANSFER_SOURCE_BIT) == 0 ||
      (desc.usage & GRANIT_TEXTURE_USAGE_TRANSFER_DESTINATION_BIT) == 0 ||
      desc.sample_count != GRANIT_SAMPLE_COUNT_1 || bytes_per_pixel == 0) {
    return GRANIT_ERROR_UNSUPPORTED;
  }
  if (region.aspect != GRANIT_TEXTURE_ASPECT_COLOR_BIT || region.width == 0 || region.height == 0 ||
      region.depth == 0 || region.array_layer_count == 0 || region.mip_level >= desc.mip_levels ||
      region.base_array_layer >= desc.array_layers ||
      region.array_layer_count > desc.array_layers - region.base_array_layer ||
      layout.offset % 4 != 0) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  const auto mip_width = std::max(UINT32_C(1), desc.width >> region.mip_level);
  const auto mip_height = std::max(UINT32_C(1), desc.height >> region.mip_level);
  const auto mip_depth = std::max(UINT32_C(1), desc.depth >> region.mip_level);
  if (region.x >= mip_width || region.width > mip_width - region.x || region.y >= mip_height ||
      region.height > mip_height - region.y || region.z >= mip_depth ||
      region.depth > mip_depth - region.z) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  const std::uint64_t tight_row = std::uint64_t{region.width} * bytes_per_pixel;
  const std::uint64_t row_pitch = layout.bytes_per_row == 0 ? tight_row : layout.bytes_per_row;
  const std::uint64_t image_rows =
      layout.rows_per_image == 0 ? region.height : layout.rows_per_image;
  if (row_pitch < tight_row || row_pitch % bytes_per_pixel != 0 || image_rows < region.height)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const std::uint64_t image_count =
      desc.dimension == GRANIT_TEXTURE_DIMENSION_3D ? region.depth : region.array_layer_count;
  const auto max = std::numeric_limits<std::uint64_t>::max();
  if (image_rows > max / row_pitch || image_count - 1 > max / (image_rows * row_pitch) ||
      region.height - 1 > max / row_pitch) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  const std::uint64_t required =
      (image_count - 1) * image_rows * row_pitch + (region.height - 1) * row_pitch + tight_row;
  if (layout.offset > source_record->desc.size ||
      required > source_record->desc.size - layout.offset) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }

  VkBufferImageCopy copy{};
  copy.bufferOffset = layout.offset;
  copy.bufferRowLength = layout.bytes_per_row == 0 ? 0 : layout.bytes_per_row / bytes_per_pixel;
  copy.bufferImageHeight = layout.rows_per_image;
  copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, region.mip_level, region.base_array_layer,
                           region.array_layer_count};
  copy.imageOffset = {static_cast<std::int32_t>(region.x), static_cast<std::int32_t>(region.y),
                      static_cast<std::int32_t>(region.z)};
  copy.imageExtent = {region.width, region.height, region.depth};

  std::lock_guard record_lock{recorder_record->mutex};
  if (native_command_recorder(*recorder_record->native).state() !=
      command_recorder_state::recording)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  retain_resource(recorder_record->retained_resources, source_record, source_record->metadata);
  retain_resource(recorder_record->retained_resources, destination_record,
                  destination_record->metadata);
  return recorder_record->renderer->copy_buffer_to_texture(
      native_command_recorder(*recorder_record->native),
      native_buffer(*source_record->native).buffer,
      native_texture(*destination_record->native).image, copy);
}

granit_result renderer_registry::copy_texture(granit_renderer renderer,
                                              granit_command_recorder recorder,
                                              granit_texture source, granit_texture destination,
                                              const granit_texture_copy_region& region) {
  auto recorder_record = acquire_command_recorder(renderer, recorder);
  if (!recorder_record)
    return GRANIT_ERROR_INVALID_HANDLE;
  std::shared_ptr<texture_record> source_record;
  std::shared_ptr<texture_record> destination_record;
  {
    std::lock_guard lock{mutex_};
    const auto& state = recorder_record->renderer;
    if (handles_.find(source, resource_type::texture, state->domain()) == nullptr ||
        handles_.find(destination, resource_type::texture, state->domain()) == nullptr) {
      return GRANIT_ERROR_INVALID_HANDLE;
    }
    const auto found_source = textures_.find(source);
    const auto found_destination = textures_.find(destination);
    if (found_source == textures_.end() || found_destination == textures_.end() ||
        found_source->second->renderer != state || found_destination->second->renderer != state) {
      return GRANIT_ERROR_INVALID_HANDLE;
    }
    source_record = found_source->second;
    destination_record = found_destination->second;
  }

  const auto& source_desc = source_record->desc;
  const auto& destination_desc = destination_record->desc;
  if (source == destination || source_desc.format != destination_desc.format ||
      source_desc.sample_count != GRANIT_SAMPLE_COUNT_1 ||
      destination_desc.sample_count != GRANIT_SAMPLE_COUNT_1 || depth_format(source_desc.format) ||
      (source_desc.usage & GRANIT_TEXTURE_USAGE_TRANSFER_SOURCE_BIT) == 0 ||
      (destination_desc.usage & GRANIT_TEXTURE_USAGE_TRANSFER_DESTINATION_BIT) == 0) {
    return GRANIT_ERROR_UNSUPPORTED;
  }
  if (region.aspect != GRANIT_TEXTURE_ASPECT_COLOR_BIT || region.reserved != 0 ||
      region.width == 0 || region.height == 0 || region.depth == 0 ||
      region.array_layer_count == 0 || region.source_mip_level >= source_desc.mip_levels ||
      region.destination_mip_level >= destination_desc.mip_levels ||
      region.source_base_array_layer >= source_desc.array_layers ||
      region.destination_base_array_layer >= destination_desc.array_layers ||
      region.array_layer_count > source_desc.array_layers - region.source_base_array_layer ||
      region.array_layer_count >
          destination_desc.array_layers - region.destination_base_array_layer) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  const auto region_fits = [](const granit_texture_desc& desc, std::uint32_t mip, std::uint32_t x,
                              std::uint32_t y, std::uint32_t z, std::uint32_t width,
                              std::uint32_t height, std::uint32_t depth) {
    const auto mip_width = std::max(UINT32_C(1), desc.width >> mip);
    const auto mip_height = std::max(UINT32_C(1), desc.height >> mip);
    const auto mip_depth = std::max(UINT32_C(1), desc.depth >> mip);
    return x < mip_width && width <= mip_width - x && y < mip_height && height <= mip_height - y &&
           z < mip_depth && depth <= mip_depth - z;
  };
  if (!region_fits(source_desc, region.source_mip_level, region.source_x, region.source_y,
                   region.source_z, region.width, region.height, region.depth) ||
      !region_fits(destination_desc, region.destination_mip_level, region.destination_x,
                   region.destination_y, region.destination_z, region.width, region.height,
                   region.depth)) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }

  const VkImageCopy copy{
      .srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, region.source_mip_level,
                         region.source_base_array_layer, region.array_layer_count},
      .srcOffset = {static_cast<std::int32_t>(region.source_x),
                    static_cast<std::int32_t>(region.source_y),
                    static_cast<std::int32_t>(region.source_z)},
      .dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, region.destination_mip_level,
                         region.destination_base_array_layer, region.array_layer_count},
      .dstOffset = {static_cast<std::int32_t>(region.destination_x),
                    static_cast<std::int32_t>(region.destination_y),
                    static_cast<std::int32_t>(region.destination_z)},
      .extent = {region.width, region.height, region.depth}};
  std::lock_guard record_lock{recorder_record->mutex};
  if (native_command_recorder(*recorder_record->native).state() !=
      command_recorder_state::recording)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  retain_resource(recorder_record->retained_resources, source_record, source_record->metadata);
  retain_resource(recorder_record->retained_resources, destination_record,
                  destination_record->metadata);
  return recorder_record->renderer->copy_texture(native_command_recorder(*recorder_record->native),
                                                 native_texture(*source_record->native).image,
                                                 native_texture(*destination_record->native).image,
                                                 copy);
}

granit_result renderer_registry::generate_mipmaps(granit_renderer renderer,
                                                  granit_command_recorder recorder,
                                                  granit_texture texture,
                                                  const granit_texture_mipmap_range& range) {
  auto recorder_record = acquire_command_recorder(renderer, recorder);
  if (!recorder_record)
    return GRANIT_ERROR_INVALID_HANDLE;
  std::shared_ptr<texture_record> texture_record_state;
  {
    std::lock_guard lock{mutex_};
    const auto& state = recorder_record->renderer;
    if (handles_.find(texture, resource_type::texture, state->domain()) == nullptr)
      return GRANIT_ERROR_INVALID_HANDLE;
    const auto found = textures_.find(texture);
    if (found == textures_.end() || found->second->renderer != state)
      return GRANIT_ERROR_INVALID_HANDLE;
    texture_record_state = found->second;
  }
  const auto& desc = texture_record_state->desc;
  if (depth_format(desc.format) || desc.sample_count != GRANIT_SAMPLE_COUNT_1 ||
      (desc.usage & GRANIT_TEXTURE_USAGE_TRANSFER_SOURCE_BIT) == 0 ||
      (desc.usage & GRANIT_TEXTURE_USAGE_TRANSFER_DESTINATION_BIT) == 0 ||
      !texture_record_state->renderer->texture_supports_linear_blit(desc.format)) {
    return GRANIT_ERROR_UNSUPPORTED;
  }
  if (range.level_count < 2 || range.array_layer_count == 0 ||
      range.base_mip_level >= desc.mip_levels ||
      range.level_count > desc.mip_levels - range.base_mip_level ||
      range.base_array_layer >= desc.array_layers ||
      range.array_layer_count > desc.array_layers - range.base_array_layer) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  const VkExtent3D base_extent{std::max(UINT32_C(1), desc.width >> range.base_mip_level),
                               std::max(UINT32_C(1), desc.height >> range.base_mip_level),
                               std::max(UINT32_C(1), desc.depth >> range.base_mip_level)};
  std::lock_guard record_lock{recorder_record->mutex};
  if (native_command_recorder(*recorder_record->native).state() !=
      command_recorder_state::recording)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  retain_resource(recorder_record->retained_resources, texture_record_state,
                  texture_record_state->metadata);
  return recorder_record->renderer->generate_mipmaps(
      native_command_recorder(*recorder_record->native),
      native_texture(*texture_record_state->native).image, base_extent, range.base_mip_level,
      range.level_count, range.base_array_layer, range.array_layer_count);
}

granit_result renderer_registry::fill_buffer(granit_renderer renderer,
                                             granit_command_recorder recorder, granit_buffer buffer,
                                             std::uint64_t offset, std::uint64_t size,
                                             std::uint32_t value) {
  auto recorder_record = acquire_command_recorder(renderer, recorder);
  if (!recorder_record) {
    return GRANIT_ERROR_INVALID_HANDLE;
  }
  std::shared_ptr<buffer_record> buffer_record_state;
  {
    std::lock_guard lock{mutex_};
    const auto& state = recorder_record->renderer;
    if (handles_.find(buffer, resource_type::buffer, state->domain()) == nullptr) {
      return GRANIT_ERROR_INVALID_HANDLE;
    }
    const auto found = buffers_.find(buffer);
    if (found == buffers_.end() || found->second->renderer != state) {
      return GRANIT_ERROR_INVALID_HANDLE;
    }
    buffer_record_state = found->second;
  }
  if ((buffer_record_state->desc.usage & GRANIT_BUFFER_USAGE_TRANSFER_DESTINATION_BIT) == 0 ||
      offset % 4 != 0 || size % 4 != 0 || size == 0 || offset >= buffer_record_state->desc.size ||
      size > buffer_record_state->desc.size - offset) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }

  std::lock_guard record_lock{recorder_record->mutex};
  if (native_command_recorder(*recorder_record->native).state() !=
      command_recorder_state::recording) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  retain_resource(recorder_record->retained_resources, buffer_record_state,
                  buffer_record_state->metadata);
  return recorder_record->renderer->fill_buffer(native_command_recorder(*recorder_record->native),
                                                native_buffer(*buffer_record_state->native).buffer,
                                                offset, size, value);
}

granit_result renderer_registry::bind_graphics_pipeline(granit_renderer renderer,
                                                        granit_command_recorder recorder,
                                                        granit_graphics_pipeline pipeline) {
  auto command = acquire_command_recorder(renderer, recorder);
  if (!command)
    return GRANIT_ERROR_INVALID_HANDLE;
  std::shared_ptr<graphics_pipeline_record> pipeline_record;
  {
    std::lock_guard lock{mutex_};
    const auto found = graphics_pipelines_.find(pipeline);
    if (found == graphics_pipelines_.end() || found->second->renderer != command->renderer)
      return GRANIT_ERROR_INVALID_HANDLE;
    pipeline_record = found->second;
  }
  std::lock_guard command_lock{command->mutex};
  if (native_command_recorder(*command->native).state() != command_recorder_state::recording)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const auto result =
      command->renderer->bind_graphics_pipeline(native_command_recorder(*command->native),
                                                native_graphics_pipeline(*pipeline_record->native));
  if (result == GRANIT_SUCCESS)
    retain_resource(command->retained_resources, pipeline_record, pipeline_record->metadata);
  return result;
}

granit_result
renderer_registry::bind_graphics_groups(granit_renderer renderer, granit_command_recorder recorder,
                                        granit_pipeline_layout layout, std::uint32_t first_group,
                                        std::span<const granit_bind_group> bind_groups) {
  auto command = acquire_command_recorder(renderer, recorder);
  if (!command)
    return GRANIT_ERROR_INVALID_HANDLE;
  std::shared_ptr<pipeline_layout_record> layout_record;
  std::vector<std::shared_ptr<bind_group_record>> group_records;
  std::vector<VkDescriptorSet> native_groups;
  std::vector<std::pair<VkBuffer, VkAccessFlags2>> buffer_accesses;
  std::vector<vulkan_image_access> image_accesses;
  {
    std::lock_guard lock{mutex_};
    const auto layout_found = pipeline_layouts_.find(layout);
    if (layout_found == pipeline_layouts_.end() ||
        layout_found->second->renderer != command->renderer)
      return GRANIT_ERROR_INVALID_HANDLE;
    layout_record = layout_found->second;
    if (first_group + bind_groups.size() > layout_record->bind_group_layouts.size())
      return GRANIT_ERROR_INVALID_ARGUMENT;
    group_records.reserve(bind_groups.size());
    native_groups.reserve(bind_groups.size());
    for (std::size_t index = 0; index < bind_groups.size(); ++index) {
      const auto found = bind_groups_.find(bind_groups[index]);
      if (found == bind_groups_.end() || found->second->renderer != command->renderer)
        return GRANIT_ERROR_INVALID_HANDLE;
      if (found->second->layout != layout_record->bind_group_layouts[first_group + index])
        return GRANIT_ERROR_INVALID_ARGUMENT;
      group_records.push_back(found->second);
      native_groups.push_back(native_bind_group(*found->second->native).set());
      buffer_accesses.insert(buffer_accesses.end(), found->second->graphics_buffer_accesses.begin(),
                             found->second->graphics_buffer_accesses.end());
      image_accesses.insert(image_accesses.end(), found->second->graphics_image_accesses.begin(),
                            found->second->graphics_image_accesses.end());
    }
  }
  std::lock_guard command_lock{command->mutex};
  if (native_command_recorder(*command->native).state() != command_recorder_state::recording)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const auto result = command->renderer->bind_graphics_groups(
      native_command_recorder(*command->native), native_pipeline_layout(*layout_record->native),
      first_group, native_groups, buffer_accesses, image_accesses);
  if (result == GRANIT_SUCCESS) {
    retain_resource(command->retained_resources, layout_record, layout_record->metadata);
    for (const auto& group : group_records)
      retain_resource(command->retained_resources, group, group->metadata);
  }
  return result;
}

granit_result renderer_registry::bind_compute_pipeline(granit_renderer renderer,
                                                       granit_command_recorder recorder,
                                                       granit_compute_pipeline pipeline) {
  auto command = acquire_command_recorder(renderer, recorder);
  if (!command)
    return GRANIT_ERROR_INVALID_HANDLE;
  std::shared_ptr<compute_pipeline_record> pipeline_record;
  {
    std::lock_guard lock{mutex_};
    const auto found = compute_pipelines_.find(pipeline);
    if (found == compute_pipelines_.end() || found->second->renderer != command->renderer)
      return GRANIT_ERROR_INVALID_HANDLE;
    pipeline_record = found->second;
  }
  std::lock_guard command_lock{command->mutex};
  if (native_command_recorder(*command->native).state() != command_recorder_state::recording)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const auto result = command->renderer->bind_compute_pipeline(
      native_command_recorder(*command->native), native_compute_pipeline(*pipeline_record->native));
  if (result == GRANIT_SUCCESS)
    retain_resource(command->retained_resources, pipeline_record, pipeline_record->metadata);
  return result;
}

granit_result
renderer_registry::bind_compute_groups(granit_renderer renderer, granit_command_recorder recorder,
                                       granit_pipeline_layout layout, std::uint32_t first_group,
                                       std::span<const granit_bind_group> bind_groups) {
  auto command = acquire_command_recorder(renderer, recorder);
  if (!command)
    return GRANIT_ERROR_INVALID_HANDLE;
  std::shared_ptr<pipeline_layout_record> layout_record;
  std::vector<std::shared_ptr<bind_group_record>> group_records;
  std::vector<VkDescriptorSet> native_groups;
  std::vector<std::pair<VkBuffer, VkAccessFlags2>> buffer_accesses;
  std::vector<vulkan_image_access> image_accesses;
  {
    std::lock_guard lock{mutex_};
    const auto layout_found = pipeline_layouts_.find(layout);
    if (layout_found == pipeline_layouts_.end() ||
        layout_found->second->renderer != command->renderer)
      return GRANIT_ERROR_INVALID_HANDLE;
    layout_record = layout_found->second;
    if (first_group + bind_groups.size() > layout_record->bind_group_layouts.size())
      return GRANIT_ERROR_INVALID_ARGUMENT;
    group_records.reserve(bind_groups.size());
    native_groups.reserve(bind_groups.size());
    for (std::size_t index = 0; index < bind_groups.size(); ++index) {
      const auto found = bind_groups_.find(bind_groups[index]);
      if (found == bind_groups_.end() || found->second->renderer != command->renderer)
        return GRANIT_ERROR_INVALID_HANDLE;
      if (found->second->layout != layout_record->bind_group_layouts[first_group + index])
        return GRANIT_ERROR_INVALID_ARGUMENT;
      group_records.push_back(found->second);
      native_groups.push_back(native_bind_group(*found->second->native).set());
      buffer_accesses.insert(buffer_accesses.end(), found->second->compute_buffer_accesses.begin(),
                             found->second->compute_buffer_accesses.end());
      image_accesses.insert(image_accesses.end(), found->second->compute_image_accesses.begin(),
                            found->second->compute_image_accesses.end());
    }
  }
  std::lock_guard command_lock{command->mutex};
  if (native_command_recorder(*command->native).state() != command_recorder_state::recording)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const auto result = command->renderer->bind_compute_groups(
      native_command_recorder(*command->native), native_pipeline_layout(*layout_record->native),
      first_group, native_groups, buffer_accesses, image_accesses);
  if (result == GRANIT_SUCCESS) {
    retain_resource(command->retained_resources, layout_record, layout_record->metadata);
    for (const auto& group : group_records)
      retain_resource(command->retained_resources, group, group->metadata);
  }
  return result;
}

granit_result renderer_registry::dispatch(granit_renderer renderer,
                                          granit_command_recorder recorder,
                                          std::uint32_t group_count_x, std::uint32_t group_count_y,
                                          std::uint32_t group_count_z) {
  auto command = acquire_command_recorder(renderer, recorder);
  if (!command)
    return GRANIT_ERROR_INVALID_HANDLE;
  std::lock_guard lock{command->mutex};
  return command->renderer->dispatch(native_command_recorder(*command->native), group_count_x,
                                     group_count_y, group_count_z);
}

granit_result renderer_registry::set_viewports(granit_renderer renderer,
                                               granit_command_recorder recorder,
                                               std::uint32_t first,
                                               std::span<const granit_viewport> viewports) {
  auto command = acquire_command_recorder(renderer, recorder);
  if (!command)
    return GRANIT_ERROR_INVALID_HANDLE;
  std::vector<VkViewport> native;
  native.reserve(viewports.size());
  for (const auto& value : viewports)
    native.push_back(
        {value.x, value.y, value.width, value.height, value.min_depth, value.max_depth});
  std::lock_guard lock{command->mutex};
  return command->renderer->set_viewports(native_command_recorder(*command->native), first, native);
}

granit_result renderer_registry::set_scissors(granit_renderer renderer,
                                              granit_command_recorder recorder, std::uint32_t first,
                                              std::span<const granit_scissor> scissors) {
  auto command = acquire_command_recorder(renderer, recorder);
  if (!command)
    return GRANIT_ERROR_INVALID_HANDLE;
  std::vector<VkRect2D> native;
  native.reserve(scissors.size());
  for (const auto& value : scissors)
    native.push_back({{value.x, value.y}, {value.width, value.height}});
  std::lock_guard lock{command->mutex};
  return command->renderer->set_scissors(native_command_recorder(*command->native), first, native);
}

granit_result
renderer_registry::bind_vertex_buffers(granit_renderer renderer, granit_command_recorder recorder,
                                       std::uint32_t first,
                                       std::span<const granit_vertex_buffer_binding> bindings) {
  auto command = acquire_command_recorder(renderer, recorder);
  if (!command)
    return GRANIT_ERROR_INVALID_HANDLE;
  std::vector<std::shared_ptr<buffer_record>> records;
  std::vector<VkBuffer> buffers;
  std::vector<VkDeviceSize> offsets;
  {
    std::lock_guard lock{mutex_};
    for (const auto& binding : bindings) {
      const auto found = buffers_.find(binding.buffer);
      if (found == buffers_.end() || found->second->renderer != command->renderer)
        return GRANIT_ERROR_INVALID_HANDLE;
      if ((found->second->desc.usage & GRANIT_BUFFER_USAGE_VERTEX_BIT) == 0 ||
          binding.offset >= found->second->desc.size)
        return GRANIT_ERROR_INVALID_ARGUMENT;
      records.push_back(found->second);
      buffers.push_back(native_buffer(*found->second->native).buffer);
      offsets.push_back(binding.offset);
    }
  }
  std::lock_guard lock{command->mutex};
  const auto result = command->renderer->bind_vertex_buffers(
      native_command_recorder(*command->native), first, buffers, offsets);
  if (result == GRANIT_SUCCESS) {
    for (const auto& record : records)
      retain_resource(command->retained_resources, record, record->metadata);
  }
  return result;
}

granit_result renderer_registry::bind_index_buffer(granit_renderer renderer,
                                                   granit_command_recorder recorder,
                                                   granit_buffer buffer, std::uint64_t offset,
                                                   granit_index_type type) {
  auto command = acquire_command_recorder(renderer, recorder);
  if (!command)
    return GRANIT_ERROR_INVALID_HANDLE;
  std::shared_ptr<buffer_record> record;
  {
    std::lock_guard lock{mutex_};
    const auto found = buffers_.find(buffer);
    if (found == buffers_.end() || found->second->renderer != command->renderer)
      return GRANIT_ERROR_INVALID_HANDLE;
    const auto alignment = type == GRANIT_INDEX_TYPE_UINT16 ? 2U : 4U;
    if ((found->second->desc.usage & GRANIT_BUFFER_USAGE_INDEX_BIT) == 0 ||
        offset >= found->second->desc.size || offset % alignment != 0)
      return GRANIT_ERROR_INVALID_ARGUMENT;
    record = found->second;
  }
  const auto native_type =
      type == GRANIT_INDEX_TYPE_UINT16 ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32;
  std::lock_guard lock{command->mutex};
  const auto result = command->renderer->bind_index_buffer(
      native_command_recorder(*command->native), native_buffer(*record->native).buffer, offset,
      native_type);
  if (result == GRANIT_SUCCESS)
    retain_resource(command->retained_resources, record, record->metadata);
  return result;
}

granit_result renderer_registry::draw(granit_renderer renderer, granit_command_recorder recorder,
                                      std::uint32_t vertex_count, std::uint32_t instance_count,
                                      std::uint32_t first_vertex, std::uint32_t first_instance) {
  auto command = acquire_command_recorder(renderer, recorder);
  if (!command)
    return GRANIT_ERROR_INVALID_HANDLE;
  std::lock_guard lock{command->mutex};
  return command->renderer->draw(native_command_recorder(*command->native), vertex_count,
                                 instance_count, first_vertex, first_instance);
}

granit_result renderer_registry::draw_indexed(granit_renderer renderer,
                                              granit_command_recorder recorder,
                                              std::uint32_t index_count,
                                              std::uint32_t instance_count,
                                              std::uint32_t first_index, std::int32_t vertex_offset,
                                              std::uint32_t first_instance) {
  auto command = acquire_command_recorder(renderer, recorder);
  if (!command)
    return GRANIT_ERROR_INVALID_HANDLE;
  std::lock_guard lock{command->mutex};
  return command->renderer->draw_indexed(native_command_recorder(*command->native), index_count,
                                         instance_count, first_index, vertex_offset,
                                         first_instance);
}

granit_result renderer_registry::begin_rendering(granit_renderer renderer,
                                                 granit_command_recorder recorder,
                                                 const granit_rendering_desc& desc) {
  auto command = acquire_command_recorder(renderer, recorder);
  if (!command)
    return GRANIT_ERROR_INVALID_HANDLE;
  std::vector<std::shared_ptr<texture_view_record>> views;
  views.reserve(desc.color_attachment_count + (desc.depth_stencil_attachment ? 1U : 0U));
  {
    std::lock_guard lock{mutex_};
    const auto acquire_view = [&](granit_texture_view handle) {
      if (handles_.find(handle, resource_type::texture_view, command->renderer->domain()) ==
          nullptr)
        return std::shared_ptr<texture_view_record>{};
      const auto found = texture_views_.find(handle);
      return found != texture_views_.end() && found->second->renderer == command->renderer
                 ? found->second
                 : std::shared_ptr<texture_view_record>{};
    };
    for (std::uint32_t index = 0; index < desc.color_attachment_count; ++index) {
      auto view = acquire_view(desc.color_attachments[index].view);
      if (!view)
        return GRANIT_ERROR_INVALID_HANDLE;
      views.push_back(std::move(view));
    }
    if (desc.depth_stencil_attachment) {
      auto view = acquire_view(desc.depth_stencil_attachment->view);
      if (!view)
        return GRANIT_ERROR_INVALID_HANDLE;
      views.push_back(std::move(view));
    }
  }
  std::uint32_t width{}, height{};
  granit_sample_count samples{};
  for (std::size_t index = 0; index < views.size(); ++index) {
    const bool depth = index >= desc.color_attachment_count;
    const auto& texture = views[index]->texture->desc;
    const auto usage = depth ? GRANIT_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT
                             : GRANIT_TEXTURE_USAGE_COLOR_ATTACHMENT_BIT;
    if (depth_format(texture.format) != depth || (texture.usage & usage) == 0)
      return GRANIT_ERROR_INVALID_ARGUMENT;
    if (width == 0) {
      width = texture.width;
      height = texture.height;
      samples = texture.sample_count;
    } else if (width != texture.width || height != texture.height ||
               samples != texture.sample_count) {
      return GRANIT_ERROR_INVALID_ARGUMENT;
    }
    if (std::find(views.begin(), views.begin() + static_cast<std::ptrdiff_t>(index),
                  views[index]) != views.begin() + static_cast<std::ptrdiff_t>(index))
      return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  if (desc.area.x > width || desc.area.width > width - desc.area.x || desc.area.y > height ||
      desc.area.height > height - desc.area.y)
    return GRANIT_ERROR_INVALID_ARGUMENT;

  std::vector<VkRenderingAttachmentInfo> colors(desc.color_attachment_count);
  for (std::uint32_t index = 0; index < desc.color_attachment_count; ++index) {
    const auto& source = desc.color_attachments[index];
    auto& target = colors[index];
    target.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    target.imageView = native_texture_view(*views[index]->native);
    target.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    target.loadOp = map_load(source.load_operation);
    target.storeOp = map_store(source.store_operation);
    target.clearValue.color = {{source.clear_value.red, source.clear_value.green,
                                source.clear_value.blue, source.clear_value.alpha}};
  }
  VkRenderingAttachmentInfo depth{}, stencil{};
  std::vector<vulkan_image_access> image_accesses;
  image_accesses.reserve(views.size());
  const auto resolved_aspect = [](const auto& view) {
    if (view->desc.range.aspect != GRANIT_TEXTURE_ASPECT_AUTOMATIC)
      return map_aspect(view->desc.range.aspect);
    VkImageAspectFlags aspect = depth_format(view->texture->desc.format)
                                    ? VK_IMAGE_ASPECT_DEPTH_BIT
                                    : VK_IMAGE_ASPECT_COLOR_BIT;
    if (stencil_format(view->texture->desc.format))
      aspect |= VK_IMAGE_ASPECT_STENCIL_BIT;
    return aspect;
  };
  const VkRenderingAttachmentInfo *depth_ptr = nullptr, *stencil_ptr = nullptr;
  for (std::uint32_t index = 0; index < desc.color_attachment_count; ++index) {
    const auto& view = views[index];
    image_accesses.push_back({
        .image = native_texture(*view->texture->native).image,
        .range = {.aspectMask = resolved_aspect(view),
                  .baseMipLevel = view->desc.range.base_mip_level,
                  .levelCount = view->desc.range.mip_level_count,
                  .baseArrayLayer = view->desc.range.base_array_layer,
                  .layerCount = view->desc.range.array_layer_count},
        .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .stages = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        .access = VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
        .preserve_content =
            desc.color_attachments[index].load_operation == GRANIT_ATTACHMENT_LOAD_OPERATION_LOAD,
    });
  }
  if (desc.depth_stencil_attachment) {
    const auto& source = *desc.depth_stencil_attachment;
    const auto& view = views.back();
    depth.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    depth.imageView = native_texture_view(*view->native);
    depth.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    depth.loadOp = map_load(source.depth_load_operation);
    depth.storeOp = map_store(source.depth_store_operation);
    depth.clearValue.depthStencil = {source.clear_value.depth, source.clear_value.stencil};
    depth_ptr = &depth;
    image_accesses.push_back({
        .image = native_texture(*view->texture->native).image,
        .range = {.aspectMask = resolved_aspect(view),
                  .baseMipLevel = view->desc.range.base_mip_level,
                  .levelCount = view->desc.range.mip_level_count,
                  .baseArrayLayer = view->desc.range.base_array_layer,
                  .layerCount = view->desc.range.array_layer_count},
        .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        .stages = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT |
                  VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
        .access = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                  VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
        .preserve_content = source.depth_load_operation == GRANIT_ATTACHMENT_LOAD_OPERATION_LOAD ||
                            source.stencil_load_operation == GRANIT_ATTACHMENT_LOAD_OPERATION_LOAD,
    });
    if (stencil_format(view->texture->desc.format)) {
      stencil = depth;
      stencil.loadOp = map_load(source.stencil_load_operation);
      stencil.storeOp = map_store(source.stencil_store_operation);
      stencil_ptr = &stencil;
    } else if (source.stencil_load_operation != GRANIT_ATTACHMENT_LOAD_OPERATION_DISCARD ||
               source.stencil_store_operation != GRANIT_ATTACHMENT_STORE_OPERATION_DISCARD) {
      return GRANIT_ERROR_INVALID_ARGUMENT;
    }
  }
  std::lock_guard command_lock{command->mutex};
  for (const auto& view : views)
    retain_resource(command->retained_resources, view, view->metadata);
  const VkRect2D area{
      {static_cast<std::int32_t>(desc.area.x), static_cast<std::int32_t>(desc.area.y)},
      {desc.area.width, desc.area.height}};
  return command->renderer->begin_rendering(native_command_recorder(*command->native), area, colors,
                                            depth_ptr, stencil_ptr, desc.layer_count,
                                            image_accesses);
}

granit_result renderer_registry::end_rendering(granit_renderer renderer,
                                               granit_command_recorder recorder) {
  auto command = acquire_command_recorder(renderer, recorder);
  if (!command)
    return GRANIT_ERROR_INVALID_HANDLE;
  std::lock_guard lock{command->mutex};
  return command->renderer->end_rendering(native_command_recorder(*command->native));
}

granit_result renderer_registry::destroy_command_recorder(granit_renderer renderer,
                                                          granit_command_recorder recorder) {
  auto record = acquire_command_recorder(renderer, recorder);
  if (!record) {
    return GRANIT_ERROR_INVALID_HANDLE;
  }
  if (record->owned_by_frame_context)
    return GRANIT_ERROR_UNSUPPORTED;
  {
    std::lock_guard record_lock{record->mutex};
    const auto wait_result = record->renderer->wait_command_recorder(*record->native);
    if (wait_result != GRANIT_SUCCESS && wait_result != GRANIT_ERROR_DEVICE_LOST) {
      return wait_result;
    }
  }
  {
    std::lock_guard lock{mutex_};
    const auto found = command_recorders_.find(recorder);
    if (found == command_recorders_.end() || found->second != record ||
        handles_.find(recorder, resource_type::command_recorder, record->renderer->domain()) ==
            nullptr) {
      return GRANIT_ERROR_INVALID_HANDLE;
    }
    command_recorders_.erase(found);
    static_cast<void>(
        handles_.erase(recorder, resource_type::command_recorder, record->renderer->domain()));
  }
  auto state = record->renderer;
  record.reset();
  static_cast<void>(state->collect_retired());
  return GRANIT_SUCCESS;
}

granit_result renderer_registry::create_timestamp_query_pool(granit_renderer renderer,
                                                             std::uint32_t query_count,
                                                             granit_timestamp_query_pool& pool) {
  try {
    auto state = acquire(renderer);
    if (!state)
      return GRANIT_ERROR_INVALID_HANDLE;
    auto record = std::make_shared<timestamp_query_pool_record>();
    record->renderer = state;
    const auto result = record->native.initialize(state->device(), query_count);
    if (result != GRANIT_SUCCESS)
      return result;
    std::lock_guard lock{mutex_};
    const auto found = renderers_.find(renderer);
    if (found == renderers_.end() || found->second != state)
      return GRANIT_ERROR_INVALID_HANDLE;
    record->metadata.creation_sequence = next_creation_sequence_++;
    const auto handle =
        handles_.insert(record.get(), resource_type::timestamp_query_pool, state->domain());
    if (handle == GRANIT_NULL_HANDLE)
      return GRANIT_ERROR_OUT_OF_MEMORY;
    try {
      timestamp_query_pools_.emplace(handle, std::move(record));
    } catch (...) {
      static_cast<void>(
          handles_.erase(handle, resource_type::timestamp_query_pool, state->domain()));
      throw;
    }
    pool = handle;
    return GRANIT_SUCCESS;
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result renderer_registry::get_timestamp_query_results(granit_renderer renderer,
                                                             granit_timestamp_query_pool pool,
                                                             std::uint32_t first,
                                                             std::span<std::uint64_t> nanoseconds) {
  std::shared_ptr<timestamp_query_pool_record> record;
  {
    std::lock_guard lock{mutex_};
    const auto found_renderer = renderers_.find(renderer);
    if (found_renderer == renderers_.end() ||
        handles_.find(pool, resource_type::timestamp_query_pool,
                      found_renderer->second->domain()) == nullptr)
      return GRANIT_ERROR_INVALID_HANDLE;
    const auto found = timestamp_query_pools_.find(pool);
    if (found == timestamp_query_pools_.end() || found->second->renderer != found_renderer->second)
      return GRANIT_ERROR_INVALID_HANDLE;
    record = found->second;
  }
  std::lock_guard lock{record->mutex};
  return record->native.read_nanoseconds(record->renderer->device(), first, nanoseconds, false);
}

granit_result renderer_registry::destroy_timestamp_query_pool(granit_renderer renderer,
                                                              granit_timestamp_query_pool pool) {
  std::shared_ptr<timestamp_query_pool_record> record;
  {
    std::lock_guard lock{mutex_};
    const auto found_renderer = renderers_.find(renderer);
    if (found_renderer == renderers_.end() ||
        handles_.find(pool, resource_type::timestamp_query_pool,
                      found_renderer->second->domain()) == nullptr)
      return GRANIT_ERROR_INVALID_HANDLE;
    const auto found = timestamp_query_pools_.find(pool);
    if (found == timestamp_query_pools_.end() || found->second->renderer != found_renderer->second)
      return GRANIT_ERROR_INVALID_HANDLE;
    record = std::move(found->second);
    timestamp_query_pools_.erase(found);
    static_cast<void>(handles_.erase(pool, resource_type::timestamp_query_pool,
                                     found_renderer->second->domain()));
  }
  const auto state = record->renderer;
  const auto serial = record->metadata.last_use_serial.load();
  state->retire_resource(serial, retirement_order::dependent, std::move(record));
  static_cast<void>(state->collect_retired());
  return GRANIT_SUCCESS;
}

granit_result renderer_registry::reset_timestamp_queries(granit_renderer renderer,
                                                         granit_command_recorder recorder,
                                                         granit_timestamp_query_pool pool,
                                                         std::uint32_t first, std::uint32_t count) {
  auto command = acquire_command_recorder(renderer, recorder);
  if (!command)
    return GRANIT_ERROR_INVALID_HANDLE;
  std::shared_ptr<timestamp_query_pool_record> query;
  {
    std::lock_guard lock{mutex_};
    const auto found = timestamp_query_pools_.find(pool);
    if (found == timestamp_query_pools_.end() || found->second->renderer != command->renderer ||
        handles_.find(pool, resource_type::timestamp_query_pool, command->renderer->domain()) ==
            nullptr)
      return GRANIT_ERROR_INVALID_HANDLE;
    query = found->second;
  }
  std::lock_guard command_lock{command->mutex};
  if (native_command_recorder(*command->native).state() != command_recorder_state::recording)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  std::lock_guard query_lock{query->mutex};
  const auto result =
      query->native.reset(command->renderer->device(),
                          native_command_recorder(*command->native).native_handle(), first, count);
  if (result == GRANIT_SUCCESS)
    retain_resource(command->retained_resources, query, query->metadata);
  return result;
}

granit_result renderer_registry::write_timestamp(granit_renderer renderer,
                                                 granit_command_recorder recorder,
                                                 granit_timestamp_query_pool pool,
                                                 granit_timestamp_stage stage,
                                                 std::uint32_t index) {
  auto command = acquire_command_recorder(renderer, recorder);
  if (!command)
    return GRANIT_ERROR_INVALID_HANDLE;
  std::shared_ptr<timestamp_query_pool_record> query;
  {
    std::lock_guard lock{mutex_};
    const auto found = timestamp_query_pools_.find(pool);
    if (found == timestamp_query_pools_.end() || found->second->renderer != command->renderer ||
        handles_.find(pool, resource_type::timestamp_query_pool, command->renderer->domain()) ==
            nullptr)
      return GRANIT_ERROR_INVALID_HANDLE;
    query = found->second;
  }
  const auto native_stage =
      stage == GRANIT_TIMESTAMP_STAGE_TOP      ? VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT
      : stage == GRANIT_TIMESTAMP_STAGE_DRAW   ? VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT
      : stage == GRANIT_TIMESTAMP_STAGE_BOTTOM ? VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT
                                               : VkPipelineStageFlags2{};
  if (native_stage == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  std::lock_guard command_lock{command->mutex};
  if (native_command_recorder(*command->native).state() != command_recorder_state::recording)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  std::lock_guard query_lock{query->mutex};
  const auto result = query->native.write(command->renderer->device(),
                                          native_command_recorder(*command->native).native_handle(),
                                          native_stage, index);
  if (result == GRANIT_SUCCESS)
    retain_resource(command->retained_resources, query, query->metadata);
  return result;
}

std::shared_ptr<renderer_registry::command_recorder_record>
renderer_registry::acquire_command_recorder(granit_renderer renderer,
                                            granit_command_recorder recorder) {
  std::lock_guard lock{mutex_};
  const auto found_renderer = renderers_.find(renderer);
  if (found_renderer == renderers_.end() ||
      handles_.find(renderer, resource_type::renderer, 0) == nullptr) {
    return {};
  }
  const auto& state = found_renderer->second;
  if (handles_.find(recorder, resource_type::command_recorder, state->domain()) == nullptr) {
    return {};
  }
  const auto found = command_recorders_.find(recorder);
  if (found == command_recorders_.end() || found->second->renderer != state) {
    return {};
  }
  return found->second;
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
