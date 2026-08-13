// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_PIPELINE_EMBEDDED_SHADERS_H
#define GRANIT_PIPELINE_EMBEDDED_SHADERS_H

#include <cstddef>
#include <span>

namespace granit::pipeline::detail {

[[nodiscard]] std::span<const std::byte> tone_mapping_vertex_shader() noexcept;
[[nodiscard]] std::span<const std::byte> tone_mapping_fragment_shader() noexcept;

} // namespace granit::pipeline::detail

#endif
