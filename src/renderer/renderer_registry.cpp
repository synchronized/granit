// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "renderer/renderer_registry.h"

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
  lifecycle_snapshot lifecycle;
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
