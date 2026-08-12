// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "lighting/shadow_ibl_resources.h"

#include <array>
#include <cmath>

namespace granit::lighting {
namespace {

template <typename T> std::span<const std::byte> bytes(const T& value) noexcept {
  return {reinterpret_cast<const std::byte*>(&value), sizeof(value)};
}

bool valid_shadow(const shadow_sampling_constants& value) noexcept {
  return math::is_finite(value.light_view_projection) && std::isfinite(value.depth_bias) &&
         std::isfinite(value.normal_bias) && std::isfinite(value.texel_size[0]) &&
         std::isfinite(value.texel_size[1]) && value.depth_bias >= 0.0F &&
         value.normal_bias >= 0.0F && value.texel_size[0] > 0.0F && value.texel_size[1] > 0.0F;
}

bool valid_ibl(const ibl_sampling_constants& value) noexcept {
  return std::isfinite(value.rotation_cos) && std::isfinite(value.rotation_sin) &&
         std::isfinite(value.intensity) && std::isfinite(value.prefiltered_max_mip) &&
         value.intensity >= 0.0F && value.prefiltered_max_mip >= 0.0F;
}

bool complete(shadow_ibl_texture_views views) noexcept {
  return views.shadow != GRANIT_NULL_HANDLE && views.ibl.irradiance != GRANIT_NULL_HANDLE &&
         views.ibl.prefiltered_environment != GRANIT_NULL_HANDLE &&
         views.ibl.brdf_lut != GRANIT_NULL_HANDLE;
}

} // namespace

granit_result shadow_ibl_resources::initialize(
    granit_renderer renderer, shadow_ibl_texture_views views,
    const shadow_sampling_constants& shadow_values,
    const ibl_sampling_constants& ibl_values) noexcept {
  if (renderer == GRANIT_NULL_HANDLE || initialized() || !complete(views) ||
      !valid_shadow(shadow_values) || !valid_ibl(ibl_values))
    return GRANIT_ERROR_INVALID_ARGUMENT;

  const auto make_constants = [&](granit::buffer& buffer, auto& value) {
    return buffer.initialize(renderer,
                             {.size = sizeof(value),
                              .usage = granit::buffer_usage::uniform |
                                       granit::buffer_usage::transfer_destination,
                              .location = granit::memory_location::automatic},
                             bytes(value));
  };
  auto result = make_constants(shadow_constants_, shadow_values);
  if (granit::succeeded(result))
    result = make_constants(ibl_constants_, ibl_values);
  if (granit::succeeded(result)) {
    result = shadow_sampler_.initialize(
        renderer, {.address_u = granit::address_mode::clamp_to_edge,
                   .address_v = granit::address_mode::clamp_to_edge,
                   .address_w = granit::address_mode::clamp_to_edge,
                   .compare = granit::compare_operation::less_equal});
  }
  if (granit::succeeded(result)) {
    result = ibl_sampler_.initialize(renderer, {.mag_filter = granit::filter::linear,
                                                .min_filter = granit::filter::linear,
                                                .mip_filter = granit::mipmap_filter::linear,
                                                .address_u = granit::address_mode::clamp_to_edge,
                                                .address_v = granit::address_mode::clamp_to_edge,
                                                .address_w = granit::address_mode::clamp_to_edge,
                                                .max_lod = 1000.0F});
  }
  if (granit::failed(result)) {
    static_cast<void>(reset());
    return static_cast<granit_result>(result);
  }

  constexpr auto fragment = granit::shader_stage_flags::fragment;
  const std::array layout_entries{
      granit::bind_group_layout_entry{shadow_binding_constants, granit::binding_type::uniform_buffer,
                                      1, granit::shader_stage_flags::vertex | fragment},
      granit::bind_group_layout_entry{shadow_binding_texture,
                                      granit::binding_type::sampled_texture, 1, fragment},
      granit::bind_group_layout_entry{shadow_binding_sampler, granit::binding_type::sampler, 1,
                                      fragment},
      granit::bind_group_layout_entry{ibl_binding_constants, granit::binding_type::uniform_buffer, 1,
                                      fragment},
      granit::bind_group_layout_entry{ibl_binding_irradiance,
                                      granit::binding_type::sampled_texture, 1, fragment},
      granit::bind_group_layout_entry{ibl_binding_prefiltered_environment,
                                      granit::binding_type::sampled_texture, 1, fragment},
      granit::bind_group_layout_entry{ibl_binding_brdf_lut,
                                      granit::binding_type::sampled_texture, 1, fragment},
      granit::bind_group_layout_entry{ibl_binding_sampler, granit::binding_type::sampler, 1,
                                      fragment}};
  result = layout_.initialize(renderer, layout_entries);
  if (granit::failed(result)) {
    static_cast<void>(reset());
    return static_cast<granit_result>(result);
  }

  const std::array group_entries{
      granit::bind_group_entry{.binding = shadow_binding_constants,
                               .resource = shadow_constants_.native_handle(),
                               .offset = 0,
                               .size = sizeof(shadow_values)},
      granit::bind_group_entry{.binding = shadow_binding_texture, .resource = views.shadow},
      granit::bind_group_entry{.binding = shadow_binding_sampler,
                               .resource = shadow_sampler_.native_handle()},
      granit::bind_group_entry{.binding = ibl_binding_constants,
                               .resource = ibl_constants_.native_handle(),
                               .offset = 0,
                               .size = sizeof(ibl_values)},
      granit::bind_group_entry{.binding = ibl_binding_irradiance,
                               .resource = views.ibl.irradiance},
      granit::bind_group_entry{.binding = ibl_binding_prefiltered_environment,
                               .resource = views.ibl.prefiltered_environment},
      granit::bind_group_entry{.binding = ibl_binding_brdf_lut,
                               .resource = views.ibl.brdf_lut},
      granit::bind_group_entry{.binding = ibl_binding_sampler,
                               .resource = ibl_sampler_.native_handle()}};
  result = group_.initialize(renderer, layout_.native_handle(), group_entries);
  if (granit::failed(result)) {
    static_cast<void>(reset());
    return static_cast<granit_result>(result);
  }
  return GRANIT_SUCCESS;
}

granit_result shadow_ibl_resources::update_shadow(
    const shadow_sampling_constants& values) noexcept {
  if (!initialized() || !valid_shadow(values))
    return GRANIT_ERROR_INVALID_ARGUMENT;
  return static_cast<granit_result>(shadow_constants_.write(0, bytes(values)));
}

granit_result shadow_ibl_resources::update_ibl(const ibl_sampling_constants& values) noexcept {
  if (!initialized() || !valid_ibl(values))
    return GRANIT_ERROR_INVALID_ARGUMENT;
  return static_cast<granit_result>(ibl_constants_.write(0, bytes(values)));
}

granit_result shadow_ibl_resources::reset() noexcept {
  granit_result first = GRANIT_SUCCESS;
  const auto capture = [&](granit::result value) {
    if (first == GRANIT_SUCCESS && granit::failed(value))
      first = static_cast<granit_result>(value);
  };
  capture(group_.reset());
  capture(layout_.reset());
  capture(ibl_sampler_.reset());
  capture(shadow_sampler_.reset());
  capture(ibl_constants_.reset());
  capture(shadow_constants_.reset());
  return first;
}

} // namespace granit::lighting
