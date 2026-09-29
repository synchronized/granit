// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_EXAMPLES_COMMON_GLTF_RENDERING_STANDARD_PBR_ASSETS_H_
#define GRANIT_EXAMPLES_COMMON_GLTF_RENDERING_STANDARD_PBR_ASSETS_H_

#include <cstddef>
#include <cstdint>
#include <span>

#include <granit/pipeline/material.h>

namespace granit::example::gltf_rendering {

/** 返回编译期内嵌的跨后端 PBR 材质归档。 */
[[nodiscard]] std::span<const std::byte> standard_pbr_material_archive() noexcept;

/** 返回编译期内嵌的 PBR Shader Library。 */
[[nodiscard]] std::span<const std::byte> standard_pbr_shader_library() noexcept;

} // namespace granit::example::gltf_rendering

#endif
