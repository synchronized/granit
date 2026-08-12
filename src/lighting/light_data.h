// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_LIGHTING_LIGHT_DATA_H
#define GRANIT_LIGHTING_LIGHT_DATA_H

#include "scene/multi_view_submission.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace granit::lighting {

inline constexpr std::uint32_t maximum_directional_lights = 16;
inline constexpr std::uint32_t maximum_point_lights = 256;
inline constexpr std::uint32_t maximum_spot_lights = 256;

struct light_limits {
  std::uint32_t directional = 4;
  std::uint32_t point = 64;
  std::uint32_t spot = 64;
};

struct light_requirements {
  std::uint32_t directional = 0;
  std::uint32_t point = 0;
  std::uint32_t spot = 0;
};

/** 与 Shader StructuredBuffer 元素保持一致的方向光布局。 */
struct alignas(16) gpu_directional_light {
  float direction_to_light[3]{};
  float padding0 = 0.0F;
  float radiance[3]{};
  float padding1 = 0.0F;
};

/** 与 Shader StructuredBuffer 元素保持一致的点光布局。 */
struct alignas(16) gpu_point_light {
  float position[3]{};
  float radius = 0.0F;
  float intensity[3]{};
  float padding = 0.0F;
};

/** 与 Shader StructuredBuffer 元素保持一致的聚光布局，锥角在 CPU 侧转换为余弦。 */
struct alignas(16) gpu_spot_light {
  float position[3]{};
  float radius = 0.0F;
  float direction[3]{};
  float outer_angle_cosine = 0.0F;
  float intensity[3]{};
  float inner_angle_cosine = 0.0F;
};

static_assert(sizeof(gpu_directional_light) == 32);
static_assert(sizeof(gpu_point_light) == 32);
static_assert(sizeof(gpu_spot_light) == 48);

struct packed_view_lights {
  std::vector<gpu_directional_light> directional;
  std::vector<gpu_point_light> point;
  std::vector<gpu_spot_light> spot;
};

enum class light_pack_error : std::uint8_t {
  none,
  view_out_of_range,
  invalid_limits,
  capacity_exceeded,
  out_of_memory,
};

/**
 * 将一个 View 的可见光源打包为 GPU 布局。
 *
 * requirements 始终返回该 View 的真实可见数量；失败时不修改 output。
 */
[[nodiscard]] light_pack_error pack_view_lights(const scene::multi_view_snapshot& snapshot,
                                                std::size_t view_index, const light_limits& limits,
                                                packed_view_lights& output,
                                                light_requirements& requirements) noexcept;

} // namespace granit::lighting

#endif
