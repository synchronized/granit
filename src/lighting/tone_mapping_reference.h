// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_LIGHTING_TONE_MAPPING_REFERENCE_H
#define GRANIT_LIGHTING_TONE_MAPPING_REFERENCE_H

#include "math/math.h"

#include <granit/renderer/resource_types.hpp>

#include <cstdint>

namespace granit::lighting {

enum class tone_mapping_output_transfer : std::uint8_t {
  /** Shader 输出线性显示颜色，由 sRGB Attachment 编码。 */
  attachment_srgb,
  /** Shader 显式编码为 sRGB，写入 UNORM Attachment。 */
  shader_srgb,
};

struct tone_mapping_desc {
  float exposure_ev = 0.0F;
  tone_mapping_output_transfer output_transfer = tone_mapping_output_transfer::attachment_srgb;
  bool enable_fxaa = true;
};

enum class tone_mapping_error : std::uint8_t {
  none,
  invalid_color,
  invalid_exposure,
  incompatible_output,
};

inline constexpr float tone_mapping_min_exposure_ev = -24.0F;
inline constexpr float tone_mapping_max_exposure_ev = 24.0F;

/** 检查输出格式与 Shader/Attachment sRGB 传递方式是否唯一匹配。 */
[[nodiscard]] tone_mapping_error
validate_tone_mapping_output(granit::texture_format format,
                             tone_mapping_output_transfer transfer) noexcept;

/** 对非负线性 HDR 单通道应用 ACES fitted 近似并限制到 [0, 1]。 */
[[nodiscard]] float aces_fitted(float value) noexcept;

/** 将 [0, 1] 线性显示值转换为精确分段 sRGB 值。 */
[[nodiscard]] float linear_to_srgb(float value) noexcept;

/** 应用曝光和 Tone Mapping；失败时不修改 output。 */
[[nodiscard]] tone_mapping_error evaluate_tone_mapping(math::float3 hdr_color,
                                                       const tone_mapping_desc& desc,
                                                       math::float3& output) noexcept;

} // namespace granit::lighting

#endif
