// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/renderer/shader.h>

#include "assets/shader_asset.h"
#include "renderer/renderer_registry.h"
#include <cstring>
#include <limits>
#include <new>

extern "C" granit_result granit_shader_create(granit_renderer renderer,
                                              const granit_shader_desc* desc,
                                              granit_shader* shader) {
  if (shader == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  *shader = GRANIT_NULL_HANDLE;
  if (renderer == GRANIT_NULL_HANDLE)
    return GRANIT_ERROR_INVALID_HANDLE;
  if (desc == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    return granit::detail::renderer_registry::instance().create_shader_from_desc(renderer, *desc,
                                                                                 *shader);
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

extern "C" granit_result granit_shader_create_from_asset(granit_renderer renderer,
                                                         const granit_shader_asset_desc* desc,
                                                         granit_shader* shader) {
  if (shader == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  *shader = GRANIT_NULL_HANDLE;
  if (renderer == GRANIT_NULL_HANDLE)
    return GRANIT_ERROR_INVALID_HANDLE;
  if (desc == nullptr || desc->struct_size < GRANIT_SHADER_ASSET_DESC_SIZE || desc->reserved != 0 ||
      desc->manifest_data == nullptr || desc->manifest_size == 0 || desc->sidecar_data == nullptr ||
      desc->sidecar_size == 0 || desc->manifest_size > std::numeric_limits<std::size_t>::max() ||
      desc->sidecar_size > std::numeric_limits<std::size_t>::max())
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    granit::tools::shader_asset_view asset;
    const auto manifest = std::span{static_cast<const std::byte*>(desc->manifest_data),
                                    static_cast<std::size_t>(desc->manifest_size)};
    if (granit::tools::decode_shader_asset(manifest, asset) !=
        granit::tools::shader_asset_error::success)
      return GRANIT_ERROR_INVALID_ARGUMENT;
    granit_renderer_shader_capabilities capabilities = GRANIT_RENDERER_SHADER_CAPABILITIES_INIT;
    auto result = granit::detail::renderer_registry::instance().get_shader_capabilities(
        renderer, capabilities);
    if (result != GRANIT_SUCCESS)
      return result;
    const auto backend = capabilities.backend == GRANIT_RENDERER_BACKEND_VULKAN
                             ? granit::tools::shader_asset_backend::vulkan
                             : granit::tools::shader_asset_backend::webgpu;
    const auto* variant = granit::tools::find_shader_asset_variant(
        asset, backend, granit::tools::shader_asset_profile::portable);
    if (variant == nullptr || (variant->required_features & ~capabilities.supported_features) != 0)
      return GRANIT_ERROR_UNSUPPORTED;
    const auto sidecar = std::span{static_cast<const std::byte*>(desc->sidecar_data),
                                   static_cast<std::size_t>(desc->sidecar_size)};
    if (granit::tools::validate_shader_asset_payload(asset, backend, sidecar) !=
        granit::tools::shader_asset_error::success)
      return GRANIT_ERROR_INVALID_ARGUMENT;
    granit_shader_desc shader_desc = GRANIT_SHADER_DESC_INIT;
    shader_desc.stage = asset.stage;
    shader_desc.entry_point = asset.entry_point.data();
    shader_desc.entry_point_length = static_cast<std::uint32_t>(asset.entry_point.size());
    if (backend == granit::tools::shader_asset_backend::vulkan) {
      shader_desc.code = sidecar.data();
      shader_desc.code_size = sidecar.size();
    } else {
      shader_desc.wgsl = reinterpret_cast<const char*>(sidecar.data());
      shader_desc.wgsl_length = sidecar.size();
    }
    return granit::detail::renderer_registry::instance().create_shader_from_desc(
        renderer, shader_desc, *shader);
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

extern "C" granit_result granit_shader_asset_inspect(const void* manifest_data,
                                                     uint64_t manifest_size,
                                                     granit_shader_asset_info* info) {
  if (manifest_data == nullptr || manifest_size == 0 ||
      manifest_size > std::numeric_limits<std::size_t>::max() || info == nullptr ||
      info->struct_size < GRANIT_SHADER_ASSET_INFO_SIZE || info->reserved != 0 ||
      (info->entry_point == nullptr && info->entry_point_capacity != 0))
    return GRANIT_ERROR_INVALID_ARGUMENT;

  granit::tools::shader_asset_view asset;
  const auto manifest = std::span{static_cast<const std::byte*>(manifest_data),
                                  static_cast<std::size_t>(manifest_size)};
  const auto decoded = granit::tools::decode_shader_asset(manifest, asset);
  if (decoded == granit::tools::shader_asset_error::unsupported_schema)
    return GRANIT_ERROR_UNSUPPORTED;
  if (decoded != granit::tools::shader_asset_error::success)
    return GRANIT_ERROR_INVALID_ARGUMENT;

  info->reserved = 0;
  info->stage = asset.stage;
  info->entry_point_length = static_cast<std::uint32_t>(asset.entry_point.size());
  info->variant_count = asset.variant_count;
  std::memcpy(info->content_id, asset.content_id.data(), asset.content_id.size());
  std::memcpy(info->cache_key, asset.cache_key.data(), asset.cache_key.size());
  for (std::uint32_t index = 0; index < asset.variant_count; ++index) {
    const auto& source = asset.variants[index];
    auto& destination = info->variants[index];
    destination.backend = source.backend == granit::tools::shader_asset_backend::vulkan
                              ? GRANIT_RENDERER_BACKEND_VULKAN
                              : GRANIT_RENDERER_BACKEND_WEBGPU;
    destination.code_format = source.code_format == granit::tools::shader_asset_code_format::spirv
                                  ? GRANIT_SHADER_CODE_FORMAT_SPIRV
                                  : GRANIT_SHADER_CODE_FORMAT_WGSL;
    destination.profile = static_cast<std::uint32_t>(source.profile);
    destination.reserved = 0;
    destination.required_features = source.required_features;
    destination.payload_size = source.byte_size;
    std::memcpy(destination.payload_digest, source.digest.data(), source.digest.size());
  }
  if (info->entry_point == nullptr)
    return info->entry_point_capacity == 0 ? GRANIT_SUCCESS : GRANIT_ERROR_INVALID_ARGUMENT;
  if (info->entry_point_capacity <= asset.entry_point.size())
    return GRANIT_ERROR_INVALID_ARGUMENT;
  std::memcpy(info->entry_point, asset.entry_point.data(), asset.entry_point.size());
  info->entry_point[asset.entry_point.size()] = '\0';
  return GRANIT_SUCCESS;
}

extern "C" granit_result granit_shader_destroy(granit_renderer renderer, granit_shader shader) {
  if (renderer == GRANIT_NULL_HANDLE || shader == GRANIT_NULL_HANDLE)
    return GRANIT_ERROR_INVALID_HANDLE;
  try {
    return granit::detail::renderer_registry::instance().destroy_shader(renderer, shader);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}
