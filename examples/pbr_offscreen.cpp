// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

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
                   .spirv = load_shader("pbr_untextured.vert.spv")},
                  {.stage = package_shader_stage::fragment,
                   .entry_point = "fragment_main",
                   .spirv = load_shader("pbr_textured.frag.spv")}},
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
  untextured.shaders.back().spirv = load_shader("pbr_untextured.frag.spv");
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

  granit::material::material_template_gpu material;
  result = granit::from_native(material.initialize(renderer.native_handle(), package));
  granit_graphics_pipeline pipeline = GRANIT_NULL_HANDLE;
  if (granit::succeeded(result)) {
    const std::array features{granit::material::material_feature_value{
        granit::material::make_feature_id(granit::material::pbr_texture_feature_name),
        granit::material::pbr_texture_all}};
    result = granit::from_native(
        material.acquire_pipeline({.pass = granit::material::make_feature_id("opaque"),
                                   .variant = granit::material::make_variant_key(features),
                                   .color_format = GRANIT_TEXTURE_FORMAT_RGBA8_UNORM,
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

  granit_texture color_texture = GRANIT_NULL_HANDLE;
  granit_texture_view color_view = GRANIT_NULL_HANDLE;
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
    result = create_attachment(GRANIT_TEXTURE_FORMAT_RGBA8_UNORM,
                               GRANIT_TEXTURE_USAGE_COLOR_ATTACHMENT_BIT |
                                   GRANIT_TEXTURE_USAGE_TRANSFER_SOURCE_BIT,
                               color_texture, color_view);
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
  if (granit::succeeded(result))
    result = recorder.begin();
  if (granit::succeeded(result))
    result = recorder.bind_graphics_pipeline(pipeline);
  const auto material_group = instance.bind_group();
  if (granit::succeeded(result))
    result =
        recorder.bind_graphics_groups(material.pipeline_layout(), 1, std::span{&material_group, 1});
  const granit::viewport viewport{0, 0, 256, 256, 0, 1};
  const granit::scissor scissor{0, 0, 256, 256};
  if (granit::succeeded(result))
    result = recorder.set_viewports(0, std::span{&viewport, 1});
  if (granit::succeeded(result))
    result = recorder.set_scissors(0, std::span{&scissor, 1});
  const granit::color_attachment_desc color{
      .view = color_view,
      .clear_value = {.red = 0.03F, .green = 0.03F, .blue = 0.05F, .alpha = 1.0F}};
  const granit::depth_stencil_attachment_desc depth{.view = depth_view};
  const granit::rendering_desc rendering{.color_attachments = std::span{&color, 1},
                                         .depth_stencil_attachment = &depth,
                                         .area = {0, 0, 256, 256}};
  if (granit::succeeded(result))
    result = recorder.begin_rendering(rendering);
  if (granit::succeeded(result))
    result = recorder.draw(3);
  if (granit::succeeded(result))
    result = recorder.end_rendering();
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
  if (granit::succeeded(result)) {
    result = recorder.copy_texture_to_buffer(color_texture, readback.native_handle(),
                                             readback_layout, readback_region);
  }
  if (granit::succeeded(result))
    result = recorder.end();
  if (granit::succeeded(result))
    result = recorder.submit();
  if (granit::succeeded(result))
    result = recorder.reset();

  if (granit::succeeded(result)) {
    void* mapped = nullptr;
    result = readback.map(0, readback_size, &mapped);
    if (granit::succeeded(result)) {
      const auto* pixels = static_cast<const std::uint8_t*>(mapped);
      const auto reference = granit::material::evaluate_pbr_direct_light(
          {.base_color = {0.8F, 0.2F, 0.1F}, .metallic = 0.5F, .perceptual_roughness = 0.5F},
          {.normal = {1.0F / 255.0F, 1.0F / 255.0F, 1.0F}});
      const std::array expected_center{quantize_unorm(reference.x), quantize_unorm(reference.y),
                                       quantize_unorm(reference.z), std::uint8_t{255}};
      constexpr std::array<std::uint8_t, 4> expected_clear{8, 8, 13, 255};
      const auto* center = pixels + (128 * render_size + 128) * 4;
      const auto* corner = pixels;
      if (!near_pixel(center, expected_center, 2) || !near_pixel(corner, expected_clear, 1)) {
        std::cerr << "PBR 像素回归失败：中心像素=" << static_cast<unsigned>(center[0]) << ','
                  << static_cast<unsigned>(center[1]) << ',' << static_cast<unsigned>(center[2])
                  << ',' << static_cast<unsigned>(center[3]) << '\n';
        result = granit::result::internal;
      }
      const auto unmap_result = readback.unmap();
      if (granit::succeeded(result))
        result = unmap_result;
    }
  }

  if (depth_view != GRANIT_NULL_HANDLE)
    static_cast<void>(granit_texture_view_destroy(renderer.native_handle(), depth_view));
  if (depth_texture != GRANIT_NULL_HANDLE)
    static_cast<void>(granit_texture_destroy(renderer.native_handle(), depth_texture));
  if (color_view != GRANIT_NULL_HANDLE)
    static_cast<void>(granit_texture_view_destroy(renderer.native_handle(), color_view));
  if (color_texture != GRANIT_NULL_HANDLE)
    static_cast<void>(granit_texture_destroy(renderer.native_handle(), color_texture));
  if (granit::failed(result)) {
    std::cerr << "离屏 PBR 绘制失败：" << granit::result_message(result) << '\n';
    return 1;
  }
  std::cout << "默认纹理 PBR 离屏绘制及像素回归完成\n";
  return 0;
}
