// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "pipeline/embedded_shaders.h"

#include <cstdint>

namespace granit::pipeline::detail {
namespace {

alignas(std::uint32_t) constexpr std::uint8_t tone_mapping_vertex_bytes[]{
#include "granit_pipeline_tone_mapping.vert.inc"
};

alignas(std::uint32_t) constexpr std::uint8_t tone_mapping_fragment_bytes[]{
#include "granit_pipeline_tone_mapping.frag.inc"
};

alignas(std::uint32_t) constexpr std::uint8_t shadow_depth_vertex_bytes[]{
#include "granit_pipeline_shadow_depth.vert.inc"
};

alignas(std::uint32_t) constexpr std::uint8_t shadow_depth_fragment_bytes[]{
#include "granit_pipeline_shadow_depth.frag.inc"
};

} // namespace

std::span<const std::byte> tone_mapping_vertex_shader() noexcept {
  return {reinterpret_cast<const std::byte*>(tone_mapping_vertex_bytes),
          sizeof(tone_mapping_vertex_bytes)};
}

std::span<const std::byte> tone_mapping_fragment_shader() noexcept {
  return {reinterpret_cast<const std::byte*>(tone_mapping_fragment_bytes),
          sizeof(tone_mapping_fragment_bytes)};
}

std::span<const std::byte> shadow_depth_vertex_shader() noexcept {
  return {reinterpret_cast<const std::byte*>(shadow_depth_vertex_bytes),
          sizeof(shadow_depth_vertex_bytes)};
}

std::span<const std::byte> shadow_depth_fragment_shader() noexcept {
  return {reinterpret_cast<const std::byte*>(shadow_depth_fragment_bytes),
          sizeof(shadow_depth_fragment_bytes)};
}

} // namespace granit::pipeline::detail
