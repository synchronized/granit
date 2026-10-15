// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_EXAMPLES_TUTORIALS_10_TRANSPARENCY_MODEL_DATA_HPP_
#define GRANIT_EXAMPLES_TUTORIALS_10_TRANSPARENCY_MODEL_DATA_HPP_

#include <array>
#include <cstdint>

namespace tutorial_transparency {
struct vertex {
  float position[3];
  float normal[3];
};
inline constexpr std::array<vertex, 24> vertices{{
    {{-1, -1, -1}, {0, 0, -1}}, {{1, -1, -1}, {0, 0, -1}},
    {{1, 1, -1}, {0, 0, -1}}, {{-1, 1, -1}, {0, 0, -1}},
    {{-1, -1, 1}, {0, 0, 1}}, {{1, -1, 1}, {0, 0, 1}},
    {{1, 1, 1}, {0, 0, 1}}, {{-1, 1, 1}, {0, 0, 1}},
    {{-1, -1, -1}, {-1, 0, 0}}, {{-1, -1, 1}, {-1, 0, 0}},
    {{-1, 1, 1}, {-1, 0, 0}}, {{-1, 1, -1}, {-1, 0, 0}},
    {{1, -1, -1}, {1, 0, 0}}, {{1, 1, -1}, {1, 0, 0}},
    {{1, 1, 1}, {1, 0, 0}}, {{1, -1, 1}, {1, 0, 0}},
    {{-1, 1, -1}, {0, 1, 0}}, {{-1, 1, 1}, {0, 1, 0}},
    {{1, 1, 1}, {0, 1, 0}}, {{1, 1, -1}, {0, 1, 0}},
    {{-1, -1, -1}, {0, -1, 0}}, {{1, -1, -1}, {0, -1, 0}},
    {{1, -1, 1}, {0, -1, 0}}, {{-1, -1, 1}, {0, -1, 0}},
}};
inline constexpr std::array<std::uint16_t, 36> indices{
    0, 2, 1, 0, 3, 2, 4, 5, 6, 4, 6, 7, 8, 12, 15, 8, 15, 11,
    12, 13, 14, 12, 14, 15, 16, 19, 18, 16, 18, 17, 20, 21, 22, 20, 22, 23,
};
} // namespace tutorial_transparency

#endif
