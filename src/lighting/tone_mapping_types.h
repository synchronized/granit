// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_LIGHTING_TONE_MAPPING_TYPES_H
#define GRANIT_LIGHTING_TONE_MAPPING_TYPES_H

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

} // namespace granit::lighting

#endif
