// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/pipeline/material.hpp>
#include <granit/pipeline/mesh.hpp>
#include <granit/pipeline/render_pipeline.hpp>
#include <granit/pipeline/scene.hpp>
#include <granit/renderer/buffer.hpp>
#include <granit/renderer/command_recorder.hpp>
#include <granit/renderer/renderer.hpp>
#include <granit/renderer/texture.hpp>

#include "lighting/tone_mapping_resources.h"
#include "material/material_package_archive.h"

#include <catch2/catch_all.hpp>

#include <array>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
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

std::vector<std::uint32_t> load_pipeline_shader(const char* name) {
  std::ifstream stream{std::string{GRANIT_PIPELINE_ASSET_DIR} + "/" + name, std::ios::binary};
  const std::vector<char> bytes{std::istreambuf_iterator<char>{stream}, {}};
  if (bytes.empty() || bytes.size() % sizeof(std::uint32_t) != 0)
    return {};
  std::vector<std::uint32_t> words(bytes.size() / sizeof(std::uint32_t));
  std::memcpy(words.data(), bytes.data(), bytes.size());
  return words;
}

std::vector<std::uint32_t> load_tone_mapping_shader(const char* name) {
  std::ifstream stream{std::string{GRANIT_PIPELINE_SHADER_DIR} + "/" + name, std::ios::binary};
  const std::vector<char> bytes{std::istreambuf_iterator<char>{stream}, {}};
  if (bytes.empty() || bytes.size() % sizeof(std::uint32_t) != 0)
    return {};
  std::vector<std::uint32_t> words(bytes.size() / sizeof(std::uint32_t));
  std::memcpy(words.data(), bytes.data(), bytes.size());
  return words;
}

std::vector<std::byte> build_automatic_material_archive() {
  using namespace granit::material;
  const auto vertex = load_pipeline_shader("pbr_shadow_ibl_lights.vert.spv");
  const auto fragment = load_pipeline_shader("pbr_shadow_ibl_lights_untextured.frag.spv");
  REQUIRE_FALSE(vertex.empty());
  REQUIRE_FALSE(fragment.empty());
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
  material_variant_desc variant{.pass = make_feature_id("opaque"),
                                .features = {{make_feature_id("pbr_texture_mask"), 0}},
                                .shaders = {{.stage = package_shader_stage::vertex,
                                             .entry_point = "vertex_main",
                                             .spirv = vertex},
                                            {.stage = package_shader_stage::fragment,
                                             .entry_point = "fragment_main",
                                             .spirv = fragment}},
                                .pipeline = {}};
  variant.pipeline.primitive.front_face = GRANIT_FRONT_FACE_CLOCKWISE;
  variant.pipeline.primitive.cull_mode = GRANIT_CULL_MODE_BACK;
  variant.pipeline.depth.test_enabled = 1;
  variant.pipeline.depth.write_enabled = 1;
  variant.pipeline.depth.compare = GRANIT_COMPARE_OPERATION_LESS_EQUAL;
  desc.variants.push_back(std::move(variant));
  material_package package;
  REQUIRE(material_package::build(std::move(desc), package) == package_error::none);
  std::vector<std::byte> archive;
  REQUIRE(encode_material_package_archive(package, archive) == archive_error::none);
  return archive;
}

struct callback_state {
  std::vector<granit_render_pipeline_stage> stages;
  std::vector<uint64_t> payloads;
  std::vector<granit_mesh> meshes;
  std::vector<granit_material> materials;
  std::vector<uint32_t> view_indices;
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
  state.view_indices.push_back(info->view_index);
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
    color.clear_value = info->view_index == 0 ? granit_clear_color_value{0.25F, 0.5F, 1.0F, 1.0F}
                                              : granit_clear_color_value{1.0F, 0.2F, 0.1F, 1.0F};
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

void initialize_test_mesh(granit_renderer renderer, granit::buffer& vertex_buffer,
                          granit::mesh& mesh) {
  REQUIRE(vertex_buffer.initialize(renderer, {.size = 36,
                                              .usage = granit::buffer_usage::vertex,
                                              .location = granit::memory_location::device}) ==
          granit::result::success);
  const granit_vertex_attribute attribute{0, GRANIT_VERTEX_FORMAT_FLOAT32X3, 0, 0};
  const granit_mesh_vertex_buffer vertex{
      vertex_buffer.native_handle(), 0, {12, GRANIT_VERTEX_STEP_MODE_VERTEX, 1, 0, &attribute}};
  granit_mesh_desc desc = GRANIT_MESH_DESC_INIT;
  desc.vertex_buffers = &vertex;
  desc.vertex_buffer_count = 1;
  desc.vertex_count = 3;
  REQUIRE(mesh.initialize(renderer, desc) == granit::result::success);
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
  granit::texture second_output_texture;
  granit::texture_view second_output_view;
  REQUIRE(output_texture.initialize(renderer.native_handle(),
                                    {.format = granit::texture_format::rgba8_unorm,
                                     .usage = granit::texture_usage::color_attachment |
                                              granit::texture_usage::transfer_source,
                                     .width = 16,
                                     .height = 16}) == granit::result::success);
  REQUIRE(output_view.initialize(renderer.native_handle(), output_texture.native_handle()) ==
          granit::result::success);
  REQUIRE(second_output_texture.initialize(renderer.native_handle(),
                                           {.format = granit::texture_format::rgba8_unorm,
                                            .usage = granit::texture_usage::color_attachment |
                                                     granit::texture_usage::transfer_source,
                                            .width = 16,
                                            .height = 16}) == granit::result::success);
  REQUIRE(second_output_view.initialize(renderer.native_handle(),
                                        second_output_texture.native_handle()) ==
          granit::result::success);

  std::array<granit_scene_view, 2> views{};
  views[0].view = identity();
  views[0].projection = identity();
  views[0].view_projection = identity();
  views[0].viewport_width = 16;
  views[0].viewport_height = 16;
  views[0].layer_mask = UINT64_MAX;
  views[1] = views[0];
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

  granit::buffer vertex_buffer;
  granit::mesh mesh;
  initialize_test_mesh(renderer.native_handle(), vertex_buffer, mesh);

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
  const granit_render_pipeline_draw_binding draw_binding{77, mesh.native_handle(),
                                                         material.native_handle(), 0};
  render_desc.draw_binding_count = 1;
  render_desc.draw_bindings = &draw_binding;
  REQUIRE(pipeline.render(render_desc) == granit::result::success);
  render_desc.frame = 1;
  render_desc.view_count = 2;
  CHECK(pipeline.render(render_desc) == granit::result::invalid_argument);
  render_desc.frame = GRANIT_NULL_HANDLE;
  render_desc.view_count = 1;
  CHECK(callback.stages ==
        std::vector<granit_render_pipeline_stage>{GRANIT_RENDER_PIPELINE_STAGE_SHADOW,
                                                  GRANIT_RENDER_PIPELINE_STAGE_OPAQUE});
  CHECK(callback.payloads == std::vector<uint64_t>{77, 77});
  CHECK(callback.meshes == std::vector<granit_mesh>{mesh.native_handle(), mesh.native_handle()});
  CHECK(callback.materials ==
        std::vector<granit_material>{material.native_handle(), material.native_handle()});
  CHECK(callback.opaque_has_shadow);
  REQUIRE(callback.ibl_groups.size() == 1);

  REQUIRE(pipeline.render(render_desc) == granit::result::success);
  CHECK(callback.stages.size() == 4);
  CHECK(callback.payloads == std::vector<uint64_t>{77, 77, 77, 77});
  CHECK(callback.meshes == std::vector<granit_mesh>{mesh.native_handle(), mesh.native_handle(),
                                                    mesh.native_handle(), mesh.native_handle()});
  REQUIRE(callback.ibl_groups.size() == 2);
  CHECK(callback.ibl_groups[0] == callback.ibl_groups[1]);

  const std::array duplicate_bindings{draw_binding, draw_binding};
  render_desc.draw_binding_count = static_cast<uint32_t>(duplicate_bindings.size());
  render_desc.draw_bindings = duplicate_bindings.data();
  CHECK(pipeline.render(render_desc) == granit::result::invalid_argument);
  render_desc.draw_binding_count = 1;
  render_desc.draw_bindings = &draw_binding;

  const std::array multi_view_outputs{
      granit_render_pipeline_output{sizeof(granit_render_pipeline_output), 0,
                                    output_view.native_handle(), GRANIT_TEXTURE_FORMAT_RGBA8_UNORM,
                                    16, 16, 0},
      granit_render_pipeline_output{sizeof(granit_render_pipeline_output), 0,
                                    second_output_view.native_handle(),
                                    GRANIT_TEXTURE_FORMAT_RGBA8_UNORM, 16, 16, 0}};
  render_desc.view_count = 2;
  render_desc.output_count = static_cast<uint32_t>(multi_view_outputs.size());
  render_desc.outputs = multi_view_outputs.data();
  REQUIRE(pipeline.render(render_desc) == granit::result::success);
  REQUIRE(callback.stages.size() == 8);
  CHECK(std::vector(callback.stages.end() - 4, callback.stages.end()) ==
        std::vector<granit_render_pipeline_stage>{
            GRANIT_RENDER_PIPELINE_STAGE_SHADOW, GRANIT_RENDER_PIPELINE_STAGE_OPAQUE,
            GRANIT_RENDER_PIPELINE_STAGE_SHADOW, GRANIT_RENDER_PIPELINE_STAGE_OPAQUE});
  CHECK(std::vector(callback.view_indices.end() - 4, callback.view_indices.end()) ==
        std::vector<uint32_t>{0, 0, 1, 1});
  granit::buffer multi_view_readback;
  REQUIRE(multi_view_readback.initialize(renderer.native_handle(),
                                         {.size = 16 * 16 * 4 * 2,
                                          .usage = granit::buffer_usage::transfer_destination,
                                          .location = granit::memory_location::readback}) ==
          granit::result::success);
  granit::command_recorder multi_view_recorder;
  REQUIRE(multi_view_recorder.initialize(renderer.native_handle()) == granit::result::success);
  REQUIRE(multi_view_recorder.begin() == granit::result::success);
  const granit_texture_write_region multi_view_region{.mip_level = 0,
                                                      .base_array_layer = 0,
                                                      .array_layer_count = 1,
                                                      .aspect = GRANIT_TEXTURE_ASPECT_COLOR_BIT,
                                                      .x = 0,
                                                      .y = 0,
                                                      .z = 0,
                                                      .width = 16,
                                                      .height = 16,
                                                      .depth = 1};
  const granit_texture_data_layout first_layout{};
  const granit_texture_data_layout second_layout{
      .offset = 16 * 16 * 4, .bytes_per_row = 0, .rows_per_image = 0};
  REQUIRE(multi_view_recorder.copy_texture_to_buffer(
              output_texture.native_handle(), multi_view_readback.native_handle(), first_layout,
              multi_view_region) == granit::result::success);
  REQUIRE(multi_view_recorder.copy_texture_to_buffer(
              second_output_texture.native_handle(), multi_view_readback.native_handle(),
              second_layout, multi_view_region) == granit::result::success);
  REQUIRE(multi_view_recorder.end() == granit::result::success);
  REQUIRE(multi_view_recorder.submit() == granit::result::success);
  REQUIRE(multi_view_recorder.reset() == granit::result::success);
  void* multi_view_pixels = nullptr;
  REQUIRE(multi_view_readback.map(0, 16 * 16 * 4 * 2, &multi_view_pixels) ==
          granit::result::success);
  const auto* first_pixel = static_cast<const uint8_t*>(multi_view_pixels) + (8 * 16 + 8) * 4;
  const auto* second_pixel =
      static_cast<const uint8_t*>(multi_view_pixels) + 16 * 16 * 4 + (8 * 16 + 8) * 4;
  CHECK(first_pixel[2] > first_pixel[0]);
  CHECK(second_pixel[0] > second_pixel[2]);
  CHECK(first_pixel[0] != second_pixel[0]);
  REQUIRE(multi_view_readback.unmap() == granit::result::success);
  render_desc.view_count = 1;
  render_desc.output_count = 0;
  render_desc.outputs = nullptr;

  auto invalid_mesh_binding = draw_binding;
  invalid_mesh_binding.mesh = 1001;
  render_desc.draw_bindings = &invalid_mesh_binding;
  CHECK(pipeline.render(render_desc) == granit::result::invalid_handle);
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

  granit_render_pipeline_desc automatic_desc = GRANIT_RENDER_PIPELINE_DESC_INIT;
  REQUIRE(granit_render_pipeline_create(first.native_handle(), &automatic_desc, &pipeline) ==
          GRANIT_SUCCESS);
  REQUIRE(granit_render_pipeline_destroy(first.native_handle(), pipeline) == GRANIT_SUCCESS);
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
  const granit_scene_directional_light directional_light{.direction_to_light = {0.0F, 0.0F, 1.0F},
                                                         .radiance = {0.0F, 0.0F, 0.0F},
                                                         .layer_mask = UINT64_MAX};
  scene_desc.directional_lights = &directional_light;
  scene_desc.directional_light_count = 1;
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

  granit_buffer vertex_buffer = GRANIT_NULL_HANDLE;
  granit_buffer_desc vertex_buffer_desc = GRANIT_BUFFER_DESC_INIT;
  vertex_buffer_desc.size = 36;
  vertex_buffer_desc.usage = GRANIT_BUFFER_USAGE_VERTEX_BIT;
  vertex_buffer_desc.memory_location = GRANIT_MEMORY_LOCATION_DEVICE;
  REQUIRE(granit_buffer_create(renderer.native_handle(), &vertex_buffer_desc, &vertex_buffer) ==
          GRANIT_SUCCESS);
  constexpr std::array<float, 9> positions{-0.65F, -0.65F, 0.5F,  0.65F, -0.65F,
                                           0.5F,   0.0F,   0.65F, 0.5F};
  REQUIRE(granit_buffer_write(renderer.native_handle(), vertex_buffer, 0, positions.data(),
                              sizeof(positions)) == GRANIT_SUCCESS);
  const granit_vertex_attribute attribute{0, GRANIT_VERTEX_FORMAT_FLOAT32X3, 0, 0};
  const granit_mesh_vertex_buffer vertex{
      vertex_buffer, 0, {12, GRANIT_VERTEX_STEP_MODE_VERTEX, 1, 0, &attribute}};
  granit_mesh_desc mesh_desc = GRANIT_MESH_DESC_INIT;
  mesh_desc.vertex_buffers = &vertex;
  mesh_desc.vertex_buffer_count = 1;
  mesh_desc.vertex_count = 3;
  granit_mesh mesh = GRANIT_NULL_HANDLE;
  REQUIRE(granit_mesh_create(renderer.native_handle(), &mesh_desc, &mesh) == GRANIT_SUCCESS);

  callback_state callback;
  callback.renderer = renderer.native_handle();
  granit_render_pipeline_desc pipeline_desc = GRANIT_RENDER_PIPELINE_DESC_INIT;
  pipeline_desc.record = record;
  pipeline_desc.user_data = &callback;
  granit_render_pipeline pipeline = GRANIT_NULL_HANDLE;
  REQUIRE(granit_render_pipeline_create(renderer.native_handle(), &pipeline_desc, &pipeline) ==
          GRANIT_SUCCESS);
  const granit_render_pipeline_draw_binding binding{91, mesh, material, 0};
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

  const auto automatic_archive = build_automatic_material_archive();
  const std::array<float, 4> base_color{0.0F, 0.0F, 0.0F, 1.0F};
  const std::array<float, 3> emissive{0.25F, 0.5F, 1.0F};
  const std::array automatic_updates{
      granit_material_parameter_update{granit_material_parameter_id("base_color", 10),
                                       GRANIT_MATERIAL_PARAMETER_FLOAT4, 0, base_color.data(),
                                       sizeof(base_color), GRANIT_NULL_HANDLE},
      granit_material_parameter_update{granit_material_parameter_id("emissive", 8),
                                       GRANIT_MATERIAL_PARAMETER_FLOAT3, 0, emissive.data(),
                                       sizeof(emissive), GRANIT_NULL_HANDLE}};
  material_desc.archive_data = automatic_archive.data();
  material_desc.archive_size = automatic_archive.size();
  material_desc.initial_updates = automatic_updates.data();
  material_desc.initial_update_count = static_cast<uint32_t>(automatic_updates.size());
  REQUIRE(granit_material_create(renderer.native_handle(), &material_desc, &material) ==
          GRANIT_SUCCESS);
  granit_render_pipeline_desc automatic_pipeline_desc = GRANIT_RENDER_PIPELINE_DESC_INIT;
  REQUIRE(granit_render_pipeline_create(renderer.native_handle(), &automatic_pipeline_desc,
                                        &pipeline) == GRANIT_SUCCESS);
  granit_render_pipeline_draw_binding automatic_binding{91, mesh, material, 0};
  render_desc.draw_bindings = &automatic_binding;
  REQUIRE(granit_render_pipeline_render(renderer.native_handle(), pipeline, &render_desc) ==
          GRANIT_SUCCESS);

  REQUIRE(recorder.begin() == granit::result::success);
  REQUIRE(recorder.copy_texture_to_buffer(output_texture, readback.native_handle(), layout,
                                          region) == granit::result::success);
  REQUIRE(recorder.end() == granit::result::success);
  REQUIRE(recorder.submit() == granit::result::success);
  REQUIRE(recorder.reset() == granit::result::success);
  REQUIRE(readback.map(0, size * size * 4, &mapped) == granit::result::success);
  pixel = static_cast<const uint8_t*>(mapped) + (size / 2 * size + size / 2) * 4;
  const std::array<uint8_t, 4> automatic_pixel{pixel[0], pixel[1], pixel[2], pixel[3]};
  REQUIRE(readback.unmap() == granit::result::success);

  // 用 H-05 资源手工执行相同的 HDR -> Tone Mapping，验证统一门面没有改变输出。
  granit::texture manual_hdr;
  granit::texture_view manual_hdr_view;
  REQUIRE(manual_hdr.initialize(renderer.native_handle(),
                                {.format = granit::texture_format::rgba16_float,
                                 .usage = granit::texture_usage::sampled |
                                          granit::texture_usage::transfer_destination}) ==
          granit::result::success);
  constexpr std::array<uint16_t, 4> manual_hdr_pixel{0x3400, 0x3800, 0x3c00, 0x3c00};
  REQUIRE(manual_hdr.write({reinterpret_cast<const std::byte*>(manual_hdr_pixel.data()),
                            sizeof(manual_hdr_pixel)},
                           {.bytes_per_row = 8}, {}) == granit::result::success);
  REQUIRE(manual_hdr_view.initialize(renderer.native_handle(), manual_hdr.native_handle()) ==
          granit::result::success);
  const auto tone_vertex = load_tone_mapping_shader("tone_mapping.vert.spv");
  const auto tone_fragment = load_tone_mapping_shader("tone_mapping.frag.spv");
  REQUIRE_FALSE(tone_vertex.empty());
  REQUIRE_FALSE(tone_fragment.empty());
  granit::lighting::tone_mapping_resources manual_tone_mapping;
  REQUIRE(manual_tone_mapping.initialize(
              renderer.native_handle(), manual_hdr_view.native_handle(),
              granit::texture_format::rgba8_unorm, {.exposure_scale = 1.0F, .encode_srgb = 1},
              std::as_bytes(std::span{tone_vertex}),
              std::as_bytes(std::span{tone_fragment})) == GRANIT_SUCCESS);
  granit::texture manual_output;
  granit::texture_view manual_output_view;
  REQUIRE(manual_output.initialize(renderer.native_handle(),
                                   {.format = granit::texture_format::rgba8_unorm,
                                    .usage = granit::texture_usage::color_attachment |
                                             granit::texture_usage::transfer_source,
                                    .width = size,
                                    .height = size}) == granit::result::success);
  REQUIRE(manual_output_view.initialize(renderer.native_handle(), manual_output.native_handle()) ==
          granit::result::success);
  granit::buffer manual_readback;
  REQUIRE(manual_readback.initialize(renderer.native_handle(),
                                     {.size = size * size * 4,
                                      .usage = granit::buffer_usage::transfer_destination,
                                      .location = granit::memory_location::readback}) ==
          granit::result::success);
  granit::command_recorder manual_recorder;
  REQUIRE(manual_recorder.initialize(renderer.native_handle()) == granit::result::success);
  REQUIRE(manual_recorder.begin() == granit::result::success);
  REQUIRE(manual_recorder.bind_graphics_pipeline(manual_tone_mapping.pipeline()) ==
          granit::result::success);
  const auto manual_group = manual_tone_mapping.group();
  REQUIRE(manual_recorder.bind_graphics_groups(manual_tone_mapping.pipeline_layout(), 0,
                                               std::span{&manual_group, 1}) ==
          granit::result::success);
  const granit::viewport manual_viewport{0, 0, size, size, 0, 1};
  const granit::scissor manual_scissor{0, 0, size, size};
  REQUIRE(manual_recorder.set_viewports(0, std::span{&manual_viewport, 1}) ==
          granit::result::success);
  REQUIRE(manual_recorder.set_scissors(0, std::span{&manual_scissor, 1}) ==
          granit::result::success);
  const granit::color_attachment_desc manual_color{.view = manual_output_view.native_handle()};
  const granit::rendering_desc manual_rendering{.color_attachments = std::span{&manual_color, 1},
                                                .area = {0, 0, size, size}};
  REQUIRE(manual_recorder.begin_rendering(manual_rendering) == granit::result::success);
  REQUIRE(manual_recorder.draw(3) == granit::result::success);
  REQUIRE(manual_recorder.end_rendering() == granit::result::success);
  REQUIRE(manual_recorder.copy_texture_to_buffer(manual_output.native_handle(),
                                                 manual_readback.native_handle(), layout,
                                                 region) == granit::result::success);
  REQUIRE(manual_recorder.end() == granit::result::success);
  REQUIRE(manual_recorder.submit() == granit::result::success);
  REQUIRE(manual_recorder.reset() == granit::result::success);
  REQUIRE(manual_readback.map(0, size * size * 4, &mapped) == granit::result::success);
  pixel = static_cast<const uint8_t*>(mapped) + (size / 2 * size + size / 2) * 4;
  for (size_t channel = 0; channel < automatic_pixel.size(); ++channel) {
    CHECK(automatic_pixel[channel] == Catch::Approx(pixel[channel]).margin(1));
  }
  REQUIRE(manual_readback.unmap() == granit::result::success);

  REQUIRE(granit_render_pipeline_destroy(renderer.native_handle(), pipeline) == GRANIT_SUCCESS);
  constexpr std::uint32_t pipeline_lifecycle_iterations = 8;
  for (std::uint32_t iteration = 0; iteration < pipeline_lifecycle_iterations; ++iteration) {
    pipeline = GRANIT_NULL_HANDLE;
    REQUIRE(granit_render_pipeline_create(renderer.native_handle(), &automatic_pipeline_desc,
                                          &pipeline) == GRANIT_SUCCESS);
    REQUIRE(granit_render_pipeline_render(renderer.native_handle(), pipeline, &render_desc) ==
            GRANIT_SUCCESS);
    REQUIRE(granit_render_pipeline_destroy(renderer.native_handle(), pipeline) == GRANIT_SUCCESS);
  }
  REQUIRE(granit_render_pipeline_create(renderer.native_handle(), &automatic_pipeline_desc,
                                        &pipeline) == GRANIT_SUCCESS);
  REQUIRE(granit_mesh_destroy(renderer.native_handle(), mesh) == GRANIT_SUCCESS);
  CHECK(granit_render_pipeline_render(renderer.native_handle(), pipeline, &render_desc) ==
        GRANIT_ERROR_INVALID_HANDLE);
  mesh = GRANIT_NULL_HANDLE;
  REQUIRE(granit_mesh_create(renderer.native_handle(), &mesh_desc, &mesh) == GRANIT_SUCCESS);
  automatic_binding.mesh = mesh;
  REQUIRE(granit_material_destroy(renderer.native_handle(), material) == GRANIT_SUCCESS);
  CHECK(granit_render_pipeline_render(renderer.native_handle(), pipeline, &render_desc) ==
        GRANIT_ERROR_INVALID_HANDLE);
  REQUIRE(granit_render_pipeline_destroy(renderer.native_handle(), pipeline) == GRANIT_SUCCESS);
  REQUIRE(granit_mesh_destroy(renderer.native_handle(), mesh) == GRANIT_SUCCESS);
  REQUIRE(granit_buffer_destroy(renderer.native_handle(), vertex_buffer) == GRANIT_SUCCESS);
  REQUIRE(granit_scene_snapshot_destroy(renderer.native_handle(), scene) == GRANIT_SUCCESS);
  REQUIRE(granit_texture_view_destroy(renderer.native_handle(), output_view) == GRANIT_SUCCESS);
  REQUIRE(granit_texture_destroy(renderer.native_handle(), output_texture) == GRANIT_SUCCESS);
}
