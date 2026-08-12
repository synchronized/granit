// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_LIGHTING_IBL_REFERENCE_H
#define GRANIT_LIGHTING_IBL_REFERENCE_H

#include "material/pbr_reference.h"

namespace granit::lighting {

struct ibl_reference_input {
  material::pbr_float3 normal{0.0F, 0.0F, 1.0F};
  material::pbr_float3 view_direction{0.0F, 0.0F, 1.0F};
  /** 漫反射辐照度立方体贴图的线性 RGB 采样结果。 */
  material::pbr_float3 irradiance{};
  /** 按粗糙度选择 mip 后的预过滤环境线性 RGB 采样结果。 */
  material::pbr_float3 prefiltered_radiance{};
  /** BRDF LUT 采样结果，x 为反射率缩放，y 为偏移。 */
  math::float2 brdf_lut{};
  float environment_intensity = 1.0F;
  float ambient_occlusion = 1.0F;
};

/** 将感知粗糙度映射到预过滤环境贴图 mip；max_mip_level 是最大 mip 索引。 */
[[nodiscard]] float ibl_prefilter_mip(float perceptual_roughness,
                                      float max_mip_level) noexcept;

/** 绕世界 Y 轴旋转环境查询方向；非法或零方向返回零向量。 */
[[nodiscard]] material::pbr_float3 rotate_environment_direction(
    material::pbr_float3 direction, float rotation_radians) noexcept;

/** 计算 split-sum IBL 的线性 RGB 间接光；不包含直接光和自发光。 */
[[nodiscard]] material::pbr_float3 evaluate_pbr_ibl(
    const material::pbr_material_parameters& material_parameters,
    const ibl_reference_input& input) noexcept;

} // namespace granit::lighting

#endif
