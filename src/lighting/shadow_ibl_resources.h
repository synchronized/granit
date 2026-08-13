// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_LIGHTING_SHADOW_IBL_RESOURCES_H
#define GRANIT_LIGHTING_SHADOW_IBL_RESOURCES_H

#include "lighting/ibl_resources.h"
#include "lighting/light_buffers.h"
#include "lighting/shadow_resources.h"

#include <array>

namespace granit::lighting {

/** Forward PBR Group 3 的完整绑定契约；Shader 变体可只使用其中一部分。 */
inline constexpr std::array standard_lighting_layout_entries{
    granit::bind_group_layout_entry{shadow_binding_constants,
                                    granit::binding_type::uniform_buffer, 1,
                                    granit::shader_stage_flags::vertex |
                                        granit::shader_stage_flags::fragment},
    granit::bind_group_layout_entry{shadow_binding_texture,
                                    granit::binding_type::sampled_texture, 1,
                                    granit::shader_stage_flags::fragment},
    granit::bind_group_layout_entry{shadow_binding_sampler, granit::binding_type::sampler, 1,
                                    granit::shader_stage_flags::fragment},
    granit::bind_group_layout_entry{ibl_binding_constants,
                                    granit::binding_type::uniform_buffer, 1,
                                    granit::shader_stage_flags::fragment},
    granit::bind_group_layout_entry{ibl_binding_irradiance,
                                    granit::binding_type::sampled_texture, 1,
                                    granit::shader_stage_flags::fragment},
    granit::bind_group_layout_entry{ibl_binding_prefiltered_environment,
                                    granit::binding_type::sampled_texture, 1,
                                    granit::shader_stage_flags::fragment},
    granit::bind_group_layout_entry{ibl_binding_brdf_lut,
                                    granit::binding_type::sampled_texture, 1,
                                    granit::shader_stage_flags::fragment},
    granit::bind_group_layout_entry{ibl_binding_sampler, granit::binding_type::sampler, 1,
                                    granit::shader_stage_flags::fragment},
    granit::bind_group_layout_entry{light_binding_counts,
                                    granit::binding_type::uniform_buffer, 1,
                                    granit::shader_stage_flags::fragment},
    granit::bind_group_layout_entry{light_binding_directional,
                                    granit::binding_type::storage_buffer, 1,
                                    granit::shader_stage_flags::fragment},
    granit::bind_group_layout_entry{light_binding_point,
                                    granit::binding_type::storage_buffer, 1,
                                    granit::shader_stage_flags::fragment},
    granit::bind_group_layout_entry{light_binding_spot,
                                    granit::binding_type::storage_buffer, 1,
                                    granit::shader_stage_flags::fragment}};

struct shadow_ibl_texture_views {
  granit_texture_view shadow = GRANIT_NULL_HANDLE;
  ibl_texture_views ibl{};
};

struct lighting_resource_features {
  bool shadows = true;
  bool ibl = true;
};

/** 拥有光照 Group 3；只借用调用方持有且由 features 启用的 Texture View。 */
class shadow_ibl_resources {
public:
  [[nodiscard]] granit_result initialize(granit_renderer renderer, shadow_ibl_texture_views views,
                                         const shadow_sampling_constants& shadow_constants,
                                         const ibl_sampling_constants& ibl_constants,
                                         const light_limits& light_capacities = {},
                                         lighting_resource_features features = {}) noexcept;
  [[nodiscard]] granit_result update_shadow(const shadow_sampling_constants& constants) noexcept;
  [[nodiscard]] granit_result update_ibl(const ibl_sampling_constants& constants) noexcept;
  [[nodiscard]] granit_result update_lights(const packed_view_lights& lights) noexcept;
  [[nodiscard]] granit_result reset() noexcept;
  [[nodiscard]] bool initialized() const noexcept { return group_.valid(); }
  [[nodiscard]] granit_bind_group_layout layout() const noexcept { return layout_.native_handle(); }
  [[nodiscard]] granit_bind_group group() const noexcept { return group_.native_handle(); }

private:
  granit::buffer shadow_constants_;
  granit::buffer ibl_constants_;
  granit::sampler shadow_sampler_;
  granit::sampler ibl_sampler_;
  light_buffers lights_;
  granit::bind_group_layout layout_;
  granit::bind_group group_;
  lighting_resource_features features_{};
};

} // namespace granit::lighting

#endif
