// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_TUTORIAL_05_MODEL_DATA_HPP_
#define GRANIT_TUTORIAL_05_MODEL_DATA_HPP_

#include <array>
#include <cstdint>

namespace tutorial_model {

struct vertex {
  float position[3];
  float uv[2];
};

inline constexpr std::array<vertex, 24> vertices{{
    {{-1, -1, 1}, {0, 1}},  {{1, -1, 1}, {1, 1}},   {{1, 1, 1}, {1, 0}},   {{-1, 1, 1}, {0, 0}},
    {{1, -1, -1}, {0, 1}},  {{-1, -1, -1}, {1, 1}}, {{-1, 1, -1}, {1, 0}}, {{1, 1, -1}, {0, 0}},
    {{-1, -1, -1}, {0, 1}}, {{-1, -1, 1}, {1, 1}},  {{-1, 1, 1}, {1, 0}},  {{-1, 1, -1}, {0, 0}},
    {{1, -1, 1}, {0, 1}},   {{1, -1, -1}, {1, 1}},  {{1, 1, -1}, {1, 0}},  {{1, 1, 1}, {0, 0}},
    {{-1, 1, 1}, {0, 1}},   {{1, 1, 1}, {1, 1}},    {{1, 1, -1}, {1, 0}},  {{-1, 1, -1}, {0, 0}},
    {{-1, -1, -1}, {0, 1}}, {{1, -1, -1}, {1, 1}},  {{1, -1, 1}, {1, 0}},  {{-1, -1, 1}, {0, 0}},
}};

inline constexpr std::array<std::uint16_t, 36> indices{
    0,  1,  2,  0,  2,  3,  4,  5,  6,  4,  6,  7,  8,  9,  10, 8,  10, 11,
    12, 13, 14, 12, 14, 15, 16, 17, 18, 16, 18, 19, 20, 21, 22, 20, 22, 23,
};

} // namespace tutorial_model

#endif
