// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "model_viewer/material_archive.h"
#include "assets/shader_asset.h"

#include <array>
#include <cstdint>
#include <cstring>

namespace granit::example::model_viewer {
namespace {

alignas(std::uint32_t) constexpr std::uint8_t archive_bytes[]{
#include "model_viewer_pbr.grmat.inc"
};

alignas(std::uint32_t) constexpr std::uint8_t vertex_manifest[]{
#include "model_viewer_pbr.vert.grshader.inc"
};
alignas(std::uint32_t) constexpr std::uint8_t vertex_spirv[]{
#include "model_viewer_pbr.vert.grshader.spv.inc"
};
alignas(std::uint32_t) constexpr std::uint8_t vertex_wgsl[]{
#include "model_viewer_pbr.vert.grshader.wgsl.inc"
};
alignas(std::uint32_t) constexpr std::uint8_t fragment_manifest[]{
#include "model_viewer_pbr.frag.grshader.inc"
};
alignas(std::uint32_t) constexpr std::uint8_t fragment_spirv[]{
#include "model_viewer_pbr.frag.grshader.spv.inc"
};
alignas(std::uint32_t) constexpr std::uint8_t fragment_wgsl[]{
#include "model_viewer_pbr.frag.grshader.wgsl.inc"
};

struct embedded_shader_asset {
  std::span<const std::uint8_t> manifest;
  std::span<const std::uint8_t> spirv;
  std::span<const std::uint8_t> wgsl;
};

constexpr std::array embedded_assets{
    embedded_shader_asset{vertex_manifest, vertex_spirv, vertex_wgsl},
    embedded_shader_asset{fragment_manifest, fragment_spirv, fragment_wgsl}};

} // namespace

std::span<const std::byte> model_viewer_material_archive() noexcept {
  return {reinterpret_cast<const std::byte*>(archive_bytes), sizeof(archive_bytes)};
}

granit_result resolve_model_viewer_shader(void*, const std::uint8_t asset_id[32],
                                          granit_renderer_backend backend, std::uint32_t profile,
                                          granit_shader_asset_desc* asset) noexcept {
  if (asset_id == nullptr || asset == nullptr || profile != GRANIT_SHADER_PROFILE_PORTABLE)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  for (const auto& candidate : embedded_assets) {
    granit::tools::shader_asset_view view;
    const auto manifest = std::as_bytes(candidate.manifest);
    if (granit::tools::decode_shader_asset(manifest, view) !=
            granit::tools::shader_asset_error::success ||
        std::memcmp(view.content_id.data(), asset_id, view.content_id.size()) != 0) {
      continue;
    }
    const auto sidecar = backend == GRANIT_RENDERER_BACKEND_VULKAN
                             ? candidate.spirv
                             : backend == GRANIT_RENDERER_BACKEND_WEBGPU ? candidate.wgsl
                                                                         : std::span<const std::uint8_t>{};
    if (sidecar.empty())
      return GRANIT_ERROR_UNSUPPORTED;
    *asset = GRANIT_SHADER_ASSET_DESC_INIT;
    asset->manifest_data = candidate.manifest.data();
    asset->manifest_size = candidate.manifest.size();
    asset->sidecar_data = sidecar.data();
    asset->sidecar_size = sidecar.size();
    return GRANIT_SUCCESS;
  }
  return GRANIT_ERROR_NOT_READY;
}

} // namespace granit::example::model_viewer
