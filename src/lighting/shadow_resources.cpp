// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "lighting/shadow_resources.h"

#include <array>
#include <cmath>

namespace granit::lighting {
namespace {

bool valid(const shadow_sampling_constants& value) noexcept {
  return math::is_finite(value.light_view_projection) && std::isfinite(value.depth_bias) &&
         std::isfinite(value.normal_bias) && std::isfinite(value.texel_size[0]) &&
         std::isfinite(value.texel_size[1]) && value.depth_bias >= 0.0F &&
         value.normal_bias >= 0.0F && value.texel_size[0] > 0.0F && value.texel_size[1] > 0.0F;
}

std::span<const std::byte> bytes(const shadow_sampling_constants& value) noexcept {
  return {reinterpret_cast<const std::byte*>(&value), sizeof(value)};
}

} // namespace

granit_result shadow_resources::initialize(granit_renderer renderer,
                                           granit_texture_view shadow_view,
                                           const shadow_sampling_constants& values) noexcept {
  if (renderer == GRANIT_NULL_HANDLE || shadow_view == GRANIT_NULL_HANDLE || initialized() ||
      !valid(values))
    return GRANIT_ERROR_INVALID_ARGUMENT;

  auto result = constants_.initialize(
      renderer,
      {.size = sizeof(values),
       .usage = granit::buffer_usage::uniform | granit::buffer_usage::transfer_destination,
       .location = granit::memory_location::automatic},
      bytes(values));
  if (granit::failed(result))
    return static_cast<granit_result>(result);

  result = sampler_.initialize(renderer, {.address_u = granit::address_mode::clamp_to_edge,
                                          .address_v = granit::address_mode::clamp_to_edge,
                                          .address_w = granit::address_mode::clamp_to_edge,
                                          .compare = granit::compare_operation::less_equal});
  if (granit::failed(result)) {
    static_cast<void>(reset());
    return static_cast<granit_result>(result);
  }

  const std::array layout_entries{
      granit::bind_group_layout_entry{.binding = shadow_binding_constants,
                                      .type = granit::binding_type::uniform_buffer,
                                      .array_count = 1,
                                      .visibility = granit::shader_stage_flags::vertex |
                                                    granit::shader_stage_flags::fragment},
      granit::bind_group_layout_entry{.binding = shadow_binding_texture,
                                      .type = granit::binding_type::sampled_depth_texture,
                                      .array_count = 1,
                                      .visibility = granit::shader_stage_flags::fragment},
      granit::bind_group_layout_entry{.binding = shadow_binding_sampler,
                                      .type = granit::binding_type::comparison_sampler,
                                      .array_count = 1,
                                      .visibility = granit::shader_stage_flags::fragment}};
  result = layout_.initialize(renderer, layout_entries);
  if (granit::failed(result)) {
    static_cast<void>(reset());
    return static_cast<granit_result>(result);
  }

  const std::array group_entries{
      granit::bind_group_entry{.binding = shadow_binding_constants,
                               .resource = constants_.native_handle(),
                               .offset = 0,
                               .size = sizeof(values)},
      granit::bind_group_entry{.binding = shadow_binding_texture, .resource = shadow_view},
      granit::bind_group_entry{.binding = shadow_binding_sampler,
                               .resource = sampler_.native_handle()}};
  result = group_.initialize(renderer, layout_.native_handle(), group_entries);
  if (granit::failed(result)) {
    static_cast<void>(reset());
    return static_cast<granit_result>(result);
  }
  return GRANIT_SUCCESS;
}

granit_result shadow_resources::update(const shadow_sampling_constants& values) noexcept {
  if (!initialized() || !valid(values))
    return GRANIT_ERROR_INVALID_ARGUMENT;
  return static_cast<granit_result>(constants_.write(0, bytes(values)));
}

granit_result shadow_resources::reset() noexcept {
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
