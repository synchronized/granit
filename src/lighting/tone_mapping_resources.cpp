// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "lighting/tone_mapping_resources.h"

#include <array>
#include <cmath>

namespace granit::lighting {
namespace {

std::span<const std::byte> bytes(const tone_mapping_constants& value) noexcept {
  return {reinterpret_cast<const std::byte*>(&value), sizeof(value)};
}

bool valid(const tone_mapping_constants& value) noexcept {
  return std::isfinite(value.exposure_scale) && value.exposure_scale > 0.0F &&
         value.encode_srgb <= 1;
}

} // namespace

granit_result tone_mapping_resources::initialize(
    granit_renderer renderer, granit_texture_view hdr_view, granit::texture_format output_format,
    const tone_mapping_constants& values, std::span<const std::byte> vertex_code,
    std::span<const std::byte> fragment_code) noexcept {
  if (renderer == GRANIT_NULL_HANDLE || hdr_view == GRANIT_NULL_HANDLE || initialized() ||
      output_format == granit::texture_format::undefined || !valid(values) || vertex_code.empty() ||
      fragment_code.empty())
    return GRANIT_ERROR_INVALID_ARGUMENT;

  auto result = constants_.initialize(
      renderer,
      {.size = sizeof(values),
       .usage = granit::buffer_usage::uniform | granit::buffer_usage::transfer_destination,
       .location = granit::memory_location::automatic},
      bytes(values));
  if (granit::succeeded(result)) {
    result = sampler_.initialize(renderer, {.mag_filter = granit::filter::linear,
                                            .min_filter = granit::filter::linear,
                                            .address_u = granit::address_mode::clamp_to_edge,
                                            .address_v = granit::address_mode::clamp_to_edge,
                                            .address_w = granit::address_mode::clamp_to_edge});
  }
  constexpr auto fragment = granit::shader_stage_flags::fragment;
  const std::array layout_entries{
      granit::bind_group_layout_entry{.binding = 0,
                                      .type = granit::binding_type::uniform_buffer,
                                      .array_count = 1,
                                      .visibility = fragment},
      granit::bind_group_layout_entry{.binding = 1,
                                      .type = granit::binding_type::sampled_texture,
                                      .array_count = 1,
                                      .visibility = fragment},
      granit::bind_group_layout_entry{.binding = 2,
                                      .type = granit::binding_type::sampler,
                                      .array_count = 1,
                                      .visibility = fragment}};
  if (granit::succeeded(result))
    result = group_layout_.initialize(renderer, layout_entries);
  const std::array group_entries{
      granit::bind_group_entry{.binding = 0,
                               .resource = constants_.native_handle(),
                               .offset = 0,
                               .size = sizeof(values)},
      granit::bind_group_entry{.binding = 1, .resource = hdr_view},
      granit::bind_group_entry{.binding = 2, .resource = sampler_.native_handle()}};
  if (granit::succeeded(result))
    result = group_.initialize(renderer, group_layout_.native_handle(), group_entries);
  const std::array layouts{group_layout_.native_handle()};
  if (granit::succeeded(result))
    result = pipeline_layout_.initialize(renderer, layouts);
  if (granit::succeeded(result)) {
    result = vertex_shader_.initialize(
        renderer,
        {.stage = granit::shader_stage::vertex, .code = vertex_code, .entry_point = "vertex_main"});
  }
  if (granit::succeeded(result)) {
    result = fragment_shader_.initialize(
        renderer, {.stage = granit::shader_stage::fragment,
                   .code = fragment_code,
                   .entry_point = "fragment_main"});
  }
  if (granit::succeeded(result)) {
    result = pipeline_.initialize(
        renderer, {.layout = pipeline_layout_.native_handle(),
                   .vertex_shader = vertex_shader_.native_handle(),
                   .fragment_shader = fragment_shader_.native_handle(),
                   .color_formats = std::span{&output_format, 1},
                   .primitive = {.topology = granit::primitive_topology::triangle_list,
                                 .front = granit::front_face::clockwise,
                                 .cull = granit::cull_mode::back}});
  }
  if (granit::failed(result)) {
    static_cast<void>(reset());
    return static_cast<granit_result>(result);
  }
  return GRANIT_SUCCESS;
}

granit_result tone_mapping_resources::update(const tone_mapping_constants& values) noexcept {
  if (!initialized() || !valid(values))
    return GRANIT_ERROR_INVALID_ARGUMENT;
  return static_cast<granit_result>(constants_.write(0, bytes(values)));
}

granit_result tone_mapping_resources::reset() noexcept {
  granit_result first = GRANIT_SUCCESS;
  const auto capture = [&](granit::result value) {
    if (first == GRANIT_SUCCESS && granit::failed(value))
      first = static_cast<granit_result>(value);
  };
  capture(pipeline_.reset());
  capture(fragment_shader_.reset());
  capture(vertex_shader_.reset());
  capture(pipeline_layout_.reset());
  capture(group_.reset());
  capture(group_layout_.reset());
  capture(sampler_.reset());
  capture(constants_.reset());
  return first;
}

} // namespace granit::lighting
