// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "lighting/light_data.h"
#include "lighting/shadow_ibl_resources.h"
#include "lighting/tone_mapping_resources.h"
#include "material/material_template_gpu.h"
#include "material/pbr_material_schema.h"
#include "pbr_test_support.h"

#include <granit/granit.hpp>

#include <catch2/catch_all.hpp>

#include <algorithm>
#include <array>
#include <cstring>
#include <fstream>
#include <iterator>
#include <vector>

namespace {

bool environment_unavailable(granit::result value) {
  return value == granit::result::backend_unavailable ||
         value == granit::result::incompatible_driver ||
         value == granit::result::no_suitable_device;
}

granit::scene::view_input make_view(std::uint64_t layer_mask) {
  granit::scene::view_input view{};
  view.view = granit::math::identity_matrix4;
  view.projection = granit::math::identity_matrix4;
  view.view_projection = granit::math::identity_matrix4;
  view.layer_mask = layer_mask;
  return view;
}

std::vector<std::uint32_t> load_shader(std::string_view name) {
  const auto directory = name.starts_with("tone_mapping") ? GRANIT_PIPELINE_SHADER_DIR
                                                           : GRANIT_PBR_SHADER_DIR;
  std::ifstream stream{std::string{directory} + "/" + std::string{name}, std::ios::binary};
  const std::vector<char> bytes{std::istreambuf_iterator<char>{stream}, {}};
  if (bytes.empty() || bytes.size() % sizeof(std::uint32_t) != 0)
    return {};
  std::vector<std::uint32_t> words(bytes.size() / sizeof(std::uint32_t));
  std::memcpy(words.data(), bytes.data(), bytes.size());
  return words;
}

std::string load_shader_text(std::string_view name) {
  const auto directory = name.starts_with("tone_mapping") ? GRANIT_PIPELINE_SHADER_DIR
                                                           : GRANIT_PBR_SHADER_DIR;
  std::ifstream stream{std::string{directory} + "/" + std::string{name}, std::ios::binary};
  return {std::istreambuf_iterator<char>{stream}, {}};
}

} // namespace

TEST_CASE("两个View执行独立PBR与Tone Mapping") {
  const std::array views{make_view(1), make_view(2)};
  const std::array points{
      granit::scene::point_light_input{
          .position = {0, 0, 1.5F}, .intensity = {1, 0, 0}, .radius = 3, .layer_mask = 1},
      granit::scene::point_light_input{
          .position = {0, 0, 1.5F}, .intensity = {0, 1, 0}, .radius = 3, .layer_mask = 2}};
  granit::scene::multi_view_snapshot snapshot;
  REQUIRE(granit::scene::build_multi_view_snapshot({.views = views,
                                                    .renderables = {},
                                                    .directional_lights = {},
                                                    .point_lights = points,
                                                    .spot_lights = {}},
                                                   snapshot) ==
          granit::scene::multi_view_error::none);

  std::array<granit::lighting::packed_view_lights, 2> packed;
  for (std::size_t index = 0; index < packed.size(); ++index) {
    granit::lighting::light_requirements requirements;
    REQUIRE(granit::lighting::pack_view_lights(
                snapshot, index, {.directional = 1, .point = 2, .spot = 1}, packed[index],
                requirements) == granit::lighting::light_pack_error::none);
    REQUIRE(packed[index].point.size() == 1);
  }
  CHECK(packed[0].point[0].intensity[0] == 1.0F);
  CHECK(packed[1].point[0].intensity[1] == 1.0F);

  granit::renderer renderer;
  const auto initialized = renderer.initialize({.application_name = "granit-multi-view-gpu"});
  if (environment_unavailable(initialized))
    SKIP("当前运行环境没有满足要求的 Vulkan 设备");
  REQUIRE(initialized == granit::result::success);

  std::array<granit::lighting::shadow_ibl_resources, 2> lights;
  for (std::size_t index = 0; index < lights.size(); ++index) {
    REQUIRE(lights[index].initialize(renderer.native_handle(), {}, {}, {},
                                     {.directional = 1, .point = 2, .spot = 1},
                                     {.shadows = false, .ibl = false}) == GRANIT_SUCCESS);
    REQUIRE(lights[index].update_lights(packed[index]) == GRANIT_SUCCESS);
  }
  CHECK(lights[0].group() != lights[1].group());

  const auto vertex = load_shader("pbr_lights.vert.spv");
  const auto fragment = load_shader("pbr_lights_untextured.frag.spv");
  const auto vertex_wgsl = load_shader_text("pbr_lights.vert.wgsl");
  const auto fragment_wgsl = load_shader_text("pbr_lights_untextured.frag.wgsl");
  REQUIRE_FALSE(vertex.empty());
  REQUIRE_FALSE(fragment.empty());
  std::array<granit::material::material_package, 2> packages;
  std::array<granit::material::material_template_gpu, 2> materials;
  std::array<granit::material::material_gpu_instance, 2> instances;
  std::array<granit_graphics_pipeline, 2> pipelines{};
  granit::material::pbr_default_resources defaults;
  REQUIRE(defaults.initialize(renderer.native_handle()) == GRANIT_SUCCESS);
  granit::bind_group_layout object_layout;
  REQUIRE(object_layout.initialize(renderer.native_handle(), {}) == granit::result::success);
  for (std::size_t index = 0; index < materials.size(); ++index) {
    REQUIRE(granit::test::build_pbr_package(packages[index], vertex, vertex_wgsl, fragment,
                                                fragment_wgsl));
    const std::array layouts{object_layout.native_handle(), lights[index].layout()};
    REQUIRE(materials[index].initialize(renderer.native_handle(), packages[index], layouts) ==
            GRANIT_SUCCESS);
    const std::array features{granit::material::material_feature_value{
        granit::material::make_feature_id(granit::material::pbr_texture_feature_name), 0}};
    REQUIRE(
        materials[index].acquire_pipeline({.pass = granit::material::make_feature_id("opaque"),
                                           .variant = granit::material::make_variant_key(features),
                                           .color_format = GRANIT_TEXTURE_FORMAT_RGBA16_FLOAT,
                                           .depth_stencil_format = GRANIT_TEXTURE_FORMAT_D32_FLOAT},
                                          pipelines[index]) == GRANIT_SUCCESS);
    REQUIRE(granit::test::initialize_pbr_instance(renderer.native_handle(), materials[index],
                                                      packages[index], defaults,
                                                      instances[index]) == granit::result::success);
  }

  std::array<granit::texture, 2> colors;
  std::array<granit::texture_view, 2> color_views;
  std::array<granit::texture, 2> depths;
  std::array<granit::texture_view, 2> depth_views;
  std::array<granit::texture, 2> outputs;
  std::array<granit::texture_view, 2> output_views;
  std::array<granit::buffer, 2> readbacks;
  std::array<granit::lighting::tone_mapping_resources, 2> tone_mapping;
  constexpr std::uint64_t readback_size = 32 * 32 * 4;
  const auto tone_vertex = load_shader("tone_mapping.vert.spv");
  const auto tone_fragment = load_shader("tone_mapping.frag.spv");
  REQUIRE_FALSE(tone_vertex.empty());
  REQUIRE_FALSE(tone_fragment.empty());
  for (std::size_t index = 0; index < colors.size(); ++index) {
    REQUIRE(colors[index].initialize(renderer.native_handle(),
                                     {.format = granit::texture_format::rgba16_float,
                                      .usage = granit::texture_usage::color_attachment |
                                               granit::texture_usage::sampled |
                                               granit::texture_usage::transfer_source,
                                      .width = 32,
                                      .height = 32}) == granit::result::success);
    REQUIRE(
        color_views[index].initialize(renderer.native_handle(), colors[index].native_handle()) ==
        granit::result::success);
    REQUIRE(depths[index].initialize(renderer.native_handle(),
                                     {.format = granit::texture_format::d32_float,
                                      .usage = granit::texture_usage::depth_stencil_attachment,
                                      .width = 32,
                                      .height = 32}) == granit::result::success);
    REQUIRE(
        depth_views[index].initialize(renderer.native_handle(), depths[index].native_handle()) ==
        granit::result::success);
    REQUIRE(outputs[index].initialize(renderer.native_handle(),
                                      {.format = granit::texture_format::rgba8_unorm,
                                       .usage = granit::texture_usage::color_attachment |
                                                granit::texture_usage::transfer_source,
                                       .width = 32,
                                       .height = 32}) == granit::result::success);
    REQUIRE(output_views[index].initialize(renderer.native_handle(),
                                           outputs[index].native_handle()) ==
            granit::result::success);
    REQUIRE(readbacks[index].initialize(renderer.native_handle(),
                                        {.size = readback_size,
                                         .usage = granit::buffer_usage::transfer_destination,
                                         .location = granit::memory_location::readback}) ==
            granit::result::success);
    REQUIRE(tone_mapping[index].initialize(
                renderer.native_handle(), color_views[index].native_handle(),
                granit::texture_format::rgba8_unorm,
                {.exposure_scale = 1.0F, .encode_srgb = 1}, std::as_bytes(std::span{tone_vertex}),
                std::as_bytes(std::span{tone_fragment})) == GRANIT_SUCCESS);
    CHECK(tone_mapping[index].group() != GRANIT_NULL_HANDLE);
  }
  CHECK(tone_mapping[0].group() != tone_mapping[1].group());

  granit::command_recorder recorder;
  REQUIRE(recorder.initialize(renderer.native_handle()) == granit::result::success);
  REQUIRE(recorder.begin() == granit::result::success);
  const granit::viewport viewport{0, 0, 32, 32, 0, 1};
  const granit::scissor scissor{0, 0, 32, 32};
  for (std::size_t index = 0; index < colors.size(); ++index) {
    REQUIRE(recorder.bind_graphics_pipeline(pipelines[index]) == granit::result::success);
    const auto material_group = instances[index].bind_group();
    REQUIRE(recorder.bind_graphics_groups(materials[index].pipeline_layout(), 1,
                                          std::span{&material_group, 1}) ==
            granit::result::success);
    const auto light_group = lights[index].group();
    REQUIRE(recorder.bind_graphics_groups(materials[index].pipeline_layout(), 3,
                                          std::span{&light_group, 1}) == granit::result::success);
    REQUIRE(recorder.set_viewports(0, std::span{&viewport, 1}) == granit::result::success);
    REQUIRE(recorder.set_scissors(0, std::span{&scissor, 1}) == granit::result::success);
    const granit::color_attachment_desc color{.view = color_views[index].native_handle()};
    const granit::depth_stencil_attachment_desc depth{.view = depth_views[index].native_handle(),
                                                      .clear_value = {.depth = 1.0F}};
    const granit::rendering_desc rendering{.color_attachments = std::span{&color, 1},
                                           .depth_stencil_attachment = &depth,
                                           .area = {0, 0, 32, 32}};
    REQUIRE(recorder.begin_rendering(rendering) == granit::result::success);
    REQUIRE(recorder.draw(3) == granit::result::success);
    REQUIRE(recorder.end_rendering() == granit::result::success);
    REQUIRE(recorder.bind_graphics_pipeline(tone_mapping[index].pipeline()) ==
            granit::result::success);
    const auto tone_group = tone_mapping[index].group();
    REQUIRE(recorder.bind_graphics_groups(tone_mapping[index].pipeline_layout(), 0,
                                          std::span{&tone_group, 1}) ==
            granit::result::success);
    const granit::color_attachment_desc output{.view = output_views[index].native_handle()};
    const granit::rendering_desc tone_rendering{.color_attachments = std::span{&output, 1},
                                                .area = {0, 0, 32, 32}};
    REQUIRE(recorder.begin_rendering(tone_rendering) == granit::result::success);
    REQUIRE(recorder.draw(3) == granit::result::success);
    REQUIRE(recorder.end_rendering() == granit::result::success);
    const granit_texture_data_layout layout{};
    const granit_texture_write_region region{.mip_level = 0,
                                             .base_array_layer = 0,
                                             .array_layer_count = 1,
                                             .aspect = GRANIT_TEXTURE_ASPECT_COLOR_BIT,
                                             .x = 0,
                                             .y = 0,
                                             .z = 0,
                                             .width = 32,
                                             .height = 32,
                                             .depth = 1};
    REQUIRE(recorder.copy_texture_to_buffer(outputs[index].native_handle(),
                                            readbacks[index].native_handle(), layout, region) ==
            granit::result::success);
  }
  REQUIRE(recorder.end() == granit::result::success);
  REQUIRE(recorder.submit() == granit::result::success);
  REQUIRE(recorder.reset() == granit::result::success);

  std::array<std::array<std::uint8_t, 4>, 2> centers{};
  for (std::size_t index = 0; index < readbacks.size(); ++index) {
    void* mapped = nullptr;
    REQUIRE(readbacks[index].map(0, readback_size, &mapped) == granit::result::success);
    const auto* pixels = static_cast<const std::uint8_t*>(mapped);
    std::copy_n(pixels + (16 * 32 + 16) * 4, 4, centers[index].begin());
    REQUIRE(readbacks[index].unmap() == granit::result::success);
  }
  CHECK(centers[0][0] > centers[0][1]);
  CHECK(centers[1][1] > centers[1][0]);
}
