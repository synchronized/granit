// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "pbr_example_support.h"

#include "material/pbr_material_schema.h"

#include <array>
#include <bit>

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
