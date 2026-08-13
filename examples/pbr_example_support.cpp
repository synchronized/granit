// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "pbr_example_support.h"

#include "material/pbr_material_schema.h"

#include <array>
#include <bit>
#include <cmath>

namespace granit::examples {
namespace {

template <std::size_t Size>
bool set_parameter(material::material_gpu_instance& instance, std::string_view name,
                   material::parameter_type type, const std::array<float, Size>& value) {
  const auto bytes = std::bit_cast<std::array<std::byte, sizeof(value)>>(value);
  return instance.set(material::make_parameter_id(name), type, bytes) ==
         material::metadata_error::none;
}

} // namespace

result pbr_lighting_resources::initialize(granit_renderer renderer) {
  auto value = shadow_texture_.initialize(
      renderer, {.format = texture_format::d32_float,
                 .usage = texture_usage::depth_stencil_attachment | texture_usage::sampled,
                 .width = 1,
                 .height = 1});
  if (succeeded(value))
    value = shadow_view_.initialize(renderer, shadow_texture_.native_handle());
  const auto cube_desc =
      texture_desc{.dimension = texture_dimension::cube,
                   .format = texture_format::rgba16_float,
                   .usage = texture_usage::sampled | texture_usage::transfer_destination,
                   .width = 1,
                   .height = 1,
                   .array_layers = 6};
  if (succeeded(value))
    value = irradiance_texture_.initialize(renderer, cube_desc);
  if (succeeded(value))
    value = prefiltered_texture_.initialize(renderer, cube_desc);
  if (succeeded(value)) {
    value = brdf_lut_texture_.initialize(
        renderer, {.format = texture_format::rgba16_float,
                   .usage = texture_usage::sampled | texture_usage::transfer_destination});
  }
  constexpr std::array<std::uint16_t, 24> cube_pixels{
      0x3c00, 0x3c00, 0x3c00, 0x3c00, 0x3c00, 0x3c00, 0x3c00, 0x3c00,
      0x3c00, 0x3c00, 0x3c00, 0x3c00, 0x3c00, 0x3c00, 0x3c00, 0x3c00,
      0x3c00, 0x3c00, 0x3c00, 0x3c00, 0x3c00, 0x3c00, 0x3c00, 0x3c00};
  constexpr std::array<std::uint16_t, 4> lut_pixel{0x3800, 0x2e66, 0x0000, 0x3c00};
  const auto cube_bytes = std::as_bytes(std::span{cube_pixels});
  if (succeeded(value)) {
    value = irradiance_texture_.write(cube_bytes, {.bytes_per_row = 8, .rows_per_image = 1},
                                      {.array_layer_count = 6});
  }
  if (succeeded(value)) {
    value = prefiltered_texture_.write(cube_bytes, {.bytes_per_row = 8, .rows_per_image = 1},
                                       {.array_layer_count = 6});
  }
  if (succeeded(value))
    value = brdf_lut_texture_.write(std::as_bytes(std::span{lut_pixel}), {.bytes_per_row = 8}, {});
  const texture_view_desc cube_view_desc{.dimension = texture_dimension::cube,
                                         .array_layer_count = 6};
  if (succeeded(value)) {
    value =
        irradiance_view_.initialize(renderer, irradiance_texture_.native_handle(), cube_view_desc);
  }
  if (succeeded(value)) {
    value = prefiltered_view_.initialize(renderer, prefiltered_texture_.native_handle(),
                                         cube_view_desc);
  }
  if (succeeded(value))
    value = brdf_lut_view_.initialize(renderer, brdf_lut_texture_.native_handle());
  if (succeeded(value)) {
    value = from_native(resources_.initialize(
        renderer,
        {.shadow = shadow_view_.native_handle(),
         .ibl = {.irradiance = irradiance_view_.native_handle(),
                 .prefiltered_environment = prefiltered_view_.native_handle(),
                 .brdf_lut = brdf_lut_view_.native_handle()}},
        {.light_view_projection = math::identity_matrix4, .texel_size = {1.0F, 1.0F}},
        {.intensity = 0.25F}, {.directional = 1, .point = 1, .spot = 1}));
  }
  if (succeeded(value)) {
    lighting::packed_view_lights lights;
    lights.directional.push_back(
        {.direction_to_light = {0.0F, 0.0F, 1.0F}, .radiance = {1.0F, 1.0F, 1.0F}});
    lights.point.push_back(
        {.position = {0.0F, 0.0F, 1.5F}, .radius = 3.0F, .intensity = {0.3F, 0.2F, 0.1F}});
    lights.spot.push_back({.position = {0.0F, 0.0F, 1.5F},
                           .radius = 3.0F,
                           .direction = {0.0F, 0.0F, -1.0F},
                           .outer_angle_cosine = std::cos(0.6F),
                           .intensity = {0.1F, 0.2F, 0.3F},
                           .inner_angle_cosine = std::cos(0.2F)});
    value = from_native(resources_.update_lights(lights));
  }
  if (failed(value))
    static_cast<void>(reset());
  return value;
}

result pbr_lighting_resources::reset() {
  auto value = from_native(resources_.reset());
  const auto capture = [&](result next) {
    if (succeeded(value))
      value = next;
  };
  capture(brdf_lut_view_.reset());
  capture(prefiltered_view_.reset());
  capture(irradiance_view_.reset());
  capture(brdf_lut_texture_.reset());
  capture(prefiltered_texture_.reset());
  capture(irradiance_texture_.reset());
  capture(shadow_view_.reset());
  capture(shadow_texture_.reset());
  return value;
}

bool build_pbr_package(material::material_package& package,
                       std::span<const std::uint32_t> vertex_shader,
                       std::span<const std::uint32_t> fragment_shader) {
  using namespace material;
  material_variant_desc variant{
      .pass = make_feature_id("opaque"),
      .features = {{make_feature_id(pbr_texture_feature_name), 0}},
      .shaders = {{.stage = package_shader_stage::vertex,
                   .entry_point = "vertex_main",
                   .spirv = {vertex_shader.begin(), vertex_shader.end()}},
                  {.stage = package_shader_stage::fragment,
                   .entry_point = "fragment_main",
                   .spirv = {fragment_shader.begin(), fragment_shader.end()}}},
      .pipeline = {}};
  variant.pipeline.primitive.cull_mode = GRANIT_CULL_MODE_BACK;
  variant.pipeline.primitive.front_face = GRANIT_FRONT_FACE_CLOCKWISE;
  variant.pipeline.depth.test_enabled = 1;
  variant.pipeline.depth.write_enabled = 1;
  variant.pipeline.depth.compare = GRANIT_COMPARE_OPERATION_LESS_EQUAL;

  material_package_desc desc;
  desc.metadata.constant_buffer_size = 48;
  desc.metadata.parameters = {
      {.name = "base_color", .type = parameter_type::float4, .offset = 0, .default_value = {}},
      {.name = "metallic", .type = parameter_type::float32, .offset = 16, .default_value = {}},
      {.name = "perceptual_roughness",
       .type = parameter_type::float32,
       .offset = 20,
       .default_value = {}},
      {.name = "normal_scale", .type = parameter_type::float32, .offset = 24, .default_value = {}},
      {.name = "occlusion_strength",
       .type = parameter_type::float32,
       .offset = 28,
       .default_value = {}},
      {.name = "emissive", .type = parameter_type::float3, .offset = 32, .default_value = {}},
      {.name = "base_color_texture",
       .type = parameter_type::texture_view,
       .binding = 1,
       .default_value = {}},
      {.name = "metallic_roughness_texture",
       .type = parameter_type::texture_view,
       .binding = 2,
       .default_value = {}},
      {.name = "normal_texture",
       .type = parameter_type::texture_view,
       .binding = 3,
       .default_value = {}},
      {.name = "occlusion_texture",
       .type = parameter_type::texture_view,
       .binding = 4,
       .default_value = {}},
      {.name = "emissive_texture",
       .type = parameter_type::texture_view,
       .binding = 5,
       .default_value = {}},
      {.name = "pbr_sampler", .type = parameter_type::sampler, .binding = 6, .default_value = {}}};
  desc.variants.push_back(std::move(variant));
  return material_package::build(std::move(desc), package) == package_error::none;
}

result initialize_pbr_instance(granit_renderer renderer,
                               material::material_template_gpu& material_template,
                               const material::material_package& package,
                               material::pbr_default_resources& defaults,
                               material::material_gpu_instance& instance) {
  auto value = from_native(
      instance.initialize(renderer, material_template.material_layout(), package.metadata()));
  if (succeeded(value))
    value = from_native(defaults.bind(instance));
  if (succeeded(value) &&
      (!set_parameter(instance, "base_color", material::parameter_type::float4,
                      std::array{0.8F, 0.2F, 0.1F, 1.0F}) ||
       !set_parameter(instance, "metallic", material::parameter_type::float32, std::array{0.5F}) ||
       !set_parameter(instance, "perceptual_roughness", material::parameter_type::float32,
                      std::array{0.5F}) ||
       !set_parameter(instance, "normal_scale", material::parameter_type::float32,
                      std::array{1.0F}) ||
       !set_parameter(instance, "occlusion_strength", material::parameter_type::float32,
                      std::array{1.0F}) ||
       !set_parameter(instance, "emissive", material::parameter_type::float3,
                      std::array{0.0F, 0.0F, 0.0F}))) {
    value = result::invalid_argument;
  }
  if (succeeded(value))
    value = from_native(instance.flush());
  return value;
}

} // namespace granit::examples
