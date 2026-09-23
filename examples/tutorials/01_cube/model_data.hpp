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

inline constexpr std::uint32_t crate_texture_extent = 64;

/** 生成仓库自有的木箱风格 RGBA 纹理，避免教程依赖运行时图片解码。 */
constexpr auto make_crate_pixels() {
  std::array<std::uint8_t, crate_texture_extent * crate_texture_extent * 4> pixels{};
  for (std::uint32_t y = 0; y < crate_texture_extent; ++y) {
    for (std::uint32_t x = 0; x < crate_texture_extent; ++x) {
      const auto offset = (y * crate_texture_extent + x) * 4;
      const auto grain = static_cast<std::uint8_t>((x * 13U + y * 7U + (x * y) % 17U) % 24U);
      std::uint8_t red = static_cast<std::uint8_t>(132U + grain);
      std::uint8_t green = static_cast<std::uint8_t>(72U + grain / 2U);
      std::uint8_t blue = static_cast<std::uint8_t>(30U + grain / 3U);

      const bool frame =
          x < 5U || y < 5U || x >= crate_texture_extent - 5U || y >= crate_texture_extent - 5U;
      const auto diagonal_a = x > y ? x - y : y - x;
      const auto reverse_x = crate_texture_extent - 1U - x;
      const auto diagonal_b = reverse_x > y ? reverse_x - y : y - reverse_x;
      const bool brace = diagonal_a < 3U || diagonal_b < 3U;
      const bool plank_seam = y % 16U == 0U || y % 16U == 1U;
      if (frame || brace) {
        red = 92U;
        green = 48U;
        blue = 20U;
      } else if (plank_seam) {
        red = 70U;
        green = 36U;
        blue = 16U;
      }
      pixels[offset] = red;
      pixels[offset + 1] = green;
      pixels[offset + 2] = blue;
      pixels[offset + 3] = 255U;
    }
  }
  return pixels;
}

} // namespace tutorial_model

#endif
