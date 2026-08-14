// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "lighting/ibl_reference.h"
#include "lighting/lighting_reference.h"
#include "lighting/shadow_ibl_resources.h"
#include "lighting/tone_mapping_reference.h"
#include "lighting/tone_mapping_resources.h"
#include "material/material_gpu_instance.h"
#include "material/material_package.h"
#include "material/material_template_gpu.h"
#include "material/pbr_default_resources.h"
#include "material/pbr_material_schema.h"
#include "material/pbr_reference.h"
#include "pbr_example_support.h"

#include <granit/granit.hpp>

#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <numeric>
#include <string>
#include <string_view>
#include <vector>

#ifndef GRANIT_BENCHMARK_REVISION
#define GRANIT_BENCHMARK_REVISION "unknown"
#define GRANIT_BENCHMARK_COMPILER "unknown"
#define GRANIT_BENCHMARK_SYSTEM "unknown"
#define GRANIT_BENCHMARK_LINK_MODE "unknown"
#endif

namespace {

struct run_options {
  std::uint32_t point_lights = 1;
  std::uint32_t iterations = 1;
  std::uint32_t samples = 1;
  std::uint32_t warmup = 0;
  bool benchmark = false;
};

bool parse_u32(std::string_view text, std::uint32_t& value) {
  std::uint32_t parsed = 0;
  const auto result = std::from_chars(text.data(), text.data() + text.size(), parsed);
  if (result.ec != std::errc{} || result.ptr != text.data() + text.size())
    return false;
  value = parsed;
  return true;
}

struct gpu_sample {
  double cpu_end_to_end = 0.0;
  double shadow = 0.0;
  double pbr_hdr = 0.0;
  double tone_mapping = 0.0;
  double total = 0.0;
};

struct sample_summary {
  double mean = 0.0;
  double p50 = 0.0;
  double p95 = 0.0;
  double p99 = 0.0;
};

sample_summary summarize(std::vector<double> values) {
  std::ranges::sort(values);
  const auto percentile = [&](double fraction) {
    const auto index =
        static_cast<std::size_t>(std::ceil(fraction * static_cast<double>(values.size())) - 1.0);
    return values[std::min(index, values.size() - 1)];
  };
  return {.mean = std::accumulate(values.begin(), values.end(), 0.0) /
                  static_cast<double>(values.size()),
          .p50 = percentile(0.50),
          .p95 = percentile(0.95),
          .p99 = percentile(0.99)};
}

bool parse_options(int argc, char** argv, run_options& options) {
#ifdef GRANIT_PBR_GPU_BENCHMARK
  options = {.point_lights = 64, .iterations = 20, .samples = 20, .warmup = 5, .benchmark = true};
#endif
  for (int index = 1; index < argc; ++index) {
    const std::string_view argument{argv[index]};
    if (argument == "--help") {
      std::cout << "用法：granit_lighting_gpu_benchmarks [--lights N] [--iterations N] "
                   "[--samples N] [--warmup N]\n";
      return false;
    }
    if (index + 1 >= argc)
      return false;
    const std::string_view value{argv[++index]};
    auto* target = argument == "--lights"       ? &options.point_lights
                   : argument == "--iterations" ? &options.iterations
                   : argument == "--samples"    ? &options.samples
                   : argument == "--warmup"     ? &options.warmup
                                                : nullptr;
    if (target == nullptr || !parse_u32(value, *target))
      return false;
  }
  return options.point_lights > 0 && options.point_lights <= 128 && options.iterations > 0 &&
         options.iterations <= 10'000 && options.samples > 0 && options.samples <= 1'000 &&
         options.warmup <= 1'000;
}

std::vector<std::uint32_t> load_shader(std::string_view name) {
  const auto directory =
      name.starts_with("tone_mapping") ? GRANIT_PIPELINE_SHADER_DIR : GRANIT_PBR_SHADER_DIR;
  const auto path = std::string{directory} + "/" + std::string{name};
  std::ifstream stream{path, std::ios::binary};
  const std::vector<char> bytes{std::istreambuf_iterator<char>{stream}, {}};
  if (bytes.empty() || bytes.size() % sizeof(std::uint32_t) != 0)
    return {};
  std::vector<std::uint32_t> words(bytes.size() / sizeof(std::uint32_t));
  std::memcpy(words.data(), bytes.data(), bytes.size());
  return words;
}

std::uint8_t quantize_unorm(float value) {
  return static_cast<std::uint8_t>(std::lround(std::clamp(value, 0.0F, 1.0F) * 255.0F));
}

bool near_pixel(const std::uint8_t* actual, const std::array<std::uint8_t, 4>& expected,
                std::uint8_t tolerance) {
  for (std::size_t channel = 0; channel < expected.size(); ++channel) {
    const auto difference = std::abs(static_cast<int>(actual[channel]) - expected[channel]);
    if (difference > tolerance)
      return false;
  }
  return true;
}

template <typename T> std::span<const std::byte> bytes(const T& value) {
  return {reinterpret_cast<const std::byte*>(value.data()), sizeof(value)};
}

} // namespace

int main(int argc, char** argv) {
  run_options options;
  if (!parse_options(argc, argv, options))
    return argc > 1 && std::string_view{argv[1]} == "--help" ? 0 : 2;
  granit::renderer renderer;
  auto result =
      renderer.initialize({.application_name = "Granit Textured PBR", .enable_validation = true});
  granit::material::material_package direct_package;
  granit::material::material_package ibl_package;
  granit::material::material_package shadow_package;
  granit::material::material_package full_package;
  const auto packages_ready =
      granit::examples::build_pbr_package(direct_package, load_shader("pbr_lights.vert.spv"),
                                          load_shader("pbr_lights_untextured.frag.spv")) &&
      granit::examples::build_pbr_package(ibl_package, load_shader("pbr_lights.vert.spv"),
                                          load_shader("pbr_ibl_lights_untextured.frag.spv")) &&
      granit::examples::build_pbr_package(shadow_package,
                                          load_shader("pbr_shadow_ibl_lights.vert.spv"),
                                          load_shader("pbr_shadow_lights_untextured.frag.spv")) &&
      granit::examples::build_pbr_package(full_package,
                                          load_shader("pbr_shadow_ibl_lights.vert.spv"),
                                          load_shader("pbr_shadow_ibl_lights_untextured.frag.spv"));
  if (granit::failed(result) || !packages_ready) {
    std::cerr << "无法初始化 Renderer 或构建 PBR 材质包\n";
    return 1;
  }

  granit::texture shadow_texture;
  granit::texture_view shadow_view;
  granit::texture irradiance_texture;
  granit::texture prefiltered_texture;
  granit::texture brdf_lut_texture;
  granit::texture_view irradiance_view;
  granit::texture_view prefiltered_view;
  granit::texture_view brdf_lut_view;
  if (granit::succeeded(result)) {
    result = shadow_texture.initialize(
        renderer.native_handle(),
        {.format = granit::texture_format::d32_float,
         .usage = granit::texture_usage::depth_stencil_attachment | granit::texture_usage::sampled,
         .width = 1,
         .height = 1});
  }
  if (granit::succeeded(result))
    result = shadow_view.initialize(renderer.native_handle(), shadow_texture.native_handle());
  const auto cube_desc = granit::texture_desc{.dimension = granit::texture_dimension::cube,
                                              .format = granit::texture_format::rgba16_float,
                                              .usage = granit::texture_usage::sampled |
                                                       granit::texture_usage::transfer_destination,
                                              .width = 1,
                                              .height = 1,
                                              .array_layers = 6};
  if (granit::succeeded(result))
    result = irradiance_texture.initialize(renderer.native_handle(), cube_desc);
  if (granit::succeeded(result))
    result = prefiltered_texture.initialize(renderer.native_handle(), cube_desc);
  if (granit::succeeded(result)) {
    result = brdf_lut_texture.initialize(
        renderer.native_handle(),
        {.format = granit::texture_format::rgba16_float,
         .usage = granit::texture_usage::sampled | granit::texture_usage::transfer_destination});
  }
  // RGBA16_FLOAT：Cube 每面为 (1,1,1,1)，LUT 为 (0.5,0.1,0,1)。
  constexpr std::array<std::uint16_t, 24> cube_pixels{
      0x3c00, 0x3c00, 0x3c00, 0x3c00, 0x3c00, 0x3c00, 0x3c00, 0x3c00,
      0x3c00, 0x3c00, 0x3c00, 0x3c00, 0x3c00, 0x3c00, 0x3c00, 0x3c00,
      0x3c00, 0x3c00, 0x3c00, 0x3c00, 0x3c00, 0x3c00, 0x3c00, 0x3c00};
  constexpr std::array<std::uint16_t, 4> lut_pixel{0x3800, 0x2e66, 0x0000, 0x3c00};
  if (granit::succeeded(result)) {
    result = irradiance_texture.write(bytes(cube_pixels), {.bytes_per_row = 8, .rows_per_image = 1},
                                      {.array_layer_count = 6});
  }
  if (granit::succeeded(result)) {
    result = prefiltered_texture.write(
        bytes(cube_pixels), {.bytes_per_row = 8, .rows_per_image = 1}, {.array_layer_count = 6});
  }
  if (granit::succeeded(result))
    result = brdf_lut_texture.write(bytes(lut_pixel), {.bytes_per_row = 8}, {});
  const granit::texture_view_desc cube_view_desc{.dimension = granit::texture_dimension::cube,
                                                 .array_layer_count = 6};
  if (granit::succeeded(result)) {
    result = irradiance_view.initialize(renderer.native_handle(),
                                        irradiance_texture.native_handle(), cube_view_desc);
  }
  if (granit::succeeded(result)) {
    result = prefiltered_view.initialize(renderer.native_handle(),
                                         prefiltered_texture.native_handle(), cube_view_desc);
  }
  if (granit::succeeded(result)) {
    result = brdf_lut_view.initialize(renderer.native_handle(), brdf_lut_texture.native_handle());
  }
  granit::lighting::shadow_ibl_resources direct_resources;
  granit::lighting::shadow_ibl_resources ibl_resources;
  granit::lighting::shadow_ibl_resources shadow_resources;
  granit::lighting::shadow_ibl_resources lighting_resources;
  const granit::lighting::light_limits light_capacities{.directional = 1, .point = 128, .spot = 1};
  if (granit::succeeded(result)) {
    result = granit::from_native(direct_resources.initialize(
        renderer.native_handle(), {}, {}, {}, light_capacities, {.shadows = false, .ibl = false}));
  }
  if (granit::succeeded(result)) {
    result = granit::from_native(ibl_resources.initialize(
        renderer.native_handle(),
        {.ibl = {.irradiance = irradiance_view.native_handle(),
                 .prefiltered_environment = prefiltered_view.native_handle(),
                 .brdf_lut = brdf_lut_view.native_handle()}},
        {}, {.intensity = 0.25F, .prefiltered_max_mip = 0.0F}, light_capacities,
        {.shadows = false, .ibl = true}));
  }
  if (granit::succeeded(result)) {
    result = granit::from_native(shadow_resources.initialize(
        renderer.native_handle(), {.shadow = shadow_view.native_handle()},
        {.light_view_projection = granit::math::identity_matrix4,
         .depth_bias = 0.0F,
         .normal_bias = 0.0F,
         .texel_size = {1.0F, 1.0F}},
        {}, light_capacities, {.shadows = true, .ibl = false}));
  }
  if (granit::succeeded(result)) {
    result = granit::from_native(lighting_resources.initialize(
        renderer.native_handle(),
        {.shadow = shadow_view.native_handle(),
         .ibl = {.irradiance = irradiance_view.native_handle(),
                 .prefiltered_environment = prefiltered_view.native_handle(),
                 .brdf_lut = brdf_lut_view.native_handle()}},
        {.light_view_projection = granit::math::identity_matrix4,
         .depth_bias = 0.0F,
         .normal_bias = 0.0F,
         .texel_size = {1.0F, 1.0F}},
        {.intensity = 0.25F, .prefiltered_max_mip = 0.0F}, light_capacities));
  }
  granit::lighting::packed_view_lights visible_lights;
  if (granit::succeeded(result)) {
    visible_lights.directional.push_back(
        {.direction_to_light = {0.0F, 0.0F, 1.0F}, .radiance = {1.0F, 1.0F, 1.0F}});
    if (options.benchmark) {
      for (std::uint32_t index = 0; index < options.point_lights; ++index) {
        const auto column = static_cast<float>(index % 16) - 7.5F;
        const auto row = static_cast<float>(index / 16) - 3.5F;
        visible_lights.point.push_back({.position = {column * 0.12F, row * 0.12F, 1.5F},
                                        .radius = 3.0F,
                                        .intensity = {0.01F, 0.008F, 0.006F}});
      }
    } else {
      visible_lights.point.push_back(
          {.position = {0.0F, 0.0F, 1.5F}, .radius = 3.0F, .intensity = {0.3F, 0.2F, 0.1F}});
      visible_lights.spot.push_back({.position = {0.0F, 0.0F, 1.5F},
                                     .radius = 3.0F,
                                     .direction = {0.0F, 0.0F, -1.0F},
                                     .outer_angle_cosine = std::cos(0.6F),
                                     .intensity = {0.1F, 0.2F, 0.3F},
                                     .inner_angle_cosine = std::cos(0.2F)});
    }
    result = granit::from_native(direct_resources.update_lights(visible_lights));
    if (granit::succeeded(result))
      result = granit::from_native(ibl_resources.update_lights(visible_lights));
    if (granit::succeeded(result))
      result = granit::from_native(shadow_resources.update_lights(visible_lights));
    if (granit::succeeded(result))
      result = granit::from_native(lighting_resources.update_lights(visible_lights));
  }
  granit::bind_group_layout object_layout;
  if (granit::succeeded(result))
    result = object_layout.initialize(renderer.native_handle(), {});

  granit::material::material_template_gpu direct_material;
  granit::material::material_template_gpu ibl_material;
  granit::material::material_template_gpu shadow_material;
  granit::material::material_template_gpu material;
  granit_graphics_pipeline direct_pipeline = GRANIT_NULL_HANDLE;
  granit_graphics_pipeline ibl_pipeline = GRANIT_NULL_HANDLE;
  granit_graphics_pipeline shadow_pipeline = GRANIT_NULL_HANDLE;
  granit_graphics_pipeline pipeline = GRANIT_NULL_HANDLE;
  const auto initialize_material = [&](granit::material::material_template_gpu& target,
                                       const granit::material::material_package& source,
                                       granit_bind_group_layout lighting_layout,
                                       granit_graphics_pipeline& target_pipeline) {
    const std::array additional_layouts{object_layout.native_handle(), lighting_layout};
    auto initialize_result = granit::from_native(
        target.initialize(renderer.native_handle(), source, additional_layouts));
    const std::array features{granit::material::material_feature_value{
        granit::material::make_feature_id(granit::material::pbr_texture_feature_name), 0}};
    if (granit::succeeded(initialize_result)) {
      initialize_result = granit::from_native(
          target.acquire_pipeline({.pass = granit::material::make_feature_id("opaque"),
                                   .variant = granit::material::make_variant_key(features),
                                   .color_format = GRANIT_TEXTURE_FORMAT_RGBA16_FLOAT,
                                   .depth_stencil_format = GRANIT_TEXTURE_FORMAT_D32_FLOAT},
                                  target_pipeline));
    }
    return initialize_result;
  };
  if (granit::succeeded(result))
    result = initialize_material(direct_material, direct_package, direct_resources.layout(),
                                 direct_pipeline);
  if (granit::succeeded(result))
    result = initialize_material(ibl_material, ibl_package, ibl_resources.layout(), ibl_pipeline);
  if (granit::succeeded(result)) {
    result = initialize_material(shadow_material, shadow_package, shadow_resources.layout(),
                                 shadow_pipeline);
  }
  if (granit::succeeded(result))
    result = initialize_material(material, full_package, lighting_resources.layout(), pipeline);
  if (granit::succeeded(result) &&
      (direct_pipeline == GRANIT_NULL_HANDLE || ibl_pipeline == GRANIT_NULL_HANDLE ||
       shadow_pipeline == GRANIT_NULL_HANDLE || pipeline == GRANIT_NULL_HANDLE)) {
    result = granit::result::internal;
  }

  granit::material::pbr_default_resources defaults;
  granit::material::material_gpu_instance direct_instance;
  granit::material::material_gpu_instance ibl_instance;
  granit::material::material_gpu_instance shadow_instance;
  granit::material::material_gpu_instance instance;
  if (granit::succeeded(result))
    result = granit::from_native(defaults.initialize(renderer.native_handle()));
  if (granit::succeeded(result)) {
    result = granit::examples::initialize_pbr_instance(renderer.native_handle(), direct_material,
                                                       direct_package, defaults, direct_instance);
  }
  if (granit::succeeded(result)) {
    result = granit::examples::initialize_pbr_instance(renderer.native_handle(), ibl_material,
                                                       ibl_package, defaults, ibl_instance);
  }
  if (granit::succeeded(result)) {
    result = granit::examples::initialize_pbr_instance(renderer.native_handle(), shadow_material,
                                                       shadow_package, defaults, shadow_instance);
  }
  if (granit::succeeded(result)) {
    result = granit::examples::initialize_pbr_instance(renderer.native_handle(), material,
                                                       full_package, defaults, instance);
  }

  granit_texture hdr_texture = GRANIT_NULL_HANDLE;
  granit_texture_view hdr_view = GRANIT_NULL_HANDLE;
  granit_texture output_texture = GRANIT_NULL_HANDLE;
  granit_texture_view output_view = GRANIT_NULL_HANDLE;
  granit_texture depth_texture = GRANIT_NULL_HANDLE;
  granit_texture_view depth_view = GRANIT_NULL_HANDLE;
  auto create_attachment = [&](granit_texture_format format, granit_texture_usage usage,
                               granit_texture& texture, granit_texture_view& view) {
    granit_texture_desc desc = GRANIT_TEXTURE_DESC_INIT;
    desc.format = format;
    desc.usage = usage;
    desc.width = 256;
    desc.height = 256;
    return granit::from_native(
        granit_texture_create_with_default_view(renderer.native_handle(), &desc, &texture, &view));
  };
  if (granit::succeeded(result))
    result = create_attachment(GRANIT_TEXTURE_FORMAT_RGBA16_FLOAT,
                               GRANIT_TEXTURE_USAGE_COLOR_ATTACHMENT_BIT |
                                   GRANIT_TEXTURE_USAGE_SAMPLED_BIT,
                               hdr_texture, hdr_view);
  if (granit::succeeded(result))
    result = create_attachment(GRANIT_TEXTURE_FORMAT_RGBA8_UNORM,
                               GRANIT_TEXTURE_USAGE_COLOR_ATTACHMENT_BIT |
                                   GRANIT_TEXTURE_USAGE_TRANSFER_SOURCE_BIT,
                               output_texture, output_view);
  if (granit::succeeded(result))
    result = create_attachment(GRANIT_TEXTURE_FORMAT_D32_FLOAT,
                               GRANIT_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, depth_texture,
                               depth_view);

  granit::command_recorder recorder;
  granit::timestamp_query_pool timestamps;
  constexpr std::uint32_t render_size = 256;
  constexpr std::uint64_t readback_size = render_size * render_size * 4;
  granit::buffer readback;
  if (granit::succeeded(result)) {
    result = readback.initialize(renderer.native_handle(),
                                 {.size = readback_size,
                                  .usage = granit::buffer_usage::transfer_destination,
                                  .location = granit::memory_location::readback});
  }
  if (granit::succeeded(result))
    result = recorder.initialize(renderer.native_handle());
  if (granit::succeeded(result))
    result = timestamps.initialize(renderer.native_handle(), 4);
  const auto tone_vertex = load_shader("tone_mapping.vert.spv");
  const auto tone_fragment = load_shader("tone_mapping.frag.spv");
  granit::lighting::tone_mapping_resources tone_mapping;
  if (granit::succeeded(result)) {
    result = granit::from_native(tone_mapping.initialize(
        renderer.native_handle(), hdr_view, granit::texture_format::rgba8_unorm,
        {.exposure_scale = 1.0F, .encode_srgb = 1}, std::as_bytes(std::span{tone_vertex}),
        std::as_bytes(std::span{tone_fragment})));
  }
  const granit::viewport viewport{0, 0, 256, 256, 0, 1};
  const granit::scissor scissor{0, 0, 256, 256};
  const granit::color_attachment_desc color{
      .view = hdr_view,
      .clear_value = {.red = 0.03F, .green = 0.03F, .blue = 0.05F, .alpha = 1.0F}};
  const granit::depth_stencil_attachment_desc depth{.view = depth_view};
  const granit::rendering_desc rendering{.color_attachments = std::span{&color, 1},
                                         .depth_stencil_attachment = &depth,
                                         .area = {0, 0, 256, 256}};
  const granit::color_attachment_desc output_color{.view = output_view};
  const granit::rendering_desc output_rendering{.color_attachments = std::span{&output_color, 1},
                                                .area = {0, 0, 256, 256}};
  const granit_texture_data_layout readback_layout{};
  const granit_texture_write_region readback_region{.mip_level = 0,
                                                    .base_array_layer = 0,
                                                    .array_layer_count = 1,
                                                    .aspect = GRANIT_TEXTURE_ASPECT_COLOR_BIT,
                                                    .x = 0,
                                                    .y = 0,
                                                    .z = 0,
                                                    .width = render_size,
                                                    .height = render_size,
                                                    .depth = 1};
  const auto reference = granit::material::evaluate_pbr_direct_light(
      {.base_color = {0.8F, 0.2F, 0.1F}, .metallic = 0.5F, .perceptual_roughness = 0.5F},
      {.normal = {1.0F / 255.0F, 1.0F / 255.0F, 1.0F}});
  const auto ibl_reference = granit::lighting::evaluate_pbr_ibl(
      {.base_color = {0.8F, 0.2F, 0.1F}, .metallic = 0.5F, .perceptual_roughness = 0.5F},
      {.normal = {1.0F / 255.0F, 1.0F / 255.0F, 1.0F},
       .irradiance = {1.0F, 1.0F, 1.0F},
       .prefiltered_radiance = {1.0F, 1.0F, 1.0F},
       .brdf_lut = {0.5F, 0.0999755859F},
       .environment_intensity = 0.25F});
  const granit::lighting::lighting_surface surface{
      .material = {.base_color = {0.8F, 0.2F, 0.1F},
                   .metallic = 0.5F,
                   .perceptual_roughness = 0.5F},
      .position = {1.0F / 256.0F, -1.0F / 256.0F, 0.5F},
      .normal = {1.0F / 255.0F, 1.0F / 255.0F, 1.0F}};
  const auto point_reference = granit::lighting::evaluate_point_light(
      surface, {.position = {0.0F, 0.0F, 1.5F}, .intensity = {0.3F, 0.2F, 0.1F}, .radius = 3.0F});
  const auto spot_reference =
      granit::lighting::evaluate_spot_light(surface, {.position = {0.0F, 0.0F, 1.5F},
                                                      .direction = {0.0F, 0.0F, -1.0F},
                                                      .intensity = {0.1F, 0.2F, 0.3F},
                                                      .radius = 3.0F,
                                                      .inner_angle = 0.2F,
                                                      .outer_angle = 0.6F});
  std::array<std::uint64_t, 4> gpu_timestamps{};
  const auto render_case = [&](float stored_shadow_depth, bool expected_lit,
                               granit::material::material_template_gpu& selected_material,
                               granit_graphics_pipeline selected_pipeline,
                               granit_bind_group selected_material_group,
                               granit_bind_group selected_lighting_group, bool shadows,
                               bool direct_lighting, bool ibl_lighting) {
    auto case_result = recorder.begin();
    if (granit::succeeded(case_result))
      case_result = recorder.reset_timestamp_queries(timestamps.native_handle(), 0, 4);
    if (granit::succeeded(case_result)) {
      case_result =
          recorder.write_timestamp(timestamps.native_handle(), GRANIT_TIMESTAMP_STAGE_TOP, 0);
    }
    const granit::depth_stencil_attachment_desc shadow_depth{
        .view = shadow_view.native_handle(), .clear_value = {.depth = stored_shadow_depth}};
    const granit::rendering_desc shadow_rendering{
        .color_attachments = {}, .depth_stencil_attachment = &shadow_depth, .area = {0, 0, 1, 1}};
    if (granit::succeeded(case_result) && shadows)
      case_result = recorder.begin_rendering(shadow_rendering);
    if (granit::succeeded(case_result) && shadows)
      case_result = recorder.end_rendering();
    if (granit::succeeded(case_result)) {
      case_result =
          recorder.write_timestamp(timestamps.native_handle(), GRANIT_TIMESTAMP_STAGE_DRAW, 1);
    }
    if (granit::succeeded(case_result))
      case_result = recorder.bind_graphics_pipeline(selected_pipeline);
    if (granit::succeeded(case_result)) {
      case_result = recorder.bind_graphics_groups(selected_material.pipeline_layout(), 1,
                                                  std::span{&selected_material_group, 1});
    }
    if (granit::succeeded(case_result)) {
      case_result = recorder.bind_graphics_groups(selected_material.pipeline_layout(), 3,
                                                  std::span{&selected_lighting_group, 1});
    }
    if (granit::succeeded(case_result))
      case_result = recorder.set_viewports(0, std::span{&viewport, 1});
    if (granit::succeeded(case_result))
      case_result = recorder.set_scissors(0, std::span{&scissor, 1});
    if (granit::succeeded(case_result))
      case_result = recorder.begin_rendering(rendering);
    if (granit::succeeded(case_result))
      case_result = recorder.draw(3);
    if (granit::succeeded(case_result))
      case_result = recorder.end_rendering();
    if (granit::succeeded(case_result)) {
      case_result =
          recorder.write_timestamp(timestamps.native_handle(), GRANIT_TIMESTAMP_STAGE_DRAW, 2);
    }
    if (granit::succeeded(case_result)) {
      case_result = recorder.bind_graphics_pipeline(tone_mapping.pipeline());
    }
    const auto tone_group = tone_mapping.group();
    if (granit::succeeded(case_result)) {
      case_result = recorder.bind_graphics_groups(tone_mapping.pipeline_layout(), 0,
                                                  std::span{&tone_group, 1});
    }
    if (granit::succeeded(case_result))
      case_result = recorder.begin_rendering(output_rendering);
    if (granit::succeeded(case_result))
      case_result = recorder.draw(3);
    if (granit::succeeded(case_result))
      case_result = recorder.end_rendering();
    if (granit::succeeded(case_result)) {
      case_result =
          recorder.write_timestamp(timestamps.native_handle(), GRANIT_TIMESTAMP_STAGE_BOTTOM, 3);
    }
    if (granit::succeeded(case_result) && !options.benchmark) {
      case_result = recorder.copy_texture_to_buffer(output_texture, readback.native_handle(),
                                                    readback_layout, readback_region);
    }
    if (granit::succeeded(case_result))
      case_result = recorder.end();
    if (granit::succeeded(case_result))
      case_result = recorder.submit();
    if (granit::succeeded(case_result))
      case_result = recorder.reset();
    if (granit::succeeded(case_result))
      case_result = timestamps.get_results(0, gpu_timestamps);
    if (granit::failed(case_result))
      return case_result;
    if (!(gpu_timestamps[0] <= gpu_timestamps[1] && gpu_timestamps[1] <= gpu_timestamps[2] &&
          gpu_timestamps[2] <= gpu_timestamps[3]))
      return granit::result::internal;

    if (options.benchmark)
      return case_result;

    void* mapped = nullptr;
    case_result = readback.map(0, readback_size, &mapped);
    if (granit::succeeded(case_result)) {
      const auto* pixels = static_cast<const std::uint8_t*>(mapped);
      const auto local_lights = direct_lighting ? granit::math::add(point_reference, spot_reference)
                                                : decltype(point_reference){};
      const auto environment = ibl_lighting ? ibl_reference : decltype(ibl_reference){};
      const auto directional =
          direct_lighting && (!shadows || expected_lit) ? reference : decltype(reference){};
      const auto expected_linear =
          granit::math::add(granit::math::add(local_lights, environment), directional);
      granit::math::float3 expected_display{};
      const auto tone_error = granit::lighting::evaluate_tone_mapping(
          expected_linear,
          {.output_transfer = granit::lighting::tone_mapping_output_transfer::shader_srgb},
          expected_display);
      granit::math::float3 expected_clear_display{};
      const auto clear_tone_error = granit::lighting::evaluate_tone_mapping(
          {0.03F, 0.03F, 0.05F},
          {.output_transfer = granit::lighting::tone_mapping_output_transfer::shader_srgb},
          expected_clear_display);
      const std::array expected_center{quantize_unorm(expected_display.x),
                                       quantize_unorm(expected_display.y),
                                       quantize_unorm(expected_display.z), std::uint8_t{255}};
      const std::array expected_clear{quantize_unorm(expected_clear_display.x),
                                      quantize_unorm(expected_clear_display.y),
                                      quantize_unorm(expected_clear_display.z), std::uint8_t{255}};
      const auto* center = pixels + (128 * render_size + 128) * 4;
      const auto* corner = pixels;
      if (tone_error != granit::lighting::tone_mapping_error::none ||
          clear_tone_error != granit::lighting::tone_mapping_error::none ||
          !near_pixel(center, expected_center, 2) || !near_pixel(corner, expected_clear, 2)) {
        std::cerr << "PBR 多光源阴影像素回归失败：中心像素=" << static_cast<unsigned>(center[0])
                  << ',' << static_cast<unsigned>(center[1]) << ','
                  << static_cast<unsigned>(center[2]) << ',' << static_cast<unsigned>(center[3])
                  << '\n';
        case_result = granit::result::internal;
      }
      const auto unmap_result = readback.unmap();
      if (granit::succeeded(case_result))
        case_result = unmap_result;
    }
    return case_result;
  };

  std::vector<gpu_sample> benchmark_samples;
  if (options.benchmark) {
    for (std::uint32_t sample = 0; sample < options.warmup && granit::succeeded(result); ++sample) {
      for (std::uint32_t iteration = 0; iteration < options.iterations && granit::succeeded(result);
           ++iteration) {
        result = render_case(1.0F, true, material, pipeline, instance.bind_group(),
                             lighting_resources.group(), true, true, true);
      }
    }
    benchmark_samples.reserve(options.samples);
    for (std::uint32_t sample = 0; sample < options.samples && granit::succeeded(result);
         ++sample) {
      gpu_sample current{};
      const auto cpu_begin = std::chrono::steady_clock::now();
      for (std::uint32_t iteration = 0; iteration < options.iterations && granit::succeeded(result);
           ++iteration) {
        result = render_case(1.0F, true, material, pipeline, instance.bind_group(),
                             lighting_resources.group(), true, true, true);
        current.shadow += static_cast<double>(gpu_timestamps[1] - gpu_timestamps[0]);
        current.pbr_hdr += static_cast<double>(gpu_timestamps[2] - gpu_timestamps[1]);
        current.tone_mapping += static_cast<double>(gpu_timestamps[3] - gpu_timestamps[2]);
        current.total += static_cast<double>(gpu_timestamps[3] - gpu_timestamps[0]);
      }
      const auto divisor = static_cast<double>(options.iterations);
      current.cpu_end_to_end =
          std::chrono::duration<double, std::nano>(std::chrono::steady_clock::now() - cpu_begin)
              .count() /
          divisor;
      current.shadow /= divisor;
      current.pbr_hdr /= divisor;
      current.tone_mapping /= divisor;
      current.total /= divisor;
      benchmark_samples.push_back(current);
    }
  } else {
    const granit::lighting::packed_view_lights no_lights;
    if (granit::succeeded(result))
      result = granit::from_native(direct_resources.update_lights(no_lights));
    if (granit::succeeded(result)) {
      result =
          render_case(1.0F, true, direct_material, direct_pipeline, direct_instance.bind_group(),
                      direct_resources.group(), false, false, false);
    }
    if (granit::succeeded(result))
      result = granit::from_native(direct_resources.update_lights(visible_lights));
    if (granit::succeeded(result)) {
      result =
          render_case(1.0F, true, direct_material, direct_pipeline, direct_instance.bind_group(),
                      direct_resources.group(), false, true, false);
    }
    if (granit::succeeded(result))
      result = granit::from_native(ibl_resources.update_lights(no_lights));
    if (granit::succeeded(result)) {
      result = render_case(1.0F, true, ibl_material, ibl_pipeline, ibl_instance.bind_group(),
                           ibl_resources.group(), false, false, true);
    }
    if (granit::succeeded(result))
      result = granit::from_native(ibl_resources.update_lights(visible_lights));
    if (granit::succeeded(result)) {
      result = render_case(1.0F, true, ibl_material, ibl_pipeline, ibl_instance.bind_group(),
                           ibl_resources.group(), false, true, true);
    }
    if (granit::succeeded(result)) {
      result =
          render_case(1.0F, true, shadow_material, shadow_pipeline, shadow_instance.bind_group(),
                      shadow_resources.group(), true, true, false);
    }
    if (granit::succeeded(result)) {
      result = render_case(0.25F, false, material, pipeline, instance.bind_group(),
                           lighting_resources.group(), true, true, true);
    }
    if (granit::succeeded(result)) {
      result = render_case(1.0F, true, material, pipeline, instance.bind_group(),
                           lighting_resources.group(), true, true, true);
    }
  }

  static_cast<void>(tone_mapping.reset());
  if (depth_view != GRANIT_NULL_HANDLE)
    static_cast<void>(granit_texture_view_destroy(renderer.native_handle(), depth_view));
  if (depth_texture != GRANIT_NULL_HANDLE)
    static_cast<void>(granit_texture_destroy(renderer.native_handle(), depth_texture));
  if (output_view != GRANIT_NULL_HANDLE)
    static_cast<void>(granit_texture_view_destroy(renderer.native_handle(), output_view));
  if (output_texture != GRANIT_NULL_HANDLE)
    static_cast<void>(granit_texture_destroy(renderer.native_handle(), output_texture));
  if (hdr_view != GRANIT_NULL_HANDLE)
    static_cast<void>(granit_texture_view_destroy(renderer.native_handle(), hdr_view));
  if (hdr_texture != GRANIT_NULL_HANDLE)
    static_cast<void>(granit_texture_destroy(renderer.native_handle(), hdr_texture));
  if (granit::failed(result)) {
    std::cerr << "离屏 PBR 绘制失败：" << granit::result_message(result) << '\n';
    return 1;
  }
  if (options.benchmark) {
    const auto print_result = [&](std::string_view name, auto member) {
      std::vector<double> values;
      values.reserve(benchmark_samples.size());
      for (const auto& sample : benchmark_samples)
        values.push_back(sample.*member);
      const auto summary = summarize(std::move(values));
      std::cout << "1," << name << ',' << options.point_lights << ',' << options.iterations << ','
                << options.samples << ',' << summary.mean << ',' << summary.p50 << ','
                << summary.p95 << ',' << summary.p99 << '\n';
    };
    std::cout << std::fixed << std::setprecision(2) << "# revision=" << GRANIT_BENCHMARK_REVISION
              << ",compiler=" << GRANIT_BENCHMARK_COMPILER << ",system=" << GRANIT_BENCHMARK_SYSTEM
              << ",link=" << GRANIT_BENCHMARK_LINK_MODE << '\n'
              << "schema,name,lights,iterations,samples,mean_ns,p50_ns,p95_ns,p99_ns\n";
    print_result("shadow", &gpu_sample::shadow);
    print_result("pbr_hdr", &gpu_sample::pbr_hdr);
    print_result("tone_mapping", &gpu_sample::tone_mapping);
    print_result("render_chain", &gpu_sample::total);
    print_result("cpu_end_to_end", &gpu_sample::cpu_end_to_end);
    return 0;
  }
  std::cout << "PBR 多光源、阴影、IBL、HDR 与 Tone Mapping 像素回归完成\n"
            << "GPU 时间（ns）：Shadow=" << gpu_timestamps[1] - gpu_timestamps[0]
            << ", PBR HDR=" << gpu_timestamps[2] - gpu_timestamps[1]
            << ", Tone Mapping=" << gpu_timestamps[3] - gpu_timestamps[2]
            << ", 渲染链总计=" << gpu_timestamps[3] - gpu_timestamps[0] << '\n';
  return 0;
}
