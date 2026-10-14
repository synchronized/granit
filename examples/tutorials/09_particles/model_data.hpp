// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_EXAMPLES_TUTORIALS_09_PARTICLES_MODEL_DATA_HPP_
#define GRANIT_EXAMPLES_TUTORIALS_09_PARTICLES_MODEL_DATA_HPP_

#include <array>
#include <cstdint>

namespace tutorial_particles {

struct vertex {
  float position[3];
};

// 前六个顶点组成低面数八面体，后八个顶点组成可复用的立方体。
inline constexpr std::array<vertex, 14> vertices{{
    {{0.0F, 1.0F, 0.0F}},  {{1.0F, 0.0F, 0.0F}},
    {{0.0F, 0.0F, 1.0F}},  {{-1.0F, 0.0F, 0.0F}},
    {{0.0F, 0.0F, -1.0F}}, {{0.0F, -1.0F, 0.0F}},
    {{-1.0F, -1.0F, -1.0F}}, {{1.0F, -1.0F, -1.0F}},
    {{1.0F, 1.0F, -1.0F}},   {{-1.0F, 1.0F, -1.0F}},
    {{-1.0F, -1.0F, 1.0F}},  {{1.0F, -1.0F, 1.0F}},
    {{1.0F, 1.0F, 1.0F}},    {{-1.0F, 1.0F, 1.0F}},
}};

inline constexpr std::array<std::uint16_t, 60> indices{
    0, 2, 1, 0, 3, 2, 0, 4, 3, 0, 1, 4,
    5, 1, 2, 5, 2, 3, 5, 3, 4, 5, 4, 1,
    0, 2, 1, 0, 3, 2, 4, 5, 6, 4, 6, 7,
    0, 4, 7, 0, 7, 3, 1, 2, 6, 1, 6, 5,
    3, 7, 6, 3, 6, 2, 0, 1, 5, 0, 5, 4,
};

} // namespace tutorial_particles

#endif
