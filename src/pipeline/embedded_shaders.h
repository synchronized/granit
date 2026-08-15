// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_PIPELINE_EMBEDDED_SHADERS_H
#define GRANIT_PIPELINE_EMBEDDED_SHADERS_H

#include <cstddef>
#include <span>

namespace granit::pipeline::detail {

[[nodiscard]] std::span<const std::byte> tone_mapping_vertex_shader() noexcept;
[[nodiscard]] std::span<const std::byte> tone_mapping_fragment_shader() noexcept;
[[nodiscard]] std::span<const std::byte> shadow_depth_vertex_shader() noexcept;
[[nodiscard]] std::span<const std::byte> shadow_depth_fragment_shader() noexcept;
[[nodiscard]] std::span<const std::byte> canvas_material_package() noexcept;
[[nodiscard]] std::span<const std::byte> debug_world_vertex_shader() noexcept;
[[nodiscard]] std::span<const std::byte> debug_world_fragment_shader(bool encode_srgb) noexcept;

} // namespace granit::pipeline::detail

#endif
