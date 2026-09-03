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
         value.encode_srgb <= 1 && std::isfinite(value.inverse_width) &&
         std::isfinite(value.inverse_height) && value.inverse_width >= 0.0F &&
         value.inverse_height >= 0.0F;
}

bool compatible_output(granit::texture_format format,
                       const tone_mapping_constants& value) noexcept {
  const auto transfer = value.encode_srgb != 0 ? tone_mapping_output_transfer::shader_srgb
                                               : tone_mapping_output_transfer::attachment_srgb;
  return validate_tone_mapping_output(format, transfer) == tone_mapping_error::none;
}

} // namespace

granit_result tone_mapping_pipeline_resources::initialize(
    granit_renderer renderer, granit::texture_format output_format,
    std::span<const std::byte> vertex_code, std::span<const std::byte> fragment_code,
    std::string_view wgsl) noexcept {
  if (renderer == GRANIT_NULL_HANDLE || initialized() ||
      output_format == granit::texture_format::undefined || vertex_code.empty() ||
      fragment_code.empty()) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  auto result = sampler_.initialize(renderer, {.mag_filter = granit::filter::linear,
                                               .min_filter = granit::filter::linear,
                                               .address_u = granit::address_mode::clamp_to_edge,
                                               .address_v = granit::address_mode::clamp_to_edge,
                                               .address_w = granit::address_mode::clamp_to_edge});
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
  const std::array layouts{group_layout_.native_handle()};
  if (granit::succeeded(result))
    result = pipeline_layout_.initialize(renderer, layouts);
  if (granit::succeeded(result)) {
    result = wgsl.empty()
                 ? vertex_shader_.initialize(renderer, {.stage = granit::shader_stage::vertex,
                                                        .code = vertex_code,
                                                        .entry_point = "vertex_main"})
                 : vertex_shader_.initialize_asset(
                       renderer, {.stage = granit::shader_stage::vertex,
                                  .spirv = vertex_code,
                                  .wgsl = wgsl,
                                  .entry_point = "vertex_main"});
  }
  if (granit::succeeded(result)) {
    result = wgsl.empty()
                 ? fragment_shader_.initialize(renderer, {.stage = granit::shader_stage::fragment,
                                                          .code = fragment_code,
                                                          .entry_point = "fragment_main"})
                 : fragment_shader_.initialize_asset(
                       renderer, {.stage = granit::shader_stage::fragment,
                                  .spirv = fragment_code,
                                  .wgsl = wgsl,
                                  .entry_point = "fragment_main"});
  }
  if (granit::succeeded(result)) {
    result = pipeline_.initialize(
        renderer, {.layout = pipeline_layout_.native_handle(),
                   .vertex_shader = vertex_shader_.native_handle(),
                   .fragment_shader = fragment_shader_.native_handle(),
                   .color_formats = std::span{&output_format, 1},
                   .depth_stencil_format = granit::texture_format::undefined,
                   .samples = granit::sample_count::one,
                   .vertex_buffers = {},
                   .primitive = {.topology = granit::primitive_topology::triangle_list,
                                 .front = granit::front_face::clockwise,
                                 .cull = granit::cull_mode::back},
                   .depth = std::nullopt,
                   .color_blends = {},
                   .depth_bias = std::nullopt});
  }
  if (granit::failed(result)) {
    static_cast<void>(reset());
    return static_cast<granit_result>(result);
  }
  renderer_ = renderer;
  output_format_ = output_format;
  return GRANIT_SUCCESS;
}

granit_result tone_mapping_pipeline_resources::reset() noexcept {
  granit_result first = GRANIT_SUCCESS;
  const auto capture = [&](granit::result value) {
    if (first == GRANIT_SUCCESS && granit::failed(value))
      first = static_cast<granit_result>(value);
  };
  capture(pipeline_.reset());
  capture(fragment_shader_.reset());
  capture(vertex_shader_.reset());
  capture(pipeline_layout_.reset());
  capture(group_layout_.reset());
  capture(sampler_.reset());
  renderer_ = GRANIT_NULL_HANDLE;
  output_format_ = granit::texture_format::undefined;
  return first;
}

granit_result
tone_mapping_binding_resources::initialize(const tone_mapping_pipeline_resources& pipeline,
                                           granit_texture_view hdr_view,
                                           const tone_mapping_constants& values) noexcept {
  if (!pipeline.initialized() || initialized() || hdr_view == GRANIT_NULL_HANDLE ||
      !valid(values) || !compatible_output(pipeline.output_format(), values)) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  auto result = constants_.initialize(
      pipeline.renderer(),
      {.size = sizeof(values),
       .usage = granit::buffer_usage::uniform | granit::buffer_usage::transfer_destination,
       .location = granit::memory_location::automatic},
      bytes(values));
  const std::array entries{granit::bind_group_entry{.binding = 0,
                                                    .resource = constants_.native_handle(),
                                                    .offset = 0,
                                                    .size = sizeof(values)},
                           granit::bind_group_entry{.binding = 1, .resource = hdr_view},
                           granit::bind_group_entry{.binding = 2, .resource = pipeline.sampler()}};
  if (granit::succeeded(result))
    result = group_.initialize(pipeline.renderer(), pipeline.group_layout(), entries);
  if (granit::failed(result)) {
    static_cast<void>(reset());
    return static_cast<granit_result>(result);
  }
  return GRANIT_SUCCESS;
}

granit_result
tone_mapping_binding_resources::update(const tone_mapping_constants& values) noexcept {
  if (!initialized() || !valid(values))
    return GRANIT_ERROR_INVALID_ARGUMENT;
  return static_cast<granit_result>(constants_.write(0, bytes(values)));
}

granit_result tone_mapping_binding_resources::reset() noexcept {
  auto first = static_cast<granit_result>(group_.reset());
  const auto constants_result = static_cast<granit_result>(constants_.reset());
  if (first == GRANIT_SUCCESS)
    first = constants_result;
  return first;
}

granit_result tone_mapping_resources::initialize(
    granit_renderer renderer, granit_texture_view hdr_view, granit::texture_format output_format,
    const tone_mapping_constants& values, std::span<const std::byte> vertex_code,
    std::span<const std::byte> fragment_code) noexcept {
  auto result = pipeline_.initialize(renderer, output_format, vertex_code, fragment_code);
  if (result == GRANIT_SUCCESS)
    result = binding_.initialize(pipeline_, hdr_view, values);
  if (result != GRANIT_SUCCESS)
    static_cast<void>(reset());
  return result;
}

granit_result tone_mapping_resources::update(const tone_mapping_constants& values) noexcept {
  return binding_.update(values);
}

granit_result tone_mapping_resources::reset() noexcept {
  auto first = binding_.reset();
  const auto pipeline_result = pipeline_.reset();
  if (first == GRANIT_SUCCESS)
    first = pipeline_result;
  return first;
}

} // namespace granit::lighting
