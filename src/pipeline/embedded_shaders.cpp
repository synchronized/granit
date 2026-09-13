// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "pipeline/embedded_shaders.h"

#include <cstdint>

namespace granit::pipeline::detail {
namespace {

alignas(std::uint32_t) constexpr std::uint8_t render_pipeline_library_bytes[]{
#include "render_pipeline_builtin.grshlib.inc"
};
alignas(std::uint32_t) constexpr std::uint8_t debug_draw_library_bytes[]{
#include "debug_draw.grshlib.inc"
};

#include "render_pipeline_shader_ids.inc"
#include "debug_draw_shader_ids.inc"

alignas(std::uint32_t) constexpr std::uint8_t canvas_material_bytes[]{
#include "granit_pipeline_canvas.grmat.inc"
};
alignas(std::uint32_t) constexpr std::uint8_t canvas_shader_library_bytes[]{
#include "unlit_canvas.grshlib.inc"
};

} // namespace

std::span<const std::byte> render_pipeline_shader_library() noexcept {
  return {reinterpret_cast<const std::byte*>(render_pipeline_library_bytes),
          sizeof(render_pipeline_library_bytes)};
}

const shader_content_id& tone_mapping_vertex_shader_id() noexcept { return tone_mapping_vertex_id; }

const shader_content_id& tone_mapping_fragment_shader_id() noexcept {
  return tone_mapping_fragment_id;
}

const shader_content_id& shadow_depth_vertex_shader_id() noexcept { return shadow_depth_vertex_id; }

const shader_content_id& shadow_depth_fragment_shader_id() noexcept {
  return shadow_depth_fragment_id;
}

std::span<const std::byte> debug_draw_shader_library() noexcept {
  return {reinterpret_cast<const std::byte*>(debug_draw_library_bytes),
          sizeof(debug_draw_library_bytes)};
}

const shader_content_id& debug_world_vertex_shader_id() noexcept { return debug_world_vertex_id; }

const shader_content_id& debug_world_fragment_shader_id(bool encode_srgb) noexcept {
  return encode_srgb ? debug_world_srgb_fragment_id : debug_world_fragment_id;
}

std::span<const std::byte> canvas_material_package() noexcept {
  return {reinterpret_cast<const std::byte*>(canvas_material_bytes), sizeof(canvas_material_bytes)};
}

std::span<const std::byte> canvas_shader_library() noexcept {
  return {reinterpret_cast<const std::byte*>(canvas_shader_library_bytes),
          sizeof(canvas_shader_library_bytes)};
}

} // namespace granit::pipeline::detail
