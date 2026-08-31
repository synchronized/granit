// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_TESTS_SUPPORT_RENDERER_FIXTURE_H_
#define GRANIT_TESTS_SUPPORT_RENDERER_FIXTURE_H_

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace granit::test::renderer_fixture {

inline constexpr std::uint32_t width = 64;
inline constexpr std::uint32_t height = 32;
inline constexpr std::uint32_t dynamic_uniform_stride = 256;

inline constexpr std::array<float, 28> vertices{
    -0.22F, -0.30F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F, 0.22F,  -0.30F, 1.0F, 0.0F, 0.0F, 0.0F, 1.0F,
    0.22F,  0.30F,  1.0F, 1.0F, 0.0F, 0.0F, 1.0F, -0.22F, 0.30F,  0.0F, 1.0F, 0.0F, 0.0F, 1.0F,
};
inline constexpr std::array<std::uint16_t, 6> indices{0, 1, 2, 2, 3, 0};

inline constexpr std::array<std::uint8_t, 16> base_color_pixels{
    200, 160, 120, 255, 200, 160, 120, 255, 200, 160, 120, 255, 200, 160, 120, 255,
};
inline constexpr std::array<std::uint8_t, 16> normal_pixels{
    128, 128, 255, 255, 128, 128, 255, 255, 128, 128, 255, 255, 128, 128, 255, 255,
};
inline constexpr std::array<std::uint8_t, 16> metallic_roughness_pixels{
    0, 128, 128, 255, 0, 128, 128, 255, 0, 128, 128, 255, 0, 128, 128, 255,
};

inline std::array<std::byte, dynamic_uniform_stride * 4> make_uniform_data() noexcept {
  std::array<std::byte, dynamic_uniform_stride * 4> data{};
  const auto write_vec4 = [&data](std::size_t offset, const std::array<float, 4>& value) {
    std::memcpy(data.data() + offset, value.data(), sizeof(value));
  };
  write_vec4(0, {-0.5F, 0.0F, 0.0F, 0.0F});
  write_vec4(16, {1.0F, 0.0F, 0.0F, 1.0F});
  write_vec4(dynamic_uniform_stride, {0.5F, 0.0F, 0.0F, 0.0F});
  write_vec4(dynamic_uniform_stride + 16, {0.0F, 1.0F, 0.0F, 1.0F});
  write_vec4(dynamic_uniform_stride * 2, {0.0F, 0.0F, 0.2F, 0.0F});
  write_vec4(dynamic_uniform_stride * 2 + 16, {0.0F, 0.0F, 1.0F, 1.0F});
  write_vec4(dynamic_uniform_stride * 3, {0.0F, 0.0F, 0.8F, 0.0F});
  write_vec4(dynamic_uniform_stride * 3 + 16, {1.0F, 1.0F, 0.0F, 1.0F});
  return data;
}

struct semantic_probe {
  std::uint32_t x{};
  std::uint32_t y{};
  std::array<std::uint8_t, 3> expected{};
  std::uint8_t tolerance{};
};

inline constexpr std::array semantic_probes{
    semantic_probe{0, 0, {0, 0, 0}, 0},
    semantic_probe{width / 2, height / 2, {0, 0, 42}, 2},
    semantic_probe{width / 4, height / 2, {129, 0, 0}, 2},
    semantic_probe{width * 3 / 4, height / 2, {0, 79, 0}, 2},
};

} // namespace granit::test::renderer_fixture

#endif
