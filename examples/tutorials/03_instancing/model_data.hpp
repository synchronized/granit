// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_EXAMPLES_TUTORIALS_03_INSTANCING_MODEL_DATA_HPP_
#define GRANIT_EXAMPLES_TUTORIALS_03_INSTANCING_MODEL_DATA_HPP_

#include <array>
#include <cstdint>

namespace tutorial_instancing {

struct vertex {
  float position[3];
};

inline constexpr std::array<vertex, 8> vertices{{
    {{-1, -1, -1}},
    {{1, -1, -1}},
    {{1, 1, -1}},
    {{-1, 1, -1}},
    {{-1, -1, 1}},
    {{1, -1, 1}},
    {{1, 1, 1}},
    {{-1, 1, 1}},
}};

inline constexpr std::array<std::uint16_t, 36> indices{
    0, 2, 1, 0, 3, 2, 4, 5, 6, 4, 6, 7, 0, 4, 7, 0, 7, 3,
    1, 2, 6, 1, 6, 5, 3, 7, 6, 3, 6, 2, 0, 1, 5, 0, 5, 4,
};

} // namespace tutorial_instancing

#endif
