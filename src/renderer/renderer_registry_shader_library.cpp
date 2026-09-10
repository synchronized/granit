// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "renderer/renderer_registry.h"
#include "renderer/renderer_registry_records.h"

#include "assets/shader_library.h"

#include <algorithm>
#include <cstring>
#include <limits>
#include <new>

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
    const auto decode_result = tools::decode_shader_library(archive, record->view);
    if (decode_result == tools::shader_library_error::unsupported_schema)
      return GRANIT_ERROR_UNSUPPORTED;
    if (decode_result != tools::shader_library_error::success)
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

granit_result renderer_registry::destroy_shader_library(granit_renderer renderer,
                                                        granit_shader_library library) {
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
  shader_libraries_.erase(found);
  return handles_.erase(library, resource_type::shader_library, owner->domain());
}

} // namespace granit::detail
