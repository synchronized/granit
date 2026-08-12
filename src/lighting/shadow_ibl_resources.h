// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_LIGHTING_SHADOW_IBL_RESOURCES_H
#define GRANIT_LIGHTING_SHADOW_IBL_RESOURCES_H

#include "lighting/ibl_resources.h"
#include "lighting/shadow_resources.h"

namespace granit::lighting {

struct shadow_ibl_texture_views {
  granit_texture_view shadow = GRANIT_NULL_HANDLE;
  ibl_texture_views ibl{};
};

/** 拥有阴影与 IBL 共用的完整 Group 3；只借用调用方持有的四个 Texture View。 */
class shadow_ibl_resources {
public:
  [[nodiscard]] granit_result initialize(granit_renderer renderer, shadow_ibl_texture_views views,
                                         const shadow_sampling_constants& shadow_constants,
                                         const ibl_sampling_constants& ibl_constants) noexcept;
  [[nodiscard]] granit_result update_shadow(
      const shadow_sampling_constants& constants) noexcept;
  [[nodiscard]] granit_result update_ibl(const ibl_sampling_constants& constants) noexcept;
  [[nodiscard]] granit_result reset() noexcept;
  [[nodiscard]] bool initialized() const noexcept { return group_.valid(); }
  [[nodiscard]] granit_bind_group_layout layout() const noexcept { return layout_.native_handle(); }
  [[nodiscard]] granit_bind_group group() const noexcept { return group_.native_handle(); }

private:
  granit::buffer shadow_constants_;
  granit::buffer ibl_constants_;
  granit::sampler shadow_sampler_;
  granit::sampler ibl_sampler_;
  granit::bind_group_layout layout_;
  granit::bind_group group_;
};

} // namespace granit::lighting

#endif
