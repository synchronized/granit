// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_PIPELINE_EMBEDDED_SHADERS_H
#define GRANIT_PIPELINE_EMBEDDED_SHADERS_H

#include <granit/renderer/shader_library.hpp>

#include <cstddef>
#include <span>
#include <string_view>

namespace granit::pipeline::detail {

[[nodiscard]] std::span<const std::byte> render_pipeline_shader_library() noexcept;
[[nodiscard]] std::string_view tone_mapping_vertex_shader_name() noexcept;
[[nodiscard]] std::string_view tone_mapping_fragment_shader_name() noexcept;
[[nodiscard]] std::string_view shadow_depth_vertex_shader_name() noexcept;
[[nodiscard]] std::string_view shadow_depth_fragment_shader_name() noexcept;

[[nodiscard]] std::span<const std::byte> debug_draw_shader_library() noexcept;
[[nodiscard]] std::string_view debug_world_vertex_shader_name() noexcept;
[[nodiscard]] std::string_view debug_world_fragment_shader_name(bool encode_srgb) noexcept;

[[nodiscard]] std::span<const std::byte> canvas_material_package() noexcept;
[[nodiscard]] std::span<const std::byte> canvas_shader_library() noexcept;

} // namespace granit::pipeline::detail

#endif
