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

  const auto renderer_view = granit::renderer_ref::from_native(renderer);
  auto result = constants_.initialize(
      renderer_view,
      {.size = sizeof(values),
       .usage = granit::buffer_usage::uniform | granit::buffer_usage::transfer_destination,
       .location = granit::memory_location::automatic},
      bytes(values));
  if (result.failed())
    return static_cast<granit_result>(result);

  result =
      sampler_.initialize(renderer_view, {.mag_filter = granit::filter::linear,
                                          .min_filter = granit::filter::linear,
                                          .mip_filter = granit::mipmap_filter::linear,
                                          .address_u = granit::address_mode::clamp_to_edge,
                                          .address_v = granit::address_mode::clamp_to_edge,
                                          .address_w = granit::address_mode::clamp_to_edge,
                                          // Shader 通过动态常量选择 mip，Sampler 不额外收紧该范围。
                                          .max_lod = 1000.0F});
  if (result.failed()) {
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
                                      .type = granit::binding_type::sampled_texture_cube,
                                      .array_count = 1,
                                      .visibility = fragment},
      granit::bind_group_layout_entry{.binding = ibl_binding_prefiltered_environment,
                                      .type = granit::binding_type::sampled_texture_cube,
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
  result = layout_.initialize(renderer_view, layout_entries);
  if (result.failed()) {
    static_cast<void>(reset());
    return static_cast<granit_result>(result);
  }

  const std::array group_entries{
      granit::bind_group_entry{.binding = ibl_binding_constants,
                               .resource = constants_.ref(),
                               .offset = 0,
                               .size = sizeof(values)},
      granit::bind_group_entry{.binding = ibl_binding_irradiance,
                               .resource =
                                   granit::binding_resource_ref::from_native(views.irradiance)},
      granit::bind_group_entry{
          .binding = ibl_binding_prefiltered_environment,
          .resource = granit::binding_resource_ref::from_native(views.prefiltered_environment)},
      granit::bind_group_entry{.binding = ibl_binding_brdf_lut,
                               .resource =
                                   granit::binding_resource_ref::from_native(views.brdf_lut)},
      granit::bind_group_entry{.binding = ibl_binding_sampler, .resource = sampler_.ref()}};
  result = group_.initialize(renderer_view, layout_.native_handle(), group_entries);
  if (result.failed()) {
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
    if (first == GRANIT_SUCCESS && value.failed())
      first = static_cast<granit_result>(value);
  };
  capture(group_.reset());
  capture(layout_.reset());
  capture(sampler_.reset());
  capture(constants_.reset());
  return first;
}

} // namespace granit::lighting
