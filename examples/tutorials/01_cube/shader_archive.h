// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_EXAMPLES_TUTORIALS_01_CUBE_SHADER_ARCHIVE_H_
#define GRANIT_EXAMPLES_TUTORIALS_01_CUBE_SHADER_ARCHIVE_H_

#include <cstddef>
#include <span>

namespace tutorial_cube {

/** 返回构建期内嵌的跨后端 Shader Library。 */
[[nodiscard]] std::span<const std::byte> shader_archive() noexcept;

} // namespace tutorial_cube

#endif
