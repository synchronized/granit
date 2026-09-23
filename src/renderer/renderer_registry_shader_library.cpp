// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "renderer/renderer_registry.h"
#include "renderer/renderer_registry_records.h"

#include "asset_formats/shader/shader_library.h"
#include "core/sha256.h"

#include <algorithm>
#include <cstring>
#include <limits>
#include <new>
#include <ranges>
#include <vector>

namespace granit::detail {
namespace {

granit_result fill_info(const auto& record, granit_shader_library_info& info) noexcept {
  std::uint64_t variant_count = 0;
  for (const auto& shader : record.view.shaders)
    variant_count += shader.variants.size();
  if (record.view.shaders.size() > UINT32_MAX || record.view.payloads.size() > UINT32_MAX ||
      variant_count > UINT32_MAX)
    return GRANIT_ERROR_INTERNAL;

  info.backend_flags = record.view.backend_mask;
  std::memcpy(info.content_digest, record.view.content_digest.data(),
              record.view.content_digest.size());
  info.shader_count = static_cast<std::uint32_t>(record.view.shaders.size());
  info.variant_count = static_cast<std::uint32_t>(variant_count);
  info.payload_count = static_cast<std::uint32_t>(record.view.payloads.size());
  info.reserved = 0;
  info.archive_size = record.archive.size();
  return GRANIT_SUCCESS;
}

} // namespace

granit_result renderer_registry::create_shader_library(granit_renderer renderer,
                                                       std::span<const std::byte> archive,
                                                       granit_shader_library& library) {
  try {
    auto owner = acquire_backend(renderer);
    if (!owner)
      return GRANIT_ERROR_INVALID_HANDLE;
    auto record = std::make_shared<shader_library_record>();
    record->owner = owner;
    record->archive = archive;
    const auto decode_result = shader_format::decode_shader_library(archive, record->view);
    if (decode_result == shader_format::shader_library_error::unsupported_schema)
      return GRANIT_ERROR_UNSUPPORTED;
    if (decode_result != shader_format::shader_library_error::success)
      return GRANIT_ERROR_INVALID_ARGUMENT;

    granit_shader_library_info checked = GRANIT_SHADER_LIBRARY_INFO_INIT;
    if (fill_info(*record, checked) != GRANIT_SUCCESS)
      return GRANIT_ERROR_INVALID_ARGUMENT;

    std::lock_guard lock{mutex_};
    const auto found = backend_renderers_.find(renderer);
    if (found == backend_renderers_.end() || found->second != owner)
      return GRANIT_ERROR_INVALID_HANDLE;
    record->metadata.creation_sequence = next_creation_sequence_++;
    const auto handle =
        handles_.insert(record.get(), resource_type::shader_library, owner->domain());
    if (handle == GRANIT_NULL_HANDLE)
      return GRANIT_ERROR_OUT_OF_MEMORY;
    try {
      shader_libraries_.emplace(handle, std::move(record));
    } catch (...) {
      static_cast<void>(handles_.erase(handle, resource_type::shader_library, owner->domain()));
      throw;
    }
    library = handle;
    return GRANIT_SUCCESS;
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result renderer_registry::get_shader_library_info(granit_renderer renderer,
                                                         granit_shader_library library,
                                                         granit_shader_library_info& info) {
  std::lock_guard lock{mutex_};
  const auto found_renderer = backend_renderers_.find(renderer);
  if (found_renderer == backend_renderers_.end() ||
      handles_.find(renderer, resource_type::renderer, 0) == nullptr)
    return GRANIT_ERROR_INVALID_HANDLE;
  const auto& owner = found_renderer->second;
  if (handles_.find(library, resource_type::shader_library, owner->domain()) == nullptr)
    return GRANIT_ERROR_INVALID_HANDLE;
  const auto found = shader_libraries_.find(library);
  if (found == shader_libraries_.end() || found->second->owner != owner)
    return GRANIT_ERROR_INVALID_HANDLE;
  return fill_info(*found->second, info);
}

granit_result renderer_registry::create_shader_from_library_name(granit_renderer renderer,
                                                                 granit_shader_library library,
                                                                 std::string_view logical_name,
                                                                 granit_shader& shader) {
  granit::shader_content_id content_id{};
  {
    std::lock_guard lock{mutex_};
    const auto found_renderer = backend_renderers_.find(renderer);
    if (found_renderer == backend_renderers_.end() ||
        handles_.find(renderer, resource_type::renderer, 0) == nullptr)
      return GRANIT_ERROR_INVALID_HANDLE;
    const auto found = shader_libraries_.find(library);
    if (found == shader_libraries_.end() || found->second->owner != found_renderer->second ||
        handles_.find(library, resource_type::shader_library, found_renderer->second->domain()) ==
            nullptr)
      return GRANIT_ERROR_INVALID_HANDLE;
    const auto* source =
        shader_format::find_shader_library_shader(found->second->view, logical_name);
    if (source == nullptr)
      return GRANIT_ERROR_NOT_READY;
    content_id = source->content_id;
  }
  return create_shader_from_library_content_id(renderer, library, content_id, shader);
}

granit_result renderer_registry::create_shader_from_library_content_id(
    granit_renderer renderer, granit_shader_library library,
    const granit::shader_content_id& content_id, granit_shader& shader) {
  try {
    std::shared_ptr<shader_library_record> library_record;
    {
      std::lock_guard lock{mutex_};
      const auto found_renderer = backend_renderers_.find(renderer);
      if (found_renderer == backend_renderers_.end() ||
          handles_.find(renderer, resource_type::renderer, 0) == nullptr)
        return GRANIT_ERROR_INVALID_HANDLE;
      const auto found = shader_libraries_.find(library);
      if (found == shader_libraries_.end() || found->second->owner != found_renderer->second ||
          handles_.find(library, resource_type::shader_library, found_renderer->second->domain()) ==
              nullptr)
        return GRANIT_ERROR_INVALID_HANDLE;
      library_record = found->second;
    }

    std::lock_guard library_lock{library_record->mutex};
    const auto cached = std::ranges::find_if(library_record->shaders, [&](const auto& candidate) {
      return candidate.first == content_id;
    });
    if (cached != library_record->shaders.end()) {
      std::lock_guard lock{mutex_};
      const auto found = shader_libraries_.find(library);
      if (found == shader_libraries_.end() || found->second != library_record)
        return GRANIT_ERROR_INVALID_HANDLE;
      const auto handle = handles_.insert(cached->second.get(), resource_type::shader,
                                          library_record->owner->domain());
      if (handle == GRANIT_NULL_HANDLE)
        return GRANIT_ERROR_OUT_OF_MEMORY;
      try {
        shaders_.emplace(handle, cached->second);
      } catch (...) {
        static_cast<void>(
            handles_.erase(handle, resource_type::shader, library_record->owner->domain()));
        throw;
      }
      shader = handle;
      return GRANIT_SUCCESS;
    }

    const auto* source =
        shader_format::find_shader_library_shader(library_record->view, content_id);
    if (source == nullptr)
      return GRANIT_ERROR_NOT_READY;
    const auto backend = library_record->owner->backend() == GRANIT_RENDERER_BACKEND_VULKAN
                             ? shader_format::shader_object_backend::vulkan
                             : shader_format::shader_object_backend::webgpu;
    const auto& capabilities = library_record->owner->capabilities();
    const auto variant = std::ranges::find_if(source->variants, [&](const auto& candidate) {
      return candidate.backend == backend && candidate.profile == shader_profile::portable &&
             (candidate.required_features & ~capabilities.shader_features) == 0;
    });
    if (variant == source->variants.end())
      return GRANIT_ERROR_UNSUPPORTED;
    const auto& payload = library_record->view.payloads[variant->payload_index];
    if (sha256_bytes(payload.bytes) != payload.digest || payload.digest != variant->payload_digest)
      return GRANIT_ERROR_INVALID_ARGUMENT;

    granit_result result = GRANIT_ERROR_UNSUPPORTED;
    if (backend == shader_format::shader_object_backend::vulkan &&
        variant->code_format == shader_code_format::spirv) {
      result = create_shader_from_code(renderer, static_cast<granit_shader_stage>(source->stage),
                                       GRANIT_SHADER_CODE_FORMAT_SPIRV, payload.bytes,
                                       source->entry_point, shader);
    } else if (backend == shader_format::shader_object_backend::webgpu &&
               variant->code_format == shader_code_format::wgsl) {
      result = create_shader_from_code(renderer, static_cast<granit_shader_stage>(source->stage),
                                       GRANIT_SHADER_CODE_FORMAT_WGSL, payload.bytes,
                                       source->entry_point, shader);
    }
    if (result != GRANIT_SUCCESS)
      return result;

    std::shared_ptr<shader_record> shader_record;
    {
      std::lock_guard lock{mutex_};
      const auto found = shaders_.find(shader);
      if (found == shaders_.end())
        return GRANIT_ERROR_INTERNAL;
      shader_record = found->second;
      shader_record->content_id = content_id;
    }
    try {
      library_record->shaders.emplace_back(content_id, std::move(shader_record));
    } catch (...) {
      static_cast<void>(destroy_shader(renderer, shader));
      shader = GRANIT_NULL_HANDLE;
      throw;
    }
    return GRANIT_SUCCESS;
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result renderer_registry::destroy_shader_library(granit_renderer renderer,
                                                        granit_shader_library library) {
  std::shared_ptr<shader_library_record> record;
  {
    std::lock_guard lock{mutex_};
    const auto found_renderer = backend_renderers_.find(renderer);
    if (found_renderer == backend_renderers_.end() ||
        handles_.find(renderer, resource_type::renderer, 0) == nullptr)
      return GRANIT_ERROR_INVALID_HANDLE;
    const auto found = shader_libraries_.find(library);
    if (found == shader_libraries_.end() || found->second->owner != found_renderer->second ||
        handles_.find(library, resource_type::shader_library, found_renderer->second->domain()) ==
            nullptr)
      return GRANIT_ERROR_INVALID_HANDLE;
    record = found->second;
  }

  std::lock_guard library_lock{record->mutex};
  std::lock_guard lock{mutex_};
  const auto found = shader_libraries_.find(library);
  if (found == shader_libraries_.end() || found->second != record)
    return GRANIT_ERROR_INVALID_HANDLE;
  // GPU 延迟回收会保留已销毁 Shader 的引用；只检查仍有效的 Shader 与 Pipeline 依赖。
  if (std::ranges::any_of(record->shaders, [&](const auto& cached) {
        const auto* shader = cached.second.get();
        return std::ranges::any_of(shaders_,
                                   [&](const auto& live) { return live.second.get() == shader; }) ||
               std::ranges::any_of(graphics_pipelines_,
                                   [&](const auto& live) {
                                     return live.second->vertex_shader.get() == shader ||
                                            live.second->fragment_shader.get() == shader;
                                   }) ||
               std::ranges::any_of(compute_pipelines_, [&](const auto& live) {
                 return live.second->compute_shader.get() == shader;
               });
      }))
    return GRANIT_ERROR_RESOURCE_IN_USE;
  const auto result =
      handles_.erase(library, resource_type::shader_library, record->owner->domain());
  if (result != GRANIT_SUCCESS)
    return result;
  shader_libraries_.erase(found);
  record->shaders.clear();
  return GRANIT_SUCCESS;
}

} // namespace granit::detail
