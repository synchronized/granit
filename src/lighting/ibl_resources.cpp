// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "lighting/ibl_resources.h"

#include <array>
#include <cmath>

namespace granit::lighting {
namespace {

bool valid(const ibl_sampling_constants& value) noexcept {
  return std::isfinite(value.rotation_cos) && std::isfinite(value.rotation_sin) &&
         std::isfinite(value.intensity) && std::isfinite(value.prefiltered_max_mip) &&
         value.intensity >= 0.0F && value.prefiltered_max_mip >= 0.0F;
}

bool complete(ibl_texture_views views) noexcept {
  return views.irradiance != GRANIT_NULL_HANDLE &&
         views.prefiltered_environment != GRANIT_NULL_HANDLE &&
         views.brdf_lut != GRANIT_NULL_HANDLE;
}

std::span<const std::byte> bytes(const ibl_sampling_constants& value) noexcept {
  return {reinterpret_cast<const std::byte*>(&value), sizeof(value)};
}

} // namespace

granit_result ibl_resources::initialize(granit_renderer renderer, ibl_texture_views views,
                                        const ibl_sampling_constants& values) noexcept {
  if (renderer == GRANIT_NULL_HANDLE || initialized() || !complete(views) || !valid(values))
    return GRANIT_ERROR_INVALID_ARGUMENT;

  auto result = constants_.initialize(
      renderer,
      {.size = sizeof(values),
       .usage = granit::buffer_usage::uniform | granit::buffer_usage::transfer_destination,
       .location = granit::memory_location::automatic},
      bytes(values));
  if (granit::failed(result))
    return static_cast<granit_result>(result);

  result = sampler_.initialize(renderer, {.mag_filter = granit::filter::linear,
                                          .min_filter = granit::filter::linear,
                                          .mip_filter = granit::mipmap_filter::linear,
                                          .address_u = granit::address_mode::clamp_to_edge,
                                          .address_v = granit::address_mode::clamp_to_edge,
                                          .address_w = granit::address_mode::clamp_to_edge,
                                          // Shader 通过动态常量选择 mip，Sampler 不额外收紧该范围。
                                          .max_lod = 1000.0F});
  if (granit::failed(result)) {
    static_cast<void>(reset());
    return static_cast<granit_result>(result);
  }

  constexpr auto fragment = granit::shader_stage_flags::fragment;
  const std::array layout_entries{
      granit::bind_group_layout_entry{.binding = ibl_binding_constants,
                                      .type = granit::binding_type::uniform_buffer,
                                      .array_count = 1,
                                      .visibility = fragment},
      granit::bind_group_layout_entry{.binding = ibl_binding_irradiance,
                                      .type = granit::binding_type::sampled_texture,
                                      .array_count = 1,
                                      .visibility = fragment},
      granit::bind_group_layout_entry{.binding = ibl_binding_prefiltered_environment,
                                      .type = granit::binding_type::sampled_texture,
                                      .array_count = 1,
                                      .visibility = fragment},
      granit::bind_group_layout_entry{.binding = ibl_binding_brdf_lut,
                                      .type = granit::binding_type::sampled_texture,
                                      .array_count = 1,
                                      .visibility = fragment},
      granit::bind_group_layout_entry{.binding = ibl_binding_sampler,
                                      .type = granit::binding_type::sampler,
                                      .array_count = 1,
                                      .visibility = fragment}};
  result = layout_.initialize(renderer, layout_entries);
  if (granit::failed(result)) {
    static_cast<void>(reset());
    return static_cast<granit_result>(result);
  }

  const std::array group_entries{
      granit::bind_group_entry{.binding = ibl_binding_constants,
                               .resource = constants_.native_handle(),
                               .offset = 0,
                               .size = sizeof(values)},
      granit::bind_group_entry{.binding = ibl_binding_irradiance,
                               .resource = views.irradiance},
      granit::bind_group_entry{.binding = ibl_binding_prefiltered_environment,
                               .resource = views.prefiltered_environment},
      granit::bind_group_entry{.binding = ibl_binding_brdf_lut, .resource = views.brdf_lut},
      granit::bind_group_entry{.binding = ibl_binding_sampler,
                               .resource = sampler_.native_handle()}};
  result = group_.initialize(renderer, layout_.native_handle(), group_entries);
  if (granit::failed(result)) {
    static_cast<void>(reset());
    return static_cast<granit_result>(result);
  }
  return GRANIT_SUCCESS;
}

granit_result ibl_resources::update(const ibl_sampling_constants& values) noexcept {
  if (!initialized() || !valid(values))
    return GRANIT_ERROR_INVALID_ARGUMENT;
  return static_cast<granit_result>(constants_.write(0, bytes(values)));
}

granit_result ibl_resources::reset() noexcept {
  granit_result first = GRANIT_SUCCESS;
  const auto capture = [&](granit::result value) {
    if (first == GRANIT_SUCCESS && granit::failed(value))
      first = static_cast<granit_result>(value);
  };
  capture(group_.reset());
  capture(layout_.reset());
  capture(sampler_.reset());
  capture(constants_.reset());
  return first;
}

} // namespace granit::lighting
