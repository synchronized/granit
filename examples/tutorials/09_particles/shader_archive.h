// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_EXAMPLES_TUTORIALS_09_PARTICLES_SHADER_ARCHIVE_H_
#define GRANIT_EXAMPLES_TUTORIALS_09_PARTICLES_SHADER_ARCHIVE_H_

#include <cstddef>
#include <span>

namespace tutorial_particles {
[[nodiscard]] std::span<const std::byte> shader_archive() noexcept;
}

#endif
