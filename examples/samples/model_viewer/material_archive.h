// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_EXAMPLES_SAMPLES_MODEL_VIEWER_MATERIAL_ARCHIVE_H_
#define GRANIT_EXAMPLES_SAMPLES_MODEL_VIEWER_MATERIAL_ARCHIVE_H_

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include <granit/pipeline/material.h>

namespace granit::example::model_viewer {

/** 返回编译期内嵌的跨后端 PBR 材质归档。 */
[[nodiscard]] std::span<const std::byte> model_viewer_material_archive() noexcept;

/** 从编译期内嵌的离线对象构建模型查看器 Shader Library。 */
[[nodiscard]] granit_result
build_model_viewer_shader_library(std::vector<std::byte>& output) noexcept;

} // namespace granit::example::model_viewer

#endif
