// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_EXAMPLES_COMMON_MODEL_VIEWER_MATERIAL_EDIT_H_
#define GRANIT_EXAMPLES_COMMON_MODEL_VIEWER_MATERIAL_EDIT_H_

#include <granit/math/types.hpp>

namespace granit::example::model_viewer {

struct material_factor_edit {
  math::float4 base_color{1.0F, 1.0F, 1.0F, 1.0F};
  float metallic{1.0F};
  float roughness{1.0F};
  float normal_scale{1.0F};
  float occlusion_strength{1.0F};
  math::float3 emissive{};
};

} // namespace granit::example::model_viewer

#endif
