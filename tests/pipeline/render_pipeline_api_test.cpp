// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/pipeline/material.hpp>
#include <granit/pipeline/render_pipeline.hpp>
#include <granit/pipeline/scene.hpp>
#include <granit/renderer/buffer.hpp>
#include <granit/renderer/command_recorder.hpp>
#include <granit/renderer/renderer.hpp>
#include <granit/renderer/texture.hpp>

#include "material/material_package_archive.h"

#include <catch2/catch_all.hpp>

#include <array>
#include <cmath>
#include <cstddef>
#include <vector>

namespace {

bool environment_unavailable(granit::result value) {
  return value == granit::result::backend_unavailable ||
         value == granit::result::incompatible_driver ||
         value == granit::result::no_suitable_device;
}

granit_matrix4 identity() { return {{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}}; }

std::vector<std::byte> build_material_archive() {
  using namespace granit::material;
  material_package_desc desc;
  constexpr std::array spirv{UINT32_C(0x07230203), UINT32_C(0x00010600), 0U, 1U, 0U};
  desc.variants.push_back({.pass = make_feature_id("opaque"),
                           .features = {},
                           .shaders = {{.stage = package_shader_stage::vertex,
                                        .entry_point = "main",
                                        .spirv = {spirv.begin(), spirv.end()}},
                                       {.stage = package_shader_stage::fragment,
                                        .entry_point = "main",
                                        .spirv = {spirv.begin(), spirv.end()}}},
                           .pipeline = {}});
  material_package package;
  REQUIRE(material_package::build(std::move(desc), package) == package_error::none);
  std::vector<std::byte> archive;
  REQUIRE(encode_material_package_archive(package, archive) == archive_error::none);
  return archive;
}

struct callback_state {
  std::vector<granit_render_pipeline_stage> stages;
  std::vector<uint64_t> payloads;
  std::vector<uint64_t> meshes;
  std::vector<granit_material> materials;
  bool opaque_has_shadow = false;
  std::vector<granit_bind_group> ibl_groups;
  granit_renderer renderer = GRANIT_NULL_HANDLE;
  granit_result result = GRANIT_SUCCESS;
};

granit_result record(const granit_render_pipeline_record_info* info, void* user_data) {
  auto& state = *static_cast<callback_state*>(user_data);
  if (info == nullptr || info->struct_size < sizeof(granit_render_pipeline_record_info) ||
      info->recorder == GRANIT_NULL_HANDLE ||
      (info->stage == GRANIT_RENDER_PIPELINE_STAGE_OPAQUE &&
       info->color_output == GRANIT_NULL_HANDLE) ||
      (info->stage == GRANIT_RENDER_PIPELINE_STAGE_SHADOW &&
       info->depth_output == GRANIT_NULL_HANDLE)) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  if (info->view == nullptr ||
      (info->payload_count != 0 && (info->payloads == nullptr || info->draw_bindings == nullptr ||
                                    info->renderables == nullptr))) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  state.stages.push_back(info->stage);
  if (info->stage == GRANIT_RENDER_PIPELINE_STAGE_OPAQUE) {
    state.opaque_has_shadow = info->shadow_input != GRANIT_NULL_HANDLE;
    if (info->ibl_irradiance == GRANIT_NULL_HANDLE ||
        info->ibl_prefiltered_environment == GRANIT_NULL_HANDLE ||
        info->ibl_brdf_lut == GRANIT_NULL_HANDLE || info->ibl_layout == GRANIT_NULL_HANDLE ||
        info->ibl_group == GRANIT_NULL_HANDLE) {
      return GRANIT_ERROR_INVALID_ARGUMENT;
    }
    state.ibl_groups.push_back(info->ibl_group);
  }
  if (info->payload_count != 0) {
    state.payloads.insert(state.payloads.end(), info->payloads,
                          info->payloads + info->payload_count);
    for (uint32_t index = 0; index < info->payload_count; ++index) {
      state.meshes.push_back(info->draw_bindings[index].mesh);
      state.materials.push_back(info->draw_bindings[index].material);
    }
  }
  if (state.result != GRANIT_SUCCESS)
    return state.result;
  granit_depth_stencil_attachment_desc depth = GRANIT_DEPTH_STENCIL_ATTACHMENT_DESC_INIT;
  depth.view = info->depth_output;
  granit_rendering_desc rendering = GRANIT_RENDERING_DESC_INIT;
  rendering.depth_stencil_attachment = &depth;
  granit_color_attachment_desc color = GRANIT_COLOR_ATTACHMENT_DESC_INIT;
  if (info->stage == GRANIT_RENDER_PIPELINE_STAGE_OPAQUE) {
    color.view = info->color_output;
    color.clear_value = {0.25F, 0.5F, 1.0F, 1.0F};
    rendering.color_attachment_count = 1;
    rendering.color_attachments = &color;
    rendering.area = {0, 0, static_cast<std::uint32_t>(info->view->viewport_width),
                      static_cast<std::uint32_t>(info->view->viewport_height)};
  } else {
    rendering.area = {0, 0, 1024, 1024};
  }
  auto result = granit_command_recorder_begin_rendering(state.renderer, info->recorder, &rendering);
  if (result == GRANIT_SUCCESS)
    result = granit_command_recorder_end_rendering(state.renderer, info->recorder);
  return result;
}

} // namespace

TEST_CASE("统一Render Pipeline按固定阶段消费Scene Snapshot") {
  granit::renderer renderer;
  const auto initialized = renderer.initialize({.application_name = "granit-public-pipeline"});
  if (environment_unavailable(initialized))
    SKIP("当前运行环境没有满足要求的 Vulkan 设备");
  REQUIRE(initialized == granit::result::success);

  granit::texture output_texture;
  granit::texture_view output_view;
  REQUIRE(output_texture.initialize(renderer.native_handle(),
                                    {.format = granit::texture_format::rgba8_unorm,
                                     .usage = granit::texture_usage::color_attachment,
                                     .width = 16,
                                     .height = 16}) == granit::result::success);
  REQUIRE(output_view.initialize(renderer.native_handle(), output_texture.native_handle()) ==
          granit::result::success);

  std::array<granit_scene_view, 1> views{};
  views[0].view = identity();
  views[0].projection = identity();
  views[0].view_projection = identity();
  views[0].viewport_width = 16;
  views[0].viewport_height = 16;
  views[0].layer_mask = UINT64_MAX;
  std::array<granit_scene_renderable, 1> renderables{};
  renderables[0].model = identity();
  renderables[0].normal_matrix = identity();
  renderables[0].bounds_radius = 0.25F;
  renderables[0].layer_mask = UINT64_MAX;
  renderables[0].payload = 77;
  std::array<granit_scene_directional_light, 1> directional_lights{};
  directional_lights[0].direction_to_light = {0.0F, 0.0F, 1.0F};
  directional_lights[0].radiance = {1.0F, 1.0F, 1.0F};
  directional_lights[0].layer_mask = UINT64_MAX;
  granit_scene_snapshot_desc scene_desc = GRANIT_SCENE_SNAPSHOT_DESC_INIT;
  scene_desc.views = views.data();
  scene_desc.view_count = static_cast<std::uint32_t>(views.size());
  scene_desc.renderables = renderables.data();
  scene_desc.renderable_count = static_cast<std::uint32_t>(renderables.size());
  scene_desc.directional_lights = directional_lights.data();
  scene_desc.directional_light_count = static_cast<std::uint32_t>(directional_lights.size());
  granit::scene_snapshot scene;
  REQUIRE(scene.initialize(renderer.native_handle(), scene_desc) == granit::result::success);

  const auto archive = build_material_archive();
  granit_material_desc material_desc = GRANIT_MATERIAL_DESC_INIT;
  material_desc.archive_data = archive.data();
  material_desc.archive_size = archive.size();
  granit::material_instance material;
  REQUIRE(material.initialize(renderer.native_handle(), material_desc) == granit::result::success);

  callback_state callback;
  callback.renderer = renderer.native_handle();
  granit_render_pipeline_desc pipeline_desc = GRANIT_RENDER_PIPELINE_DESC_INIT;
  pipeline_desc.record = record;
  pipeline_desc.user_data = &callback;
  granit::render_pipeline pipeline;
  REQUIRE(pipeline.initialize(renderer.native_handle(), pipeline_desc) == granit::result::success);
  granit_render_pipeline_render_desc render_desc = GRANIT_RENDER_PIPELINE_RENDER_DESC_INIT;
  render_desc.scene = scene.native_handle();
  render_desc.output = output_view.native_handle();
  render_desc.output_format = GRANIT_TEXTURE_FORMAT_RGBA8_UNORM;
  render_desc.width = 16;
  render_desc.height = 16;
  CHECK(pipeline.render(render_desc) == granit::result::invalid_argument);
  const granit_render_pipeline_draw_binding draw_binding{77, 1001, material.native_handle(), 0};
  render_desc.draw_binding_count = 1;
  render_desc.draw_bindings = &draw_binding;
  REQUIRE(pipeline.render(render_desc) == granit::result::success);
  CHECK(callback.stages ==
        std::vector<granit_render_pipeline_stage>{GRANIT_RENDER_PIPELINE_STAGE_SHADOW,
                                                  GRANIT_RENDER_PIPELINE_STAGE_OPAQUE});
  CHECK(callback.payloads == std::vector<uint64_t>{77, 77});
  CHECK(callback.meshes == std::vector<uint64_t>{1001, 1001});
  CHECK(callback.materials ==
        std::vector<granit_material>{material.native_handle(), material.native_handle()});
  CHECK(callback.opaque_has_shadow);
  REQUIRE(callback.ibl_groups.size() == 1);

  REQUIRE(pipeline.render(render_desc) == granit::result::success);
  CHECK(callback.stages.size() == 4);
  CHECK(callback.payloads == std::vector<uint64_t>{77, 77, 77, 77});
  CHECK(callback.meshes == std::vector<uint64_t>{1001, 1001, 1001, 1001});
  REQUIRE(callback.ibl_groups.size() == 2);
  CHECK(callback.ibl_groups[0] == callback.ibl_groups[1]);

  const std::array duplicate_bindings{draw_binding, draw_binding};
  render_desc.draw_binding_count = static_cast<uint32_t>(duplicate_bindings.size());
  render_desc.draw_bindings = duplicate_bindings.data();
  CHECK(pipeline.render(render_desc) == granit::result::invalid_argument);
  render_desc.draw_binding_count = 1;
  render_desc.draw_bindings = &draw_binding;

  callback.result = GRANIT_ERROR_NOT_READY;
  CHECK(pipeline.render(render_desc) == granit::result::not_ready);
}

TEST_CASE("统一Render Pipeline拒绝跨Renderer与越界View") {
  granit::renderer first;
  granit::renderer second;
  const auto first_result = first.initialize({.application_name = "granit-pipeline-first"});
  const auto second_result = second.initialize({.application_name = "granit-pipeline-second"});
  if (environment_unavailable(first_result) || environment_unavailable(second_result))
    SKIP("当前运行环境没有满足要求的 Vulkan 设备");
  REQUIRE(first_result == granit::result::success);
  REQUIRE(second_result == granit::result::success);
  callback_state callback;
  granit_render_pipeline_desc desc = GRANIT_RENDER_PIPELINE_DESC_INIT;
  desc.record = record;
  desc.user_data = &callback;
  granit_render_pipeline pipeline = GRANIT_NULL_HANDLE;
  REQUIRE(granit_render_pipeline_create(first.native_handle(), &desc, &pipeline) == GRANIT_SUCCESS);
  CHECK(granit_render_pipeline_destroy(second.native_handle(), pipeline) ==
        GRANIT_ERROR_INVALID_HANDLE);
  REQUIRE(granit_render_pipeline_destroy(first.native_handle(), pipeline) == GRANIT_SUCCESS);
  CHECK(granit_render_pipeline_destroy(first.native_handle(), pipeline) ==
        GRANIT_ERROR_INVALID_HANDLE);
}

TEST_CASE("公共Render Pipeline ABI输出可回读的Tone Mapping像素") {
  granit::renderer renderer;
  const auto initialized = renderer.initialize({.application_name = "granit-pipeline-pixel-abi"});
  if (environment_unavailable(initialized))
    SKIP("当前运行环境没有满足要求的 Vulkan 设备");
  REQUIRE(initialized == granit::result::success);

  constexpr uint32_t size = 8;
  granit_texture output_texture = GRANIT_NULL_HANDLE;
  granit_texture_view output_view = GRANIT_NULL_HANDLE;
  granit_texture_desc output_desc = GRANIT_TEXTURE_DESC_INIT;
  output_desc.format = GRANIT_TEXTURE_FORMAT_RGBA8_UNORM;
  output_desc.usage =
      GRANIT_TEXTURE_USAGE_COLOR_ATTACHMENT_BIT | GRANIT_TEXTURE_USAGE_TRANSFER_SOURCE_BIT;
  output_desc.width = size;
  output_desc.height = size;
  REQUIRE(granit_texture_create_with_default_view(renderer.native_handle(), &output_desc,
                                                  &output_texture, &output_view) == GRANIT_SUCCESS);

  granit_scene_view view{};
  view.view = identity();
  view.projection = identity();
  view.view_projection = identity();
  view.viewport_width = size;
  view.viewport_height = size;
  view.layer_mask = UINT64_MAX;
  granit_scene_renderable renderable{};
  renderable.model = identity();
  renderable.normal_matrix = identity();
  renderable.bounds_radius = 0.25F;
  renderable.layer_mask = UINT64_MAX;
  renderable.payload = 91;
  granit_scene_snapshot_desc scene_desc = GRANIT_SCENE_SNAPSHOT_DESC_INIT;
  scene_desc.views = &view;
  scene_desc.view_count = 1;
  scene_desc.renderables = &renderable;
  scene_desc.renderable_count = 1;
  granit_scene_snapshot scene = GRANIT_NULL_HANDLE;
  REQUIRE(granit_scene_snapshot_create(renderer.native_handle(), &scene_desc, &scene) ==
          GRANIT_SUCCESS);

  const auto archive = build_material_archive();
  granit_material_desc material_desc = GRANIT_MATERIAL_DESC_INIT;
  material_desc.archive_data = archive.data();
  material_desc.archive_size = archive.size();
  granit_material material = GRANIT_NULL_HANDLE;
  REQUIRE(granit_material_create(renderer.native_handle(), &material_desc, &material) ==
          GRANIT_SUCCESS);

  callback_state callback;
  callback.renderer = renderer.native_handle();
  granit_render_pipeline_desc pipeline_desc = GRANIT_RENDER_PIPELINE_DESC_INIT;
  pipeline_desc.record = record;
  pipeline_desc.user_data = &callback;
  granit_render_pipeline pipeline = GRANIT_NULL_HANDLE;
  REQUIRE(granit_render_pipeline_create(renderer.native_handle(), &pipeline_desc, &pipeline) ==
          GRANIT_SUCCESS);
  const granit_render_pipeline_draw_binding binding{91, 2001, material, 0};
  granit_render_pipeline_render_desc render_desc = GRANIT_RENDER_PIPELINE_RENDER_DESC_INIT;
  render_desc.scene = scene;
  render_desc.output = output_view;
  render_desc.output_format = GRANIT_TEXTURE_FORMAT_RGBA8_UNORM;
  render_desc.width = size;
  render_desc.height = size;
  render_desc.draw_binding_count = 1;
  render_desc.draw_bindings = &binding;
  REQUIRE(granit_render_pipeline_render(renderer.native_handle(), pipeline, &render_desc) ==
          GRANIT_SUCCESS);

  granit::buffer readback;
  REQUIRE(readback.initialize(renderer.native_handle(),
                              {.size = size * size * 4,
                               .usage = granit::buffer_usage::transfer_destination,
                               .location = granit::memory_location::readback}) ==
          granit::result::success);
  granit::command_recorder recorder;
  REQUIRE(recorder.initialize(renderer.native_handle()) == granit::result::success);
  REQUIRE(recorder.begin() == granit::result::success);
  const granit_texture_data_layout layout{};
  const granit_texture_write_region region{.mip_level = 0,
                                           .base_array_layer = 0,
                                           .array_layer_count = 1,
                                           .aspect = GRANIT_TEXTURE_ASPECT_COLOR_BIT,
                                           .x = 0,
                                           .y = 0,
                                           .z = 0,
                                           .width = size,
                                           .height = size,
                                           .depth = 1};
  REQUIRE(recorder.copy_texture_to_buffer(output_texture, readback.native_handle(), layout,
                                          region) == granit::result::success);
  REQUIRE(recorder.end() == granit::result::success);
  REQUIRE(recorder.submit() == granit::result::success);
  REQUIRE(recorder.reset() == granit::result::success);

  const auto aces_srgb = [](float value) {
    const auto mapped = std::clamp(
        value * (2.51F * value + 0.03F) / (value * (2.43F * value + 0.59F) + 0.14F), 0.0F, 1.0F);
    return mapped <= 0.0031308F ? 12.92F * mapped : 1.055F * std::pow(mapped, 1.0F / 2.4F) - 0.055F;
  };
  const std::array expected{aces_srgb(0.25F), aces_srgb(0.5F), aces_srgb(1.0F)};
  void* mapped = nullptr;
  REQUIRE(readback.map(0, size * size * 4, &mapped) == granit::result::success);
  const auto* pixel = static_cast<const uint8_t*>(mapped) + (size / 2 * size + size / 2) * 4;
  const auto quantize = [](float value) {
    return static_cast<uint8_t>(std::lround(value * 255.0F));
  };
  CHECK(pixel[0] == Catch::Approx(quantize(expected[0])).margin(1));
  CHECK(pixel[1] == Catch::Approx(quantize(expected[1])).margin(1));
  CHECK(pixel[2] == Catch::Approx(quantize(expected[2])).margin(1));
  CHECK(pixel[3] == 255);
  REQUIRE(readback.unmap() == granit::result::success);

  REQUIRE(granit_render_pipeline_destroy(renderer.native_handle(), pipeline) == GRANIT_SUCCESS);
  REQUIRE(granit_material_destroy(renderer.native_handle(), material) == GRANIT_SUCCESS);
  REQUIRE(granit_scene_snapshot_destroy(renderer.native_handle(), scene) == GRANIT_SUCCESS);
  REQUIRE(granit_texture_view_destroy(renderer.native_handle(), output_view) == GRANIT_SUCCESS);
  REQUIRE(granit_texture_destroy(renderer.native_handle(), output_texture) == GRANIT_SUCCESS);
}
