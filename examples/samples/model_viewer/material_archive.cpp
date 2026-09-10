// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "material_archive.h"
#include "assets/shader_library.h"

#include <array>
#include <cstdint>

namespace granit::example::model_viewer {
namespace {

alignas(std::uint32_t) constexpr std::uint8_t archive_bytes[]{
#include "model_viewer_pbr.grmat.inc"
};

alignas(std::uint32_t) constexpr std::uint8_t vertex_manifest[]{
#include "pbr_standard.vert.grshader.inc"
};
alignas(std::uint32_t) constexpr std::uint8_t vertex_spirv[]{
#include "pbr_standard.vert.grshader.spv.inc"
};
alignas(std::uint32_t) constexpr std::uint8_t vertex_wgsl[]{
#include "pbr_standard.vert.grshader.wgsl.inc"
};
alignas(std::uint32_t) constexpr std::uint8_t fragment_manifest[]{
#include "pbr_standard.frag.grshader.inc"
};
alignas(std::uint32_t) constexpr std::uint8_t fragment_spirv[]{
#include "pbr_standard.frag.grshader.spv.inc"
};
alignas(std::uint32_t) constexpr std::uint8_t fragment_wgsl[]{
#include "pbr_standard.frag.grshader.wgsl.inc"
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

granit_result build_model_viewer_shader_library(std::vector<std::byte>& output) noexcept {
  std::array<granit::tools::shader_library_asset_source, embedded_assets.size()> sources{};
  for (std::size_t index = 0; index < embedded_assets.size(); ++index) {
    const auto& asset = embedded_assets[index];
    sources[index] = {std::as_bytes(asset.manifest), std::as_bytes(asset.wgsl),
                      std::as_bytes(asset.spirv)};
  }
  return granit::tools::encode_shader_library({sources, granit::tools::shader_library_backend_all},
                                              output) ==
                 granit::tools::shader_library_error::success
             ? GRANIT_SUCCESS
             : GRANIT_ERROR_INTERNAL;
}

} // namespace granit::example::model_viewer
