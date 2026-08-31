// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "renderer/renderer_registry.h"
#include "renderer/renderer_registry_records.h"

#include "backend/diagnostics.h"
#include "renderer/renderer_registry_helpers.h"

#include <algorithm>
#include <new>
#include <utility>
#include <vector>

namespace granit::detail {

granit_result renderer_registry::create_win32_surface(granit_renderer renderer,
                                                      void* native_instance, void* native_window,
                                                      granit_surface& surface) {
  try {
    auto owner = acquire_backend(renderer);
    auto state = std::dynamic_pointer_cast<backend_presentation_renderer>(owner);
    if (!owner || !state) {
      return GRANIT_ERROR_INVALID_HANDLE;
    }

    auto record = std::make_shared<surface_record>();
    record->owner = owner;
    record->renderer = state;
    record->native = state->allocate_surface_resource();
    const auto create_result =
        state->create_win32_surface(native_instance, native_window, *record->native);
    if (create_result != GRANIT_SUCCESS) {
      return create_result;
    }

    std::lock_guard lock{mutex_};
    const auto renderer_found = backend_renderers_.find(renderer);
    if (renderer_found == backend_renderers_.end() || renderer_found->second != owner) {
      return GRANIT_ERROR_INVALID_HANDLE;
    }
    record->metadata.creation_sequence = next_creation_sequence_++;
    const auto handle = handles_.insert(record.get(), resource_type::surface, owner->domain());
    if (handle == GRANIT_NULL_HANDLE) {
      return GRANIT_ERROR_OUT_OF_MEMORY;
    }
    try {
      surfaces_.emplace(handle, std::move(record));
    } catch (...) {
      static_cast<void>(handles_.erase(handle, resource_type::surface, owner->domain()));
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
    auto owner = acquire_backend(renderer);
    auto state = std::dynamic_pointer_cast<backend_presentation_renderer>(owner);
    if (!owner || !state)
      return GRANIT_ERROR_INVALID_HANDLE;

    auto record = std::make_shared<surface_record>();
    record->owner = owner;
    record->renderer = state;
    record->native = state->allocate_surface_resource();
    const auto create_result = state->create_xcb_surface(connection, window, *record->native);
    if (create_result != GRANIT_SUCCESS)
      return create_result;

    std::lock_guard lock{mutex_};
    const auto renderer_found = backend_renderers_.find(renderer);
    if (renderer_found == backend_renderers_.end() || renderer_found->second != owner)
      return GRANIT_ERROR_INVALID_HANDLE;
    record->metadata.creation_sequence = next_creation_sequence_++;
    const auto handle = handles_.insert(record.get(), resource_type::surface, owner->domain());
    if (handle == GRANIT_NULL_HANDLE)
      return GRANIT_ERROR_OUT_OF_MEMORY;
    try {
      surfaces_.emplace(handle, std::move(record));
    } catch (...) {
      static_cast<void>(handles_.erase(handle, resource_type::surface, owner->domain()));
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
    auto owner = acquire_backend(renderer);
    auto state = std::dynamic_pointer_cast<backend_presentation_renderer>(owner);
    if (!owner || !state)
      return GRANIT_ERROR_INVALID_HANDLE;

    auto record = std::make_shared<surface_record>();
    record->owner = owner;
    record->renderer = state;
    record->native = state->allocate_surface_resource();
    const auto create_result =
        state->create_wayland_surface(display, native_surface, *record->native);
    if (create_result != GRANIT_SUCCESS)
      return create_result;

    std::lock_guard lock{mutex_};
    const auto renderer_found = backend_renderers_.find(renderer);
    if (renderer_found == backend_renderers_.end() || renderer_found->second != owner)
      return GRANIT_ERROR_INVALID_HANDLE;
    record->metadata.creation_sequence = next_creation_sequence_++;
    const auto handle = handles_.insert(record.get(), resource_type::surface, owner->domain());
    if (handle == GRANIT_NULL_HANDLE)
      return GRANIT_ERROR_OUT_OF_MEMORY;
    try {
      surfaces_.emplace(handle, std::move(record));
    } catch (...) {
      static_cast<void>(handles_.erase(handle, resource_type::surface, owner->domain()));
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

granit_result renderer_registry::create_canvas_surface(granit_renderer renderer,
                                                       std::string_view selector,
                                                       granit_surface& surface) {
  try {
    auto owner = acquire_backend(renderer);
    auto state = std::dynamic_pointer_cast<backend_presentation_renderer>(owner);
    if (!owner || !state)
      return GRANIT_ERROR_INVALID_HANDLE;

    auto record = std::make_shared<surface_record>();
    record->owner = owner;
    record->renderer = state;
    record->native = state->allocate_surface_resource();
    const auto create_result = state->create_canvas_surface(selector, *record->native);
    if (create_result != GRANIT_SUCCESS)
      return create_result;

    std::lock_guard lock{mutex_};
    const auto renderer_found = backend_renderers_.find(renderer);
    if (renderer_found == backend_renderers_.end() || renderer_found->second != owner)
      return GRANIT_ERROR_INVALID_HANDLE;
    record->metadata.creation_sequence = next_creation_sequence_++;
    const auto handle = handles_.insert(record.get(), resource_type::surface, owner->domain());
    if (handle == GRANIT_NULL_HANDLE)
      return GRANIT_ERROR_OUT_OF_MEMORY;
    try {
      surfaces_.emplace(handle, std::move(record));
    } catch (...) {
      static_cast<void>(handles_.erase(handle, resource_type::surface, owner->domain()));
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
  std::shared_ptr<backend_renderer> owner;
  std::shared_ptr<backend_presentation_renderer> presentation;
  std::shared_ptr<backend_diagnostic_renderer> diagnostics;
  std::shared_ptr<surface_record> native_surface;
  lifecycle_snapshot lifecycle;
  std::vector<std::shared_ptr<swapchain_record>> native_swapchains;
  std::vector<std::shared_ptr<texture_view_record>> native_views;
  std::vector<std::shared_ptr<texture_record>> native_textures;
  {
    std::lock_guard lock{mutex_};
    const auto renderer_found = backend_renderers_.find(renderer);
    if (renderer_found == backend_renderers_.end() ||
        handles_.find(renderer, resource_type::renderer, 0) == nullptr) {
      return GRANIT_ERROR_INVALID_HANDLE;
    }
    owner = renderer_found->second;
    presentation = std::dynamic_pointer_cast<backend_presentation_renderer>(owner);
    diagnostics = std::dynamic_pointer_cast<backend_diagnostic_renderer>(owner);
    if (!presentation)
      return GRANIT_ERROR_UNSUPPORTED;
    if (handles_.find(surface, resource_type::surface, owner->domain()) == nullptr) {
      return GRANIT_ERROR_INVALID_HANDLE;
    }
    const auto found = surfaces_.find(surface);
    if (found == surfaces_.end() || found->second->owner != owner) {
      return GRANIT_ERROR_INVALID_HANDLE;
    }
    for (const auto& [frame_handle, frame] : frames_) {
      static_cast<void>(frame_handle);
      if (frame->swapchain->surface == found->second)
        return GRANIT_ERROR_INVALID_ARGUMENT;
    }
    native_surface = found->second;
  }
  const auto idle_result = presentation->wait_for_present_idle();
  if (idle_result != GRANIT_SUCCESS && idle_result != GRANIT_ERROR_DEVICE_LOST)
    return idle_result;
  static_cast<void>(presentation->collect_present_retired());
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
        if (diagnostics && diagnostics->validation_enabled()) {
          lifecycle.add(lifecycle_resource_type::swapchain, swapchain->first,
                        swapchain->second->metadata.creation_sequence);
        }
        for (const auto handle : swapchain->second->views) {
          const auto view = texture_views_.find(handle);
          if (view != texture_views_.end()) {
            native_views.push_back(std::move(view->second));
            texture_views_.erase(view);
          }
          static_cast<void>(handles_.erase(handle, resource_type::texture_view, owner->domain()));
        }
        for (const auto handle : swapchain->second->textures) {
          const auto texture = textures_.find(handle);
          if (texture != textures_.end()) {
            native_textures.push_back(std::move(texture->second));
            textures_.erase(texture);
          }
          static_cast<void>(handles_.erase(handle, resource_type::texture, owner->domain()));
        }
        native_swapchains.push_back(std::move(swapchain->second));
        static_cast<void>(
            handles_.erase(swapchain->first, resource_type::swapchain, owner->domain()));
        swapchain = swapchains_.erase(swapchain);
      } else {
        ++swapchain;
      }
    }
    surfaces_.erase(found);
    const auto erase_result = handles_.erase(surface, resource_type::surface, owner->domain());
    if (erase_result != GRANIT_SUCCESS) {
      return erase_result;
    }
  }
  if (diagnostics) {
    write_child_lifecycle_diagnostic(diagnostics->diagnostics(), lifecycle_resource_type::surface,
                                     surface, lifecycle_resource_type::swapchain,
                                     lifecycle.summary(lifecycle_resource_type::swapchain));
  }
  native_views.clear();
  native_textures.clear();
  native_swapchains.clear();
  native_surface.reset();
  return GRANIT_SUCCESS;
}

granit_result renderer_registry::create_swapchain(granit_renderer renderer, granit_surface surface,
                                                  const backend_swapchain_desc& desc,
                                                  granit_swapchain& swapchain) {
  try {
    std::shared_ptr<backend_renderer> owner;
    std::shared_ptr<backend_presentation_renderer> state;
    std::shared_ptr<surface_record> surface_state;
    {
      std::lock_guard lock{mutex_};
      const auto renderer_found = backend_renderers_.find(renderer);
      if (renderer_found == backend_renderers_.end() ||
          handles_.find(renderer, resource_type::renderer, 0) == nullptr) {
        return GRANIT_ERROR_INVALID_HANDLE;
      }
      owner = renderer_found->second;
      state = std::dynamic_pointer_cast<backend_presentation_renderer>(owner);
      if (!state || handles_.find(surface, resource_type::surface, owner->domain()) == nullptr) {
        return GRANIT_ERROR_INVALID_HANDLE;
      }
      const auto surface_found = surfaces_.find(surface);
      if (surface_found == surfaces_.end() || surface_found->second->owner != owner) {
        return GRANIT_ERROR_INVALID_HANDLE;
      }
      surface_state = surface_found->second;
    }

    auto record = std::make_shared<swapchain_record>();
    record->owner = owner;
    record->presentation = state;
    record->surface = surface_state;
    record->native = state->allocate_swapchain_resource();
    const auto create_result =
        state->create_swapchain(*surface_state->native, desc, *record->native);
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
      handle = handles_.insert(record.get(), resource_type::swapchain, owner->domain());
      if (handle == GRANIT_NULL_HANDLE)
        return GRANIT_ERROR_OUT_OF_MEMORY;
      try {
        swapchains_.emplace(handle, record);
      } catch (...) {
        static_cast<void>(handles_.erase(handle, resource_type::swapchain, owner->domain()));
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
                                                    const backend_swapchain_desc& desc) {
  std::shared_ptr<swapchain_record> record;
  std::vector<std::shared_ptr<texture_view_record>> old_views;
  std::vector<std::shared_ptr<texture_record>> old_textures;
  {
    std::lock_guard lock{mutex_};
    const auto renderer_found = backend_renderers_.find(renderer);
    if (renderer_found == backend_renderers_.end() ||
        handles_.find(renderer, resource_type::renderer, 0) == nullptr) {
      return GRANIT_ERROR_INVALID_HANDLE;
    }
    const auto& owner = renderer_found->second;
    if (handles_.find(swapchain, resource_type::swapchain, owner->domain()) == nullptr) {
      return GRANIT_ERROR_INVALID_HANDLE;
    }
    const auto found = swapchains_.find(swapchain);
    if (found == swapchains_.end() || found->second->owner != owner) {
      return GRANIT_ERROR_INVALID_HANDLE;
    }
    if (owner->lifecycle_status().state == backend_lifecycle_state::device_lost)
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
  const auto idle_result = record->presentation->wait_for_present_idle();
  if (idle_result != GRANIT_SUCCESS)
    return idle_result;
  static_cast<void>(record->presentation->collect_present_retired());
  {
    std::lock_guard lock{mutex_};
    const auto renderer_found = backend_renderers_.find(renderer);
    const auto found = swapchains_.find(swapchain);
    if (renderer_found == backend_renderers_.end() || found == swapchains_.end() ||
        renderer_found->second != record->owner || found->second != record)
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
          handles_.erase(handle, resource_type::texture_view, record->owner->domain()));
    }
    for (const auto handle : record->textures) {
      const auto texture = textures_.find(handle);
      if (texture != textures_.end()) {
        old_textures.push_back(std::move(texture->second));
        textures_.erase(texture);
      }
      static_cast<void>(handles_.erase(handle, resource_type::texture, record->owner->domain()));
    }
    record->views.clear();
    record->textures.clear();
  }
  old_views.clear();
  old_textures.clear();
  const auto result =
      record->presentation->recreate_swapchain(*record->surface->native, desc, *record->native);
  const auto install_result = install_swapchain_backbuffers(swapchain, record);
  return result == GRANIT_SUCCESS ? install_result : result;
}

granit_result
renderer_registry::install_swapchain_backbuffers(granit_swapchain swapchain,
                                                 const std::shared_ptr<swapchain_record>& record) {
  try {
    std::vector<backend_swapchain_backbuffer> backbuffers;
    const auto backbuffer_result =
        record->presentation->get_swapchain_backbuffers(*record->native, backbuffers);
    if (backbuffer_result != GRANIT_SUCCESS)
      return backbuffer_result;
    return install_swapchain_backbuffers(swapchain, record, std::move(backbuffers));
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result renderer_registry::install_swapchain_backbuffers(
    granit_swapchain swapchain, const std::shared_ptr<swapchain_record>& record,
    std::vector<backend_swapchain_backbuffer> backbuffers) {
  try {
    std::vector<std::shared_ptr<texture_record>> textures;
    std::vector<std::shared_ptr<texture_view_record>> views;
    textures.reserve(backbuffers.size());
    views.reserve(backbuffers.size());
    for (auto& backbuffer : backbuffers) {
      const auto prepare_result = record->presentation->prepare_swapchain_backbuffer(backbuffer);
      if (prepare_result != GRANIT_SUCCESS)
        return prepare_result;
      auto texture = std::make_shared<texture_record>();
      texture->owner = record->owner;
      texture->native = std::move(backbuffer.texture);
      texture->publicly_destroyable = false;
      texture->desc = backbuffer.desc;
      auto view = std::make_shared<texture_view_record>();
      view->owner = record->owner;
      view->texture = texture;
      view->publicly_destroyable = false;
      granit_texture_view_desc desc = GRANIT_TEXTURE_VIEW_DESC_INIT;
      view->desc = desc;
      view->native = std::move(backbuffer.view);
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
      const auto texture_handle =
          handles_.insert(textures[index].get(), resource_type::texture, record->owner->domain());
      if (texture_handle == GRANIT_NULL_HANDLE)
        return GRANIT_ERROR_OUT_OF_MEMORY;
      const auto view_handle =
          handles_.insert(views[index].get(), resource_type::texture_view, record->owner->domain());
      if (view_handle == GRANIT_NULL_HANDLE) {
        static_cast<void>(
            handles_.erase(texture_handle, resource_type::texture, record->owner->domain()));
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
                                                    backend_swapchain_info& info) {
  std::lock_guard lock{mutex_};
  const auto renderer_found = backend_renderers_.find(renderer);
  if (renderer_found == backend_renderers_.end() ||
      handles_.find(renderer, resource_type::renderer, 0) == nullptr) {
    return GRANIT_ERROR_INVALID_HANDLE;
  }
  const auto& owner = renderer_found->second;
  if (handles_.find(swapchain, resource_type::swapchain, owner->domain()) == nullptr) {
    return GRANIT_ERROR_INVALID_HANDLE;
  }
  const auto found = swapchains_.find(swapchain);
  if (found == swapchains_.end() || found->second->owner != owner) {
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
  const auto renderer_found = backend_renderers_.find(renderer);
  if (renderer_found == backend_renderers_.end() ||
      handles_.find(renderer, resource_type::renderer, 0) == nullptr)
    return GRANIT_ERROR_INVALID_HANDLE;
  const auto& owner = renderer_found->second;
  if (handles_.find(swapchain, resource_type::swapchain, owner->domain()) == nullptr)
    return GRANIT_ERROR_INVALID_HANDLE;
  const auto found = swapchains_.find(swapchain);
  if (found == swapchains_.end() || found->second->owner != owner)
    return GRANIT_ERROR_INVALID_HANDLE;
  if (owner->lifecycle_status().state == backend_lifecycle_state::device_lost)
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
    const auto renderer_found = backend_renderers_.find(renderer);
    if (renderer_found == backend_renderers_.end())
      return GRANIT_ERROR_INVALID_HANDLE;
    const auto found = swapchains_.find(swapchain);
    if (found == swapchains_.end() || found->second->owner != renderer_found->second)
      return GRANIT_ERROR_INVALID_HANDLE;
    if (renderer_found->second->lifecycle_status().state == backend_lifecycle_state::device_lost)
      return GRANIT_ERROR_DEVICE_LOST;
    if (found->second->surface_lost)
      return GRANIT_ERROR_SURFACE_LOST;
    swapchain_record_state = found->second;
  }
  auto record = std::make_shared<frame_record>();
  record->owner = swapchain_record_state->owner;
  record->presentation = swapchain_record_state->presentation;
  record->queue = std::dynamic_pointer_cast<backend_queue>(swapchain_record_state->owner);
  if (!record->queue)
    return GRANIT_ERROR_INTERNAL;
  record->swapchain = swapchain_record_state;
  granit_frame handle{};
  {
    std::lock_guard lock{mutex_};
    const auto found = swapchains_.find(swapchain);
    if (found == swapchains_.end() || found->second != swapchain_record_state)
      return GRANIT_ERROR_INVALID_HANDLE;
    handle = handles_.insert(record.get(), resource_type::frame, record->owner->domain());
    if (handle == GRANIT_NULL_HANDLE)
      return GRANIT_ERROR_OUT_OF_MEMORY;
    try {
      frames_.emplace(handle, record);
    } catch (...) {
      static_cast<void>(handles_.erase(handle, resource_type::frame, record->owner->domain()));
      throw;
    }
  }
  backend_acquired_swapchain_frame acquired{};
  const auto result =
      record->presentation->acquire_swapchain_frame(*swapchain_record_state->native, acquired);
  record->image_index = acquired.image_index;
  record->slot_index = acquired.slot_index;
  needs_recreate = acquired.needs_recreate;
  static_cast<void>(record->presentation->collect_present_retired());
  if (result != GRANIT_SUCCESS) {
    std::lock_guard lock{mutex_};
    if (result == GRANIT_ERROR_SURFACE_LOST)
      swapchain_record_state->surface_lost = true;
    frames_.erase(handle);
    static_cast<void>(handles_.erase(handle, resource_type::frame, record->owner->domain()));
    return result;
  }
  if (acquired.dynamic_backbuffer.texture) {
    std::vector<backend_swapchain_backbuffer> backbuffers;
    backbuffers.push_back(std::move(acquired.dynamic_backbuffer));
    const auto install_result =
        install_swapchain_backbuffers(swapchain, swapchain_record_state, std::move(backbuffers));
    if (install_result != GRANIT_SUCCESS) {
      bool ignored_needs_recreate{};
      static_cast<void>(record->presentation->cancel_swapchain_frame(
          *swapchain_record_state->native, record->image_index, record->slot_index,
          ignored_needs_recreate));
      std::lock_guard lock{mutex_};
      frames_.erase(handle);
      static_cast<void>(handles_.erase(handle, resource_type::frame, record->owner->domain()));
      return install_result;
    }
    record->dynamic_backbuffer = true;
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
    const auto found_renderer = backend_renderers_.find(renderer);
    const auto found_swapchain = swapchains_.find(swapchain);
    const auto found_frame = frames_.find(frame);
    if (found_renderer == backend_renderers_.end() || found_swapchain == swapchains_.end() ||
        found_frame == frames_.end() || found_frame->second->owner != found_renderer->second ||
        found_frame->second->swapchain != found_swapchain->second ||
        handles_.find(frame, resource_type::frame, found_renderer->second->domain()) == nullptr)
      return GRANIT_ERROR_INVALID_HANDLE;
    record = found_frame->second;
  }
  std::lock_guard frame_lock{record->mutex};
  const auto slot_count = record->presentation->frame_slot_count();
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
    if (found == frames_.end() || found->second->owner != command->owner)
      return GRANIT_ERROR_INVALID_HANDLE;
    frame_state = found->second;
  }
  std::scoped_lock locks{command->mutex, frame_state->mutex};
  if (frame_state->submitted)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  submission_serial serial{};
  const auto result = frame_state->queue->submit_swapchain_frame(
      *command->native, *frame_state->swapchain->native, frame_state->image_index,
      frame_state->slot_index, serial);
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
    const auto found_renderer = backend_renderers_.find(renderer);
    if (found_renderer == backend_renderers_.end() || found == frames_.end() ||
        found_swapchain == swapchains_.end() ||
        found->second->swapchain != found_swapchain->second ||
        found->second->owner != found_swapchain->second->owner ||
        found->second->owner != found_renderer->second)
      return GRANIT_ERROR_INVALID_HANDLE;
    record = found->second;
  }
  std::lock_guard frame_lock{record->mutex};
  if (!record->submitted)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const auto result = record->presentation->present_swapchain_frame(
      *record->swapchain->native, record->image_index, record->slot_index, needs_recreate);
  static_cast<void>(record->presentation->collect_present_retired());
  if (result == GRANIT_ERROR_SURFACE_LOST) {
    std::lock_guard lock{mutex_};
    record->swapchain->surface_lost = true;
  }
  std::vector<std::shared_ptr<texture_view_record>> expired_views;
  std::vector<std::shared_ptr<texture_record>> expired_textures;
  {
    std::lock_guard lock{mutex_};
    if (record->dynamic_backbuffer) {
      for (const auto handle : record->swapchain->views) {
        if (const auto found = texture_views_.find(handle); found != texture_views_.end()) {
          expired_views.push_back(std::move(found->second));
          texture_views_.erase(found);
        }
        static_cast<void>(
            handles_.erase(handle, resource_type::texture_view, record->owner->domain()));
      }
      for (const auto handle : record->swapchain->textures) {
        if (const auto found = textures_.find(handle); found != textures_.end()) {
          expired_textures.push_back(std::move(found->second));
          textures_.erase(found);
        }
        static_cast<void>(handles_.erase(handle, resource_type::texture, record->owner->domain()));
      }
      record->swapchain->views.clear();
      record->swapchain->textures.clear();
    }
    const auto found = frames_.find(frame);
    if (found != frames_.end() && found->second == record) {
      frames_.erase(found);
      static_cast<void>(handles_.erase(frame, resource_type::frame, record->owner->domain()));
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
    const auto found_renderer = backend_renderers_.find(renderer);
    const auto found_swapchain = swapchains_.find(swapchain);
    const auto found = frames_.find(frame);
    if (found_renderer == backend_renderers_.end() || found_swapchain == swapchains_.end() ||
        found == frames_.end() || found->second->owner != found_renderer->second ||
        found->second->swapchain != found_swapchain->second)
      return GRANIT_ERROR_INVALID_HANDLE;
    record = found->second;
  }
  std::lock_guard frame_lock{record->mutex};
  if (record->submitted)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const auto result = record->presentation->cancel_swapchain_frame(
      *record->swapchain->native, record->image_index, record->slot_index, needs_recreate);
  if (result == GRANIT_ERROR_SURFACE_LOST) {
    std::lock_guard lock{mutex_};
    record->swapchain->surface_lost = true;
  }
  if (result != GRANIT_SUCCESS && result != GRANIT_ERROR_OUT_OF_DATE &&
      result != GRANIT_ERROR_SURFACE_LOST && result != GRANIT_ERROR_DEVICE_LOST)
    return result;
  std::vector<std::shared_ptr<texture_view_record>> expired_views;
  std::vector<std::shared_ptr<texture_record>> expired_textures;
  {
    std::lock_guard lock{mutex_};
    if (record->dynamic_backbuffer) {
      for (const auto handle : record->swapchain->views) {
        if (const auto found_view = texture_views_.find(handle);
            found_view != texture_views_.end()) {
          expired_views.push_back(std::move(found_view->second));
          texture_views_.erase(found_view);
        }
        static_cast<void>(
            handles_.erase(handle, resource_type::texture_view, record->owner->domain()));
      }
      for (const auto handle : record->swapchain->textures) {
        if (const auto found_texture = textures_.find(handle); found_texture != textures_.end()) {
          expired_textures.push_back(std::move(found_texture->second));
          textures_.erase(found_texture);
        }
        static_cast<void>(handles_.erase(handle, resource_type::texture, record->owner->domain()));
      }
      record->swapchain->views.clear();
      record->swapchain->textures.clear();
    }
    const auto found = frames_.find(frame);
    if (found != frames_.end() && found->second == record) {
      frames_.erase(found);
      static_cast<void>(handles_.erase(frame, resource_type::frame, record->owner->domain()));
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
    const auto renderer_found = backend_renderers_.find(renderer);
    if (renderer_found == backend_renderers_.end() ||
        handles_.find(renderer, resource_type::renderer, 0) == nullptr) {
      return GRANIT_ERROR_INVALID_HANDLE;
    }
    const auto& owner = renderer_found->second;
    if (handles_.find(swapchain, resource_type::swapchain, owner->domain()) == nullptr) {
      return GRANIT_ERROR_INVALID_HANDLE;
    }
    const auto found = swapchains_.find(swapchain);
    if (found == swapchains_.end() || found->second->owner != owner) {
      return GRANIT_ERROR_INVALID_HANDLE;
    }
    for (const auto& [frame_handle, frame] : frames_) {
      static_cast<void>(frame_handle);
      if (frame->swapchain == found->second)
        return GRANIT_ERROR_INVALID_ARGUMENT;
    }
    record = found->second;
  }
  const auto idle_result = record->presentation->wait_for_present_idle();
  if (idle_result != GRANIT_SUCCESS && idle_result != GRANIT_ERROR_DEVICE_LOST)
    return idle_result;
  static_cast<void>(record->presentation->collect_present_retired());
  {
    std::lock_guard lock{mutex_};
    const auto renderer_found = backend_renderers_.find(renderer);
    const auto found = swapchains_.find(swapchain);
    if (renderer_found == backend_renderers_.end() || found == swapchains_.end() ||
        renderer_found->second != record->owner || found->second != record)
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
          handles_.erase(handle, resource_type::texture_view, record->owner->domain()));
    }
    for (const auto handle : record->textures) {
      const auto item = textures_.find(handle);
      if (item != textures_.end()) {
        textures.push_back(std::move(item->second));
        textures_.erase(item);
      }
      static_cast<void>(handles_.erase(handle, resource_type::texture, record->owner->domain()));
    }
    swapchains_.erase(found);
    const auto erase_result =
        handles_.erase(swapchain, resource_type::swapchain, record->owner->domain());
    if (erase_result != GRANIT_SUCCESS) {
      return erase_result;
    }
  }
  views.clear();
  textures.clear();
  record.reset();
  return GRANIT_SUCCESS;
}

} // namespace granit::detail
