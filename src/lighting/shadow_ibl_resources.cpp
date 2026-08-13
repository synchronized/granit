// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "lighting/shadow_ibl_resources.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace granit::lighting {
namespace {

template <typename T> std::span<const std::byte> bytes(const T& value) noexcept {
  return {reinterpret_cast<const std::byte*>(&value), sizeof(value)};
}

template <typename T> std::uint64_t light_buffer_size(std::uint32_t capacity) noexcept {
  return sizeof(T) * static_cast<std::uint64_t>(std::max(capacity, 1U));
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

bool complete(shadow_ibl_texture_views views, lighting_resource_features features) noexcept {
  const auto has_shadow = views.shadow != GRANIT_NULL_HANDLE;
  const auto has_any_ibl = views.ibl.irradiance != GRANIT_NULL_HANDLE ||
                           views.ibl.prefiltered_environment != GRANIT_NULL_HANDLE ||
                           views.ibl.brdf_lut != GRANIT_NULL_HANDLE;
  const auto has_complete_ibl = views.ibl.irradiance != GRANIT_NULL_HANDLE &&
                                views.ibl.prefiltered_environment != GRANIT_NULL_HANDLE &&
                                views.ibl.brdf_lut != GRANIT_NULL_HANDLE;
  return has_shadow == features.shadows && has_any_ibl == features.ibl &&
         (!features.ibl || has_complete_ibl);
}

} // namespace

granit_result shadow_ibl_resources::initialize(granit_renderer renderer,
                                               shadow_ibl_texture_views views,
                                               const shadow_sampling_constants& shadow_values,
                                               const ibl_sampling_constants& ibl_values,
                                               const light_limits& light_capacities,
                                               lighting_resource_features features,
                                               granit_bind_group_layout external_layout) noexcept {
  if (renderer == GRANIT_NULL_HANDLE || initialized() || !complete(views, features) ||
      (features.shadows && !valid_shadow(shadow_values)) ||
      (features.ibl && !valid_ibl(ibl_values)) ||
      (external_layout != GRANIT_NULL_HANDLE && (!features.shadows || !features.ibl)))
    return GRANIT_ERROR_INVALID_ARGUMENT;

  features_ = features;

  const auto make_constants = [&](granit::buffer& buffer, auto& value) {
    return buffer.initialize(
        renderer,
        {.size = sizeof(value),
         .usage = granit::buffer_usage::uniform | granit::buffer_usage::transfer_destination,
         .location = granit::memory_location::automatic},
        bytes(value));
  };
  auto result = granit::result::success;
  if (features.shadows)
    result = make_constants(shadow_constants_, shadow_values);
  if (granit::succeeded(result) && features.ibl)
    result = make_constants(ibl_constants_, ibl_values);
  if (granit::succeeded(result) && features.shadows) {
    result =
        shadow_sampler_.initialize(renderer, {.address_u = granit::address_mode::clamp_to_edge,
                                              .address_v = granit::address_mode::clamp_to_edge,
                                              .address_w = granit::address_mode::clamp_to_edge,
                                              .compare = granit::compare_operation::less_equal});
  }
  if (granit::succeeded(result) && features.ibl) {
    result = ibl_sampler_.initialize(renderer, {.mag_filter = granit::filter::linear,
                                                .min_filter = granit::filter::linear,
                                                .mip_filter = granit::mipmap_filter::linear,
                                                .address_u = granit::address_mode::clamp_to_edge,
                                                .address_v = granit::address_mode::clamp_to_edge,
                                                .address_w = granit::address_mode::clamp_to_edge,
                                                .max_lod = 1000.0F});
  }
  if (granit::succeeded(result))
    result = granit::from_native(lights_.initialize(renderer, light_capacities));
  if (granit::failed(result)) {
    static_cast<void>(reset());
    return static_cast<granit_result>(result);
  }

  constexpr auto fragment = granit::shader_stage_flags::fragment;
  std::vector<granit::bind_group_layout_entry> layout_entries;
  layout_entries.reserve(12);
  if (features.shadows && features.ibl) {
    layout_entries.assign(standard_lighting_layout_entries.begin(),
                          standard_lighting_layout_entries.end());
  } else {
  if (features.shadows) {
    layout_entries.push_back({shadow_binding_constants, granit::binding_type::uniform_buffer, 1,
                              granit::shader_stage_flags::vertex | fragment});
    layout_entries.push_back(
        {shadow_binding_texture, granit::binding_type::sampled_texture, 1, fragment});
    layout_entries.push_back({shadow_binding_sampler, granit::binding_type::sampler, 1, fragment});
  }
  if (features.ibl) {
    layout_entries.push_back(
        {ibl_binding_constants, granit::binding_type::uniform_buffer, 1, fragment});
    layout_entries.push_back(
        {ibl_binding_irradiance, granit::binding_type::sampled_texture, 1, fragment});
    layout_entries.push_back({ibl_binding_prefiltered_environment,
                              granit::binding_type::sampled_texture, 1, fragment});
    layout_entries.push_back(
        {ibl_binding_brdf_lut, granit::binding_type::sampled_texture, 1, fragment});
    layout_entries.push_back({ibl_binding_sampler, granit::binding_type::sampler, 1, fragment});
  }
  layout_entries.push_back(
      {light_binding_counts, granit::binding_type::uniform_buffer, 1, fragment});
  layout_entries.push_back(
      {light_binding_directional, granit::binding_type::storage_buffer, 1, fragment});
  layout_entries.push_back(
      {light_binding_point, granit::binding_type::storage_buffer, 1, fragment});
  layout_entries.push_back(
      {light_binding_spot, granit::binding_type::storage_buffer, 1, fragment});
  }
  if (external_layout == GRANIT_NULL_HANDLE) {
    result = layout_.initialize(renderer, layout_entries);
    if (granit::failed(result)) {
      static_cast<void>(reset());
      return static_cast<granit_result>(result);
    }
    layout_handle_ = layout_.native_handle();
  } else {
    layout_handle_ = external_layout;
  }

  std::vector<granit::bind_group_entry> group_entries;
  if (features.shadows) {
    group_entries.insert(
        group_entries.end(),
        {{.binding = shadow_binding_constants,
          .resource = shadow_constants_.native_handle(),
          .offset = 0,
          .size = sizeof(shadow_values)},
         {.binding = shadow_binding_texture, .resource = views.shadow},
         {.binding = shadow_binding_sampler, .resource = shadow_sampler_.native_handle()}});
  }
  if (features.ibl) {
    group_entries.insert(
        group_entries.end(),
        {{.binding = ibl_binding_constants,
          .resource = ibl_constants_.native_handle(),
          .offset = 0,
          .size = sizeof(ibl_values)},
         {.binding = ibl_binding_irradiance, .resource = views.ibl.irradiance},
         {.binding = ibl_binding_prefiltered_environment,
          .resource = views.ibl.prefiltered_environment},
         {.binding = ibl_binding_brdf_lut, .resource = views.ibl.brdf_lut},
         {.binding = ibl_binding_sampler, .resource = ibl_sampler_.native_handle()}});
  }
  group_entries.insert(
      group_entries.end(),
      {
          granit::bind_group_entry{.binding = light_binding_counts,
                                   .resource = lights_.counts(),
                                   .offset = 0,
                                   .size = sizeof(gpu_light_counts)},
          granit::bind_group_entry{
              .binding = light_binding_directional,
              .resource = lights_.directional(),
              .offset = 0,
              .size = light_buffer_size<gpu_directional_light>(light_capacities.directional)},
          granit::bind_group_entry{.binding = light_binding_point,
                                   .resource = lights_.point(),
                                   .offset = 0,
                                   .size =
                                       light_buffer_size<gpu_point_light>(light_capacities.point)},
          granit::bind_group_entry{.binding = light_binding_spot,
                                   .resource = lights_.spot(),
                                   .offset = 0,
                                   .size =
                                       light_buffer_size<gpu_spot_light>(light_capacities.spot)},
      });
  result = group_.initialize(renderer, layout_handle_, group_entries);
  if (granit::failed(result)) {
    static_cast<void>(reset());
    return static_cast<granit_result>(result);
  }
  return GRANIT_SUCCESS;
}

granit_result
shadow_ibl_resources::update_shadow(const shadow_sampling_constants& values) noexcept {
  if (!initialized() || !features_.shadows || !valid_shadow(values))
    return GRANIT_ERROR_INVALID_ARGUMENT;
  return static_cast<granit_result>(shadow_constants_.write(0, bytes(values)));
}

granit_result shadow_ibl_resources::update_ibl(const ibl_sampling_constants& values) noexcept {
  if (!initialized() || !features_.ibl || !valid_ibl(values))
    return GRANIT_ERROR_INVALID_ARGUMENT;
  return static_cast<granit_result>(ibl_constants_.write(0, bytes(values)));
}

granit_result shadow_ibl_resources::update_lights(const packed_view_lights& lights) noexcept {
  if (!initialized())
    return GRANIT_ERROR_INVALID_ARGUMENT;
  return lights_.update(lights);
}

granit_result shadow_ibl_resources::reset() noexcept {
  granit_result first = GRANIT_SUCCESS;
  const auto capture = [&](granit::result value) {
    if (first == GRANIT_SUCCESS && granit::failed(value))
      first = static_cast<granit_result>(value);
  };
  capture(group_.reset());
  capture(layout_.reset());
  layout_handle_ = GRANIT_NULL_HANDLE;
  capture(granit::from_native(lights_.reset()));
  capture(ibl_sampler_.reset());
  capture(shadow_sampler_.reset());
  capture(ibl_constants_.reset());
  capture(shadow_constants_.reset());
  features_ = {};
  return first;
}

} // namespace granit::lighting
