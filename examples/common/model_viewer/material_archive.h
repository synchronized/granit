// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_EXAMPLES_COMMON_MODEL_VIEWER_MATERIAL_ARCHIVE_H_
#define GRANIT_EXAMPLES_COMMON_MODEL_VIEWER_MATERIAL_ARCHIVE_H_

#include <cstddef>
#include <granit/pipeline/material.h>
#include <span>

namespace granit::example::model_viewer {

/** 返回编译期内嵌的跨后端 PBR 材质归档。 */
[[nodiscard]] std::span<const std::byte> model_viewer_material_archive() noexcept;

/** 从编译期内嵌存储解析模型查看器材质引用的 Shader Asset。 */
granit_result resolve_model_viewer_shader(void* user_data, const std::uint8_t asset_id[32],
                                          granit_renderer_backend backend, std::uint32_t profile,
                                          granit_shader_asset_desc* asset) noexcept;

} // namespace granit::example::model_viewer

#endif
