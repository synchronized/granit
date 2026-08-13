// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/pipeline/material.hpp>
#include <granit/pipeline/render_pipeline.hpp>
#include <granit/pipeline/scene.hpp>
#include <granit/renderer/renderer.hpp>
#include <granit/renderer/texture.hpp>

#include "material/material_package_archive.h"

#include <catch2/catch_all.hpp>

#include <array>
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
  granit_renderer renderer = GRANIT_NULL_HANDLE;
  granit_result result = GRANIT_SUCCESS;
};

granit_result record(const granit_render_pipeline_record_info* info, void* user_data) {
  auto& state = *static_cast<callback_state*>(user_data);
  if (info == nullptr || info->struct_size < sizeof(granit_render_pipeline_record_info) ||
      info->recorder == GRANIT_NULL_HANDLE || info->color_output == GRANIT_NULL_HANDLE) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  if (info->view == nullptr ||
      (info->payload_count != 0 && (info->payloads == nullptr || info->draw_bindings == nullptr ||
                                    info->renderables == nullptr))) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  state.stages.push_back(info->stage);
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
  granit_color_attachment_desc color = GRANIT_COLOR_ATTACHMENT_DESC_INIT;
  color.view = info->color_output;
  color.clear_value = {0.25F, 0.5F, 1.0F, 1.0F};
  granit_depth_stencil_attachment_desc depth = GRANIT_DEPTH_STENCIL_ATTACHMENT_DESC_INIT;
  depth.view = info->depth_output;
  granit_rendering_desc rendering = GRANIT_RENDERING_DESC_INIT;
  rendering.color_attachment_count = 1;
  rendering.color_attachments = &color;
  rendering.depth_stencil_attachment = &depth;
  rendering.area = {0, 0, static_cast<std::uint32_t>(info->view->viewport_width),
                    static_cast<std::uint32_t>(info->view->viewport_height)};
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
  granit_scene_snapshot_desc scene_desc = GRANIT_SCENE_SNAPSHOT_DESC_INIT;
  scene_desc.views = views.data();
  scene_desc.view_count = static_cast<std::uint32_t>(views.size());
  scene_desc.renderables = renderables.data();
  scene_desc.renderable_count = static_cast<std::uint32_t>(renderables.size());
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
        std::vector<granit_render_pipeline_stage>{GRANIT_RENDER_PIPELINE_STAGE_OPAQUE});
  CHECK(callback.payloads == std::vector<uint64_t>{77});
  CHECK(callback.meshes == std::vector<uint64_t>{1001});
  CHECK(callback.materials == std::vector<granit_material>{material.native_handle()});

  REQUIRE(pipeline.render(render_desc) == granit::result::success);
  CHECK(callback.stages.size() == 2);
  CHECK(callback.payloads == std::vector<uint64_t>{77, 77});
  CHECK(callback.meshes == std::vector<uint64_t>{1001, 1001});

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
