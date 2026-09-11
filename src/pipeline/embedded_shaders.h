// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_PIPELINE_EMBEDDED_SHADERS_H
#define GRANIT_PIPELINE_EMBEDDED_SHADERS_H

#include <granit/renderer/shader_library.h>

#include <array>
#include <cstddef>
#include <span>

namespace granit::pipeline::detail {

using shader_content_id = std::array<std::byte, GRANIT_SHADER_LIBRARY_CONTENT_DIGEST_SIZE>;

[[nodiscard]] std::span<const std::byte> render_pipeline_shader_library() noexcept;
[[nodiscard]] const shader_content_id& tone_mapping_vertex_shader_id() noexcept;
[[nodiscard]] const shader_content_id& tone_mapping_fragment_shader_id() noexcept;
[[nodiscard]] const shader_content_id& shadow_depth_vertex_shader_id() noexcept;
[[nodiscard]] const shader_content_id& shadow_depth_fragment_shader_id() noexcept;

[[nodiscard]] std::span<const std::byte> debug_draw_shader_library() noexcept;
[[nodiscard]] const shader_content_id& debug_world_vertex_shader_id() noexcept;
[[nodiscard]] const shader_content_id& debug_world_fragment_shader_id(bool encode_srgb) noexcept;

[[nodiscard]] std::span<const std::byte> canvas_material_package() noexcept;
[[nodiscard]] std::span<const std::byte> canvas_shader_library() noexcept;

} // namespace granit::pipeline::detail

#endif
