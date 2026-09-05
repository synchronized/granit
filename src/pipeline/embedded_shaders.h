// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_PIPELINE_EMBEDDED_SHADERS_H
#define GRANIT_PIPELINE_EMBEDDED_SHADERS_H

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

#include <granit/pipeline/material.h>

namespace granit::pipeline::detail {

[[nodiscard]] std::span<const std::byte> tone_mapping_vertex_shader() noexcept;
[[nodiscard]] std::span<const std::byte> tone_mapping_fragment_shader() noexcept;
[[nodiscard]] std::string_view tone_mapping_wgsl() noexcept;
[[nodiscard]] std::span<const std::byte> shadow_depth_vertex_shader() noexcept;
[[nodiscard]] std::span<const std::byte> shadow_depth_fragment_shader() noexcept;
[[nodiscard]] std::string_view shadow_depth_vertex_wgsl() noexcept;
[[nodiscard]] std::string_view shadow_depth_fragment_wgsl() noexcept;
[[nodiscard]] std::span<const std::byte> canvas_material_package() noexcept;
granit_result resolve_canvas_shader(void* user_data, const std::uint8_t asset_id[32],
                                    granit_renderer_backend backend, std::uint32_t profile,
                                    granit_shader_asset_desc* asset) noexcept;
[[nodiscard]] std::span<const std::byte> debug_world_vertex_shader() noexcept;
[[nodiscard]] std::span<const std::byte> debug_world_fragment_shader(bool encode_srgb) noexcept;

} // namespace granit::pipeline::detail

#endif
