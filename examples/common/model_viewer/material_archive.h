// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_EXAMPLES_COMMON_MODEL_VIEWER_MATERIAL_ARCHIVE_H_
#define GRANIT_EXAMPLES_COMMON_MODEL_VIEWER_MATERIAL_ARCHIVE_H_

#include <cstddef>
#include <span>

namespace granit::example::model_viewer {

/** 返回编译期内嵌的跨后端 PBR 材质归档。 */
[[nodiscard]] std::span<const std::byte> model_viewer_material_archive() noexcept;

} // namespace granit::example::model_viewer

#endif
