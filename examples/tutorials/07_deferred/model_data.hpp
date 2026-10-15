// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_EXAMPLES_TUTORIALS_07_DEFERRED_MODEL_DATA_HPP_
#define GRANIT_EXAMPLES_TUTORIALS_07_DEFERRED_MODEL_DATA_HPP_

#include <array>
#include <cstdint>

namespace tutorial_deferred {

struct vertex {
  float position[3];
  float normal[3];
};

inline constexpr std::array<vertex, 24> vertices{{
    {{-1, -1, -1}, {0, 0, -1}}, {{1, -1, -1}, {0, 0, -1}}, {{1, 1, -1}, {0, 0, -1}},
    {{-1, 1, -1}, {0, 0, -1}},  {{-1, -1, 1}, {0, 0, 1}},  {{1, -1, 1}, {0, 0, 1}},
    {{1, 1, 1}, {0, 0, 1}},     {{-1, 1, 1}, {0, 0, 1}},   {{-1, -1, -1}, {-1, 0, 0}},
    {{-1, -1, 1}, {-1, 0, 0}},  {{-1, 1, 1}, {-1, 0, 0}},  {{-1, 1, -1}, {-1, 0, 0}},
    {{1, -1, -1}, {1, 0, 0}},   {{1, 1, -1}, {1, 0, 0}},   {{1, 1, 1}, {1, 0, 0}},
    {{1, -1, 1}, {1, 0, 0}},    {{-1, 1, -1}, {0, 1, 0}},  {{-1, 1, 1}, {0, 1, 0}},
    {{1, 1, 1}, {0, 1, 0}},     {{1, 1, -1}, {0, 1, 0}},   {{-1, -1, -1}, {0, -1, 0}},
    {{1, -1, -1}, {0, -1, 0}},  {{1, -1, 1}, {0, -1, 0}},  {{-1, -1, 1}, {0, -1, 0}},
}};

inline constexpr std::array<std::uint16_t, 36> indices{
    // Granit 逻辑裁剪空间的正面绕序与原生窗口坐标不同；背面剔除使用此顺序。
    0,  1,  2,  0,  2,  3,  4,  7,  6,  4,  6,  5, 8,  11, 10, 8,  10, 9,
    12, 15, 14, 12, 14, 13, 16, 19, 18, 16, 18, 17, 20, 23, 22, 20, 22, 21,
};

} // namespace tutorial_deferred

#endif
