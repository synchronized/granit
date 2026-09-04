// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_LIGHTING_TONE_MAPPING_REFERENCE_H
#define GRANIT_LIGHTING_TONE_MAPPING_REFERENCE_H

#include "lighting/tone_mapping_types.h"
#include "math/math.h"

namespace granit::lighting {

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
