// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_PIPELINE_DEFAULT_IBL_RESOURCES_H_
#define GRANIT_PIPELINE_DEFAULT_IBL_RESOURCES_H_

#include "lighting/ibl_resources.h"

#include <granit/renderer/texture.hpp>

namespace granit::pipeline::detail {

/** 管线实例拥有的中性黑环境；用于保证未提供环境资产时 IBL 绑定仍完整。 */
class default_ibl_resources {
public:
  [[nodiscard]] granit_result initialize(granit_renderer renderer) noexcept;
  [[nodiscard]] granit_result reset() noexcept;
  [[nodiscard]] bool initialized() const noexcept { return resources_.initialized(); }
  [[nodiscard]] granit_texture_view irradiance() const noexcept {
    return irradiance_view_.native_handle();
  }
  [[nodiscard]] granit_texture_view prefiltered_environment() const noexcept {
    return prefiltered_view_.native_handle();
  }
  [[nodiscard]] granit_texture_view brdf_lut() const noexcept {
    return brdf_lut_view_.native_handle();
  }
  [[nodiscard]] granit_bind_group_layout layout() const noexcept { return resources_.layout(); }
  [[nodiscard]] granit_bind_group group() const noexcept { return resources_.group(); }

private:
  granit::texture irradiance_texture_;
  granit::texture prefiltered_texture_;
  granit::texture brdf_lut_texture_;
  granit::texture_view irradiance_view_;
  granit::texture_view prefiltered_view_;
  granit::texture_view brdf_lut_view_;
  granit::lighting::ibl_resources resources_;
};

} // namespace granit::pipeline::detail

#endif
