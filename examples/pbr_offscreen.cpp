// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "lighting/ibl_reference.h"
#include "lighting/shadow_ibl_resources.h"
#include "lighting/tone_mapping_reference.h"
#include "lighting/tone_mapping_resources.h"
#include "material/material_gpu_instance.h"
#include "material/material_package.h"
#include "material/material_template_gpu.h"
#include "material/pbr_default_resources.h"
#include "material/pbr_material_schema.h"
#include "material/pbr_reference.h"

#include <granit/granit.hpp>

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

namespace {

std::vector<std::uint32_t> load_shader(std::string_view name) {
  const auto path = std::string{GRANIT_EXAMPLE_ASSET_DIR} + "/" + std::string{name};
  std::ifstream stream{path, std::ios::binary};
  const std::vector<char> bytes{std::istreambuf_iterator<char>{stream}, {}};
  if (bytes.empty() || bytes.size() % sizeof(std::uint32_t) != 0)
    return {};
  std::vector<std::uint32_t> words(bytes.size() / sizeof(std::uint32_t));
  std::memcpy(words.data(), bytes.data(), bytes.size());
  return words;
}

bool make_package(granit::material::material_package& package) {
  using namespace granit::material;
  material_variant_desc variant{
      .pass = make_feature_id("opaque"),
      .features = {{make_feature_id(pbr_texture_feature_name), pbr_texture_all}},
      .shaders = {{.stage = package_shader_stage::vertex,
                   .entry_point = "vertex_main",
                   .spirv = load_shader("pbr_shadow_ibl_lights.vert.spv")},
                  {.stage = package_shader_stage::fragment,
                   .entry_point = "fragment_main",
                   .spirv = load_shader("pbr_shadow_ibl_lights_untextured.frag.spv")}},
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
      {.name = "emissive", .type = parameter_type::float3, .offset = 32, .default_value = {}}};
  desc.metadata.parameters.insert(desc.metadata.parameters.end(),
                                  {{.name = "base_color_texture",
                                    .type = parameter_type::texture_view,
                                    .binding = pbr_binding_base_color,
                                    .default_value = {}},
                                   {.name = "metallic_roughness_texture",
                                    .type = parameter_type::texture_view,
                                    .binding = pbr_binding_metallic_roughness,
                                    .default_value = {}},
                                   {.name = "normal_texture",
                                    .type = parameter_type::texture_view,
                                    .binding = pbr_binding_normal,
                                    .default_value = {}},
                                   {.name = "occlusion_texture",
                                    .type = parameter_type::texture_view,
                                    .binding = pbr_binding_occlusion,
                                    .default_value = {}},
                                   {.name = "emissive_texture",
                                    .type = parameter_type::texture_view,
                                    .binding = pbr_binding_emissive,
                                    .default_value = {}},
                                   {.name = "pbr_sampler",
                                    .type = parameter_type::sampler,
                                    .binding = pbr_binding_sampler,
                                    .default_value = {}}});
  auto untextured = variant;
  untextured.features.front().value = 0;
  untextured.shaders.back().spirv = load_shader("pbr_shadow_ibl_lights_untextured.frag.spv");
  desc.variants.push_back(std::move(untextured));
  desc.variants.push_back(std::move(variant));
  return material_package::build(std::move(desc), package) == package_error::none;
}

template <std::size_t Size>
bool set_parameter(granit::material::material_gpu_instance& instance, std::string_view name,
                   granit::material::parameter_type type, const std::array<float, Size>& value) {
  const auto bytes = std::bit_cast<std::array<std::byte, sizeof(value)>>(value);
  return instance.set(granit::material::make_parameter_id(name), type, bytes) ==
         granit::material::metadata_error::none;
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

int main() {
  granit::renderer renderer;
  auto result =
      renderer.initialize({.application_name = "Granit Textured PBR", .enable_validation = true});
  granit::material::material_package package;
  if (granit::failed(result) || !make_package(package)) {
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
  granit::lighting::shadow_ibl_resources lighting_resources;
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
        {.intensity = 0.25F, .prefiltered_max_mip = 0.0F}));
  }
  if (granit::succeeded(result)) {
    granit::lighting::packed_view_lights lights;
    lights.directional.push_back(
        {.direction_to_light = {0.0F, 0.0F, 1.0F}, .radiance = {1.0F, 1.0F, 1.0F}});
    result = granit::from_native(lighting_resources.update_lights(lights));
  }
  granit::bind_group_layout object_layout;
  if (granit::succeeded(result))
    result = object_layout.initialize(renderer.native_handle(), {});

  granit::material::material_template_gpu material;
  if (granit::succeeded(result)) {
    const std::array additional_layouts{object_layout.native_handle(), lighting_resources.layout()};
    result = granit::from_native(
        material.initialize(renderer.native_handle(), package, additional_layouts));
  }
  granit_graphics_pipeline pipeline = GRANIT_NULL_HANDLE;
  if (granit::succeeded(result)) {
    const std::array features{granit::material::material_feature_value{
        granit::material::make_feature_id(granit::material::pbr_texture_feature_name),
        granit::material::pbr_texture_all}};
    result = granit::from_native(
        material.acquire_pipeline({.pass = granit::material::make_feature_id("opaque"),
                                   .variant = granit::material::make_variant_key(features),
                                   .color_format = GRANIT_TEXTURE_FORMAT_RGBA16_FLOAT,
                                   .depth_stencil_format = GRANIT_TEXTURE_FORMAT_D32_FLOAT},
                                  pipeline));
  }

  granit::material::pbr_default_resources defaults;
  granit::material::material_gpu_instance instance;
  if (granit::succeeded(result))
    result = granit::from_native(defaults.initialize(renderer.native_handle()));
  if (granit::succeeded(result)) {
    result = granit::from_native(instance.initialize(
        renderer.native_handle(), material.material_layout(), package.metadata()));
  }
  if (granit::succeeded(result))
    result = granit::from_native(defaults.bind(instance));
  if (granit::succeeded(result) &&
      (!set_parameter(instance, "base_color", granit::material::parameter_type::float4,
                      std::array{0.8F, 0.2F, 0.1F, 1.0F}) ||
       !set_parameter(instance, "metallic", granit::material::parameter_type::float32,
                      std::array{0.5F}) ||
       !set_parameter(instance, "perceptual_roughness", granit::material::parameter_type::float32,
                      std::array{0.5F}) ||
       !set_parameter(instance, "normal_scale", granit::material::parameter_type::float32,
                      std::array{1.0F}) ||
       !set_parameter(instance, "occlusion_strength", granit::material::parameter_type::float32,
                      std::array{1.0F}) ||
       !set_parameter(instance, "emissive", granit::material::parameter_type::float3,
                      std::array{0.0F, 0.0F, 0.0F}))) {
    result = granit::result::invalid_argument;
  }
  if (granit::succeeded(result))
    result = granit::from_native(instance.flush());

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
  const auto tone_vertex = load_shader("tone_mapping.vert.spv");
  const auto tone_fragment = load_shader("tone_mapping.frag.spv");
  granit::lighting::tone_mapping_resources tone_mapping;
  if (granit::succeeded(result)) {
    result = granit::from_native(tone_mapping.initialize(
        renderer.native_handle(), hdr_view, granit::texture_format::rgba8_unorm,
        {.exposure_scale = 1.0F, .encode_srgb = 1}, std::as_bytes(std::span{tone_vertex}),
        std::as_bytes(std::span{tone_fragment})));
  }
  const auto material_group = instance.bind_group();
  const auto lighting_group = lighting_resources.group();
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
  const auto render_case = [&](float stored_shadow_depth, bool expected_lit) {
    auto case_result = recorder.begin();
    const granit::depth_stencil_attachment_desc shadow_depth{
        .view = shadow_view.native_handle(), .clear_value = {.depth = stored_shadow_depth}};
    const granit::rendering_desc shadow_rendering{
        .color_attachments = {}, .depth_stencil_attachment = &shadow_depth, .area = {0, 0, 1, 1}};
    if (granit::succeeded(case_result))
      case_result = recorder.begin_rendering(shadow_rendering);
    if (granit::succeeded(case_result))
      case_result = recorder.end_rendering();
    if (granit::succeeded(case_result))
      case_result = recorder.bind_graphics_pipeline(pipeline);
    if (granit::succeeded(case_result)) {
      case_result = recorder.bind_graphics_groups(material.pipeline_layout(), 1,
                                                  std::span{&material_group, 1});
    }
    if (granit::succeeded(case_result)) {
      case_result = recorder.bind_graphics_groups(material.pipeline_layout(), 3,
                                                  std::span{&lighting_group, 1});
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
      case_result = recorder.copy_texture_to_buffer(output_texture, readback.native_handle(),
                                                    readback_layout, readback_region);
    }
    if (granit::succeeded(case_result))
      case_result = recorder.end();
    if (granit::succeeded(case_result))
      case_result = recorder.submit();
    if (granit::succeeded(case_result))
      case_result = recorder.reset();
    if (granit::failed(case_result))
      return case_result;

    void* mapped = nullptr;
    case_result = readback.map(0, readback_size, &mapped);
    if (granit::succeeded(case_result)) {
      const auto* pixels = static_cast<const std::uint8_t*>(mapped);
      const auto expected_linear =
          expected_lit ? granit::math::add(reference, ibl_reference) : ibl_reference;
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
        std::cerr << "PBR 阴影像素回归失败：中心像素=" << static_cast<unsigned>(center[0]) << ','
                  << static_cast<unsigned>(center[1]) << ',' << static_cast<unsigned>(center[2])
                  << ',' << static_cast<unsigned>(center[3]) << '\n';
        case_result = granit::result::internal;
      }
      const auto unmap_result = readback.unmap();
      if (granit::succeeded(case_result))
        case_result = unmap_result;
    }
    return case_result;
  };

  if (granit::succeeded(result))
    result = render_case(0.25F, false);
  if (granit::succeeded(result))
    result = render_case(1.0F, true);

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
  std::cout << "PBR 阴影、IBL、HDR 与 Tone Mapping 像素回归完成\n";
  return 0;
}
