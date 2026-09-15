// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/renderer/shader_library.h>

#include "renderer/renderer_registry.h"
#include "asset_formats/shader/shader_library.h"

#include <cstring>
#include <limits>
#include <new>

namespace {

granit_result validate_info(granit_shader_library_info* info) noexcept {
  if (info == nullptr || info->struct_size < GRANIT_SHADER_LIBRARY_INFO_VERSION_1_SIZE ||
      info->reserved != 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  return GRANIT_SUCCESS;
}

granit_result fill_info(const granit::detail::shader_format::shader_library_view& library,
                        std::uint64_t archive_size, granit_shader_library_info& info) noexcept {
  std::uint64_t variant_count = 0;
  for (const auto& shader : library.shaders)
    variant_count += shader.variants.size();
  if (library.shaders.size() > UINT32_MAX || library.payloads.size() > UINT32_MAX ||
      variant_count > UINT32_MAX)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  info.backend_flags = library.backend_mask;
  std::memcpy(info.content_digest, library.content_digest.data(), library.content_digest.size());
  info.shader_count = static_cast<std::uint32_t>(library.shaders.size());
  info.variant_count = static_cast<std::uint32_t>(variant_count);
  info.payload_count = static_cast<std::uint32_t>(library.payloads.size());
  info.reserved = 0;
  info.archive_size = archive_size;
  return GRANIT_SUCCESS;
}

} // namespace

extern "C" granit_result granit_shader_library_inspect(const void* archive_data,
                                                       uint64_t archive_size,
                                                       granit_shader_library_info* info) {
  if (archive_data == nullptr || archive_size == 0 ||
      archive_size > std::numeric_limits<std::size_t>::max() ||
      validate_info(info) != GRANIT_SUCCESS)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    granit::detail::shader_format::shader_library_view library;
    const auto archive = std::span{static_cast<const std::byte*>(archive_data),
                                   static_cast<std::size_t>(archive_size)};
    const auto result = granit::detail::shader_format::decode_shader_library(archive, library);
    if (result == granit::detail::shader_format::shader_library_error::unsupported_schema)
      return GRANIT_ERROR_UNSUPPORTED;
    if (result != granit::detail::shader_format::shader_library_error::success)
      return GRANIT_ERROR_INVALID_ARGUMENT;
    return fill_info(library, archive_size, *info);
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

extern "C" granit_result granit_shader_library_create(granit_renderer renderer,
                                                      const granit_shader_library_desc* desc,
                                                      granit_shader_library* library) {
  if (library == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  *library = GRANIT_NULL_HANDLE;
  if (renderer == GRANIT_NULL_HANDLE)
    return GRANIT_ERROR_INVALID_HANDLE;
  if (desc == nullptr || desc->struct_size < GRANIT_SHADER_LIBRARY_DESC_VERSION_1_SIZE ||
      desc->reserved != 0 || desc->archive_data == nullptr || desc->archive_size == 0 ||
      desc->archive_size > std::numeric_limits<std::size_t>::max())
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    const auto archive = std::span{static_cast<const std::byte*>(desc->archive_data),
                                   static_cast<std::size_t>(desc->archive_size)};
    return granit::detail::renderer_registry::instance().create_shader_library(renderer, archive,
                                                                               *library);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

extern "C" granit_result granit_shader_library_get_info(granit_renderer renderer,
                                                        granit_shader_library library,
                                                        granit_shader_library_info* info) {
  if (validate_info(info) != GRANIT_SUCCESS)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  if (renderer == GRANIT_NULL_HANDLE || library == GRANIT_NULL_HANDLE)
    return GRANIT_ERROR_INVALID_HANDLE;
  try {
    return granit::detail::renderer_registry::instance().get_shader_library_info(renderer, library,
                                                                                 *info);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

extern "C" granit_result
granit_shader_create_from_library(granit_renderer renderer, granit_shader_library library,
                                  const granit_shader_content_id content_id,
                                  granit_shader* shader) {
  if (shader == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  *shader = GRANIT_NULL_HANDLE;
  if (renderer == GRANIT_NULL_HANDLE || library == GRANIT_NULL_HANDLE)
    return GRANIT_ERROR_INVALID_HANDLE;
  if (content_id == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    granit::shader_content_id id{};
    std::memcpy(id.data(), content_id, id.size());
    return granit::detail::renderer_registry::instance().create_shader_from_library(
        renderer, library, id, *shader);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

extern "C" granit_result granit_shader_library_destroy(granit_renderer renderer,
                                                       granit_shader_library library) {
  if (renderer == GRANIT_NULL_HANDLE || library == GRANIT_NULL_HANDLE)
    return GRANIT_ERROR_INVALID_HANDLE;
  try {
    return granit::detail::renderer_registry::instance().destroy_shader_library(renderer, library);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}
