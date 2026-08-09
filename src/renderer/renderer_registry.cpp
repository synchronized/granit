// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "renderer/renderer_registry.h"

#include <algorithm>
#include <cstring>
#include <new>
#include <utility>
#include <vector>

namespace granit::detail {
namespace {

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

void retain_resource(std::vector<std::shared_ptr<void>>& resources,
                     const std::shared_ptr<void>& resource) {
  for (const auto& retained : resources) {
    if (retained.get() == resource.get()) {
      return;
    }
  }
  resources.push_back(resource);
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

renderer_registry::buffer_record::~buffer_record() {
  if (renderer) {
    renderer->destroy_native_buffer(native);
  }
}

renderer_registry::texture_record::~texture_record() {
  if (renderer && owned) {
    renderer->destroy_native_texture(native);
  }
}

renderer_registry::texture_view_record::~texture_view_record() {
  if (renderer) {
    renderer->destroy_native_texture_view(native);
  }
}

renderer_registry::sampler_record::~sampler_record() {
  if (renderer)
    renderer->destroy_native_sampler(native);
}

renderer_registry::command_recorder_record::~command_recorder_record() {
  if (renderer) {
    renderer->destroy_native_command_recorder(native);
  }
}

granit_result renderer_registry::create(std::string_view application_name, bool enable_validation,
                                        std::uint32_t surface_types, std::uint32_t frames_in_flight,
                                        granit_renderer& renderer) {
  try {
    auto state = std::make_shared<renderer_state>();
    const auto initialize_result =
        state->initialize(application_name, enable_validation, surface_types, frames_in_flight);
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
  lifecycle_snapshot lifecycle;
  std::vector<std::shared_ptr<command_recorder_record>> native_command_recorders;
  std::vector<std::shared_ptr<swapchain_record>> native_swapchains;
  std::vector<std::shared_ptr<surface_record>> native_surfaces;
  std::vector<std::shared_ptr<buffer_record>> native_buffers;
  std::vector<std::shared_ptr<texture_view_record>> native_texture_views;
  std::vector<std::shared_ptr<texture_record>> native_textures;
  std::vector<std::shared_ptr<sampler_record>> native_samplers;
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
    }
    for (auto frame = frames_.begin(); frame != frames_.end();) {
      if (frame->second->renderer == state) {
        static_cast<void>(handles_.erase(frame->first, resource_type::frame, state->domain()));
        frame = frames_.erase(frame);
      } else {
        ++frame;
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
    for (auto sampler = samplers_.begin(); sampler != samplers_.end();) {
      if (sampler->second->renderer == state) {
        native_samplers.push_back(std::move(sampler->second));
        static_cast<void>(handles_.erase(sampler->first, resource_type::sampler, state->domain()));
        sampler = samplers_.erase(sampler);
      } else {
        ++sampler;
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
  write_lifecycle_diagnostic(renderer, state->domain(), lifecycle);
  static_cast<void>(state->wait_for_all_submissions());
  native_command_recorders.clear();
  native_swapchains.clear();
  native_surfaces.clear();
  native_buffers.clear();
  native_texture_views.clear();
  native_textures.clear();
  native_samplers.clear();
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
    native_surface = std::move(found->second);
    surfaces_.erase(found);
    const auto erase_result = handles_.erase(surface, resource_type::surface, state->domain());
    if (erase_result != GRANIT_SUCCESS) {
      return erase_result;
    }
  }
  write_child_lifecycle_diagnostic(lifecycle_resource_type::surface, surface,
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
    const auto create_result =
        state->create_swapchain(surface_state->native_handle, desc, *record->native);
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
    for (const auto& [frame_handle, frame] : frames_) {
      static_cast<void>(frame_handle);
      if (frame->swapchain == found->second)
        return GRANIT_ERROR_INVALID_ARGUMENT;
    }
    record = found->second;
    for (const auto handle : record->views) {
      const auto view = texture_views_.find(handle);
      if (view != texture_views_.end()) {
        old_views.push_back(std::move(view->second));
        texture_views_.erase(view);
      }
      static_cast<void>(handles_.erase(handle, resource_type::texture_view, state->domain()));
    }
    for (const auto handle : record->textures) {
      const auto texture = textures_.find(handle);
      if (texture != textures_.end()) {
        old_textures.push_back(std::move(texture->second));
        textures_.erase(texture);
      }
      static_cast<void>(handles_.erase(handle, resource_type::texture, state->domain()));
    }
    record->views.clear();
    record->textures.clear();
  }
  old_views.clear();
  old_textures.clear();
  const auto result =
      record->renderer->recreate_swapchain(record->surface->native_handle, desc, *record->native);
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
      texture->native.image = image;
      texture->owned = false;
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
      const auto result = record->renderer->create_native_texture_view(
          texture->native, texture->desc, desc, view->native);
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
  if (result != GRANIT_SUCCESS) {
    std::lock_guard lock{mutex_};
    frames_.erase(handle);
    static_cast<void>(handles_.erase(handle, resource_type::frame, record->renderer->domain()));
    return result;
  }
  frame = handle;
  image_index = record->image_index;
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
  const auto result =
      command->renderer->submit_swapchain_frame(command->native, *frame_state->swapchain->native,
                                                frame_state->image_index, frame_state->slot_index);
  if (result == GRANIT_SUCCESS)
    frame_state->submitted = true;
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
    record = std::move(found->second);
    for (const auto handle : record->views) {
      const auto item = texture_views_.find(handle);
      if (item != texture_views_.end()) {
        views.push_back(std::move(item->second));
        texture_views_.erase(item);
      }
      static_cast<void>(handles_.erase(handle, resource_type::texture_view, state->domain()));
    }
    for (const auto handle : record->textures) {
      const auto item = textures_.find(handle);
      if (item != textures_.end()) {
        textures.push_back(std::move(item->second));
        textures_.erase(item);
      }
      static_cast<void>(handles_.erase(handle, resource_type::texture, state->domain()));
    }
    swapchains_.erase(found);
    const auto erase_result = handles_.erase(swapchain, resource_type::swapchain, state->domain());
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
    const auto create_result = state->create_native_buffer(desc, record->native);
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
    const auto result = record->renderer->invalidate_buffer(record->native, offset, size);
    if (result != GRANIT_SUCCESS) {
      return result;
    }
  }
  record->mapped = true;
  record->mapped_offset = offset;
  record->mapped_size = size;
  data = static_cast<unsigned char*>(record->native.mapped_data) + offset;
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
    result =
        record->renderer->flush_buffer(record->native, record->mapped_offset, record->mapped_size);
  }
  record->mapped = false;
  record->mapped_offset = 0;
  record->mapped_size = 0;
  return result;
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
  record.reset();
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
    std::memcpy(static_cast<unsigned char*>(record->native.mapped_data) + offset, data,
                static_cast<std::size_t>(size));
    return record->renderer->flush_buffer(record->native, offset, size);
  }
  return record->renderer->upload_buffer(record->native, offset, data, size);
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
    const auto result = state->create_native_texture(desc, record->native);
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
    const auto result =
        state->create_native_texture_view(parent->native, parent->desc, desc, record->native);
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
  record.reset();
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
  write_child_lifecycle_diagnostic(lifecycle_resource_type::texture, texture,
                                   lifecycle_resource_type::texture_view,
                                   lifecycle.summary(lifecycle_resource_type::texture_view));
  views.clear();
  record.reset();
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
    const auto result = state->create_native_sampler(desc, record->native);
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
  record.reset();
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
    const auto result = state->create_native_command_recorder(record->native);
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
  return record->renderer->begin_command_recorder(record->native);
}

granit_result renderer_registry::end_command_recorder(granit_renderer renderer,
                                                      granit_command_recorder recorder) {
  auto record = acquire_command_recorder(renderer, recorder);
  if (!record) {
    return GRANIT_ERROR_INVALID_HANDLE;
  }
  std::lock_guard record_lock{record->mutex};
  return record->renderer->end_command_recorder(record->native);
}

granit_result renderer_registry::submit_command_recorder(granit_renderer renderer,
                                                         granit_command_recorder recorder) {
  auto record = acquire_command_recorder(renderer, recorder);
  if (!record) {
    return GRANIT_ERROR_INVALID_HANDLE;
  }
  std::lock_guard record_lock{record->mutex};
  return record->renderer->submit_command_recorder(record->native);
}

granit_result renderer_registry::reset_command_recorder(granit_renderer renderer,
                                                        granit_command_recorder recorder) {
  auto record = acquire_command_recorder(renderer, recorder);
  if (!record) {
    return GRANIT_ERROR_INVALID_HANDLE;
  }
  std::lock_guard record_lock{record->mutex};
  const auto wait_result = record->renderer->wait_command_recorder(record->native);
  if (wait_result != GRANIT_SUCCESS) {
    return wait_result;
  }
  const auto result = record->renderer->reset_command_recorder(record->native);
  if (result == GRANIT_SUCCESS) {
    record->retained_resources.clear();
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
  if (recorder_record->native.state() != command_recorder_state::recording) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  retain_resource(recorder_record->retained_resources, source_record);
  retain_resource(recorder_record->retained_resources, destination_record);
  return recorder_record->renderer->copy_buffer(recorder_record->native,
                                                source_record->native.buffer,
                                                destination_record->native.buffer, native_regions);
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
  if (recorder_record->native.state() != command_recorder_state::recording) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  retain_resource(recorder_record->retained_resources, buffer_record_state);
  return recorder_record->renderer->fill_buffer(
      recorder_record->native, buffer_record_state->native.buffer, offset, size, value);
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
    target.imageView = views[index]->native;
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
        .image = view->texture->native.image,
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
    depth.imageView = view->native;
    depth.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    depth.loadOp = map_load(source.depth_load_operation);
    depth.storeOp = map_store(source.depth_store_operation);
    depth.clearValue.depthStencil = {source.clear_value.depth, source.clear_value.stencil};
    depth_ptr = &depth;
    image_accesses.push_back({
        .image = view->texture->native.image,
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
    retain_resource(command->retained_resources, view);
  const VkRect2D area{
      {static_cast<std::int32_t>(desc.area.x), static_cast<std::int32_t>(desc.area.y)},
      {desc.area.width, desc.area.height}};
  return command->renderer->begin_rendering(command->native, area, colors, depth_ptr, stencil_ptr,
                                            desc.layer_count, image_accesses);
}

granit_result renderer_registry::end_rendering(granit_renderer renderer,
                                               granit_command_recorder recorder) {
  auto command = acquire_command_recorder(renderer, recorder);
  if (!command)
    return GRANIT_ERROR_INVALID_HANDLE;
  std::lock_guard lock{command->mutex};
  return command->renderer->end_rendering(command->native);
}

granit_result renderer_registry::destroy_command_recorder(granit_renderer renderer,
                                                          granit_command_recorder recorder) {
  auto record = acquire_command_recorder(renderer, recorder);
  if (!record) {
    return GRANIT_ERROR_INVALID_HANDLE;
  }
  {
    std::lock_guard record_lock{record->mutex};
    const auto wait_result = record->renderer->wait_command_recorder(record->native);
    if (wait_result != GRANIT_SUCCESS) {
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
  record.reset();
  return GRANIT_SUCCESS;
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
