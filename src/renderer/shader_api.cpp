// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/renderer/shader.h>

#include "renderer/renderer_registry.h"
#include "assets/shader_asset.h"
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
  if (desc == nullptr || desc->struct_size < GRANIT_SHADER_ASSET_DESC_SIZE ||
      desc->reserved != 0 || desc->manifest_data == nullptr || desc->manifest_size == 0 ||
      desc->sidecar_data == nullptr || desc->sidecar_size == 0 ||
      desc->manifest_size > std::numeric_limits<std::size_t>::max() ||
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
    if (variant == nullptr ||
        (variant->required_features & ~capabilities.supported_features) != 0)
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

extern "C" granit_result granit_shader_destroy(granit_renderer renderer, granit_shader shader) {
  if (renderer == GRANIT_NULL_HANDLE || shader == GRANIT_NULL_HANDLE)
    return GRANIT_ERROR_INVALID_HANDLE;
  try {
    return granit::detail::renderer_registry::instance().destroy_shader(renderer, shader);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}
