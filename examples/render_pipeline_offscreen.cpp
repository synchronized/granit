// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/granit.hpp>
#include <granit/pipeline/render_pipeline.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <iterator>
#include <vector>

namespace {

granit_matrix4 identity() {
  granit_matrix4 value{};
  value.elements[0] = 1.0F;
  value.elements[5] = 1.0F;
  value.elements[10] = 1.0F;
  value.elements[15] = 1.0F;
  return value;
}

std::vector<char> load_package() {
  std::ifstream stream{GRANIT_RENDER_PIPELINE_EXAMPLE_PACKAGE, std::ios::binary};
  return {std::istreambuf_iterator<char>{stream}, {}};
}

bool check(granit_result result, const char* operation) {
  if (result == GRANIT_SUCCESS)
    return true;
  std::cerr << operation << "失败，结果码：" << result << '\n';
  return false;
}

} // namespace

int main() {
  granit::renderer renderer;
  if (granit::failed(renderer.initialize({.application_name = "Granit Render Pipeline"}))) {
    std::cerr << "当前环境无法创建 Vulkan Renderer\n";
    return 1;
  }
  const auto native_renderer = renderer.native_handle();
  constexpr std::uint32_t size = 64;

  granit_texture output = GRANIT_NULL_HANDLE;
  granit_texture_view output_view = GRANIT_NULL_HANDLE;
  granit_texture_desc output_desc = GRANIT_TEXTURE_DESC_INIT;
  output_desc.format = GRANIT_TEXTURE_FORMAT_RGBA8_UNORM;
  output_desc.usage =
      GRANIT_TEXTURE_USAGE_COLOR_ATTACHMENT_BIT | GRANIT_TEXTURE_USAGE_TRANSFER_SOURCE_BIT;
  output_desc.width = size;
  output_desc.height = size;
  if (!check(granit_texture_create_with_default_view(native_renderer, &output_desc, &output,
                                                     &output_view),
             "创建输出纹理")) {
    return 1;
  }

  constexpr std::array<float, 9> positions{-0.65F, -0.65F, 0.5F,  0.65F, -0.65F,
                                           0.5F,   0.0F,   0.65F, 0.5F};
  granit::buffer vertex_buffer;
  if (granit::failed(vertex_buffer.initialize(native_renderer,
                                              {.size = sizeof(positions),
                                               .usage = granit::buffer_usage::vertex,
                                               .location = granit::memory_location::device},
                                              std::as_bytes(std::span{positions})))) {
    std::cerr << "创建顶点缓冲失败\n";
    return 1;
  }
  const granit_vertex_attribute attribute{0, GRANIT_VERTEX_FORMAT_FLOAT32X3, 0, 0};
  const granit_mesh_vertex_buffer vertex{
      vertex_buffer.native_handle(), 0, {12, GRANIT_VERTEX_STEP_MODE_VERTEX, 1, 0, &attribute}};
  granit_mesh_desc mesh_desc = GRANIT_MESH_DESC_INIT;
  mesh_desc.vertex_buffers = &vertex;
  mesh_desc.vertex_buffer_count = 1;
  mesh_desc.vertex_count = 3;
  granit_mesh mesh = GRANIT_NULL_HANDLE;
  if (!check(granit_mesh_create(native_renderer, &mesh_desc, &mesh), "创建 Mesh"))
    return 1;

  const auto archive = load_package();
  const std::array<float, 4> base_color{0.8F, 0.2F, 0.1F, 1.0F};
  const granit_material_parameter_update update{granit_material_parameter_id("base_color", 10),
                                                GRANIT_MATERIAL_PARAMETER_FLOAT4,
                                                0,
                                                base_color.data(),
                                                sizeof(base_color),
                                                GRANIT_NULL_HANDLE};
  granit_material_desc material_desc = GRANIT_MATERIAL_DESC_INIT;
  material_desc.archive_data = archive.data();
  material_desc.archive_size = archive.size();
  material_desc.initial_updates = &update;
  material_desc.initial_update_count = 1;
  granit_material material = GRANIT_NULL_HANDLE;
  if (archive.empty() ||
      !check(granit_material_create(native_renderer, &material_desc, &material), "创建 Material")) {
    return 1;
  }

  granit_scene_view view{};
  view.view = identity();
  view.projection = identity();
  view.view_projection = identity();
  view.viewport_width = static_cast<float>(size);
  view.viewport_height = static_cast<float>(size);
  view.layer_mask = UINT64_MAX;
  granit_scene_renderable renderable{};
  renderable.model = identity();
  renderable.normal_matrix = identity();
  renderable.bounds_radius = 1.0F;
  renderable.layer_mask = UINT64_MAX;
  renderable.payload = 1;
  const granit_scene_directional_light light{.direction_to_light = {0.0F, 0.0F, 1.0F},
                                             .radiance = {1.0F, 1.0F, 1.0F},
                                             .layer_mask = UINT64_MAX};
  granit_scene_snapshot_desc scene_desc = GRANIT_SCENE_SNAPSHOT_DESC_INIT;
  scene_desc.views = &view;
  scene_desc.view_count = 1;
  scene_desc.renderables = &renderable;
  scene_desc.renderable_count = 1;
  scene_desc.directional_lights = &light;
  scene_desc.directional_light_count = 1;
  granit_scene_snapshot scene = GRANIT_NULL_HANDLE;
  if (!check(granit_scene_snapshot_create(native_renderer, &scene_desc, &scene), "创建 Scene"))
    return 1;

  granit_render_pipeline pipeline = GRANIT_NULL_HANDLE;
  const granit_render_pipeline_desc pipeline_desc = GRANIT_RENDER_PIPELINE_DESC_INIT;
  if (!check(granit_render_pipeline_create(native_renderer, &pipeline_desc, &pipeline),
             "创建 Render Pipeline")) {
    return 1;
  }
  const granit_render_pipeline_draw_binding binding{1, mesh, material, 0};
  granit_render_pipeline_render_desc render_desc = GRANIT_RENDER_PIPELINE_RENDER_DESC_INIT;
  render_desc.scene = scene;
  render_desc.output = output_view;
  render_desc.output_format = GRANIT_TEXTURE_FORMAT_RGBA8_UNORM;
  render_desc.width = size;
  render_desc.height = size;
  render_desc.draw_binding_count = 1;
  render_desc.draw_bindings = &binding;
  if (!check(granit_render_pipeline_render(native_renderer, pipeline, &render_desc), "渲染"))
    return 1;

  granit::buffer readback;
  granit::command_recorder recorder;
  const char* readback_stage = "创建 Readback Buffer";
  auto result =
      readback.initialize(native_renderer, {.size = size * size * 4,
                                            .usage = granit::buffer_usage::transfer_destination,
                                            .location = granit::memory_location::readback});
  if (granit::succeeded(result)) {
    readback_stage = "创建 Command Recorder";
    result = recorder.initialize(native_renderer);
  }
  if (granit::succeeded(result)) {
    readback_stage = "开始回读命令";
    result = recorder.begin();
  }
  if (granit::succeeded(result)) {
    readback_stage = "录制纹理回读";
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
    result = recorder.copy_texture_to_buffer(output, readback.native_handle(), layout, region);
  }
  if (granit::succeeded(result)) {
    readback_stage = "结束回读命令";
    result = recorder.end();
  }
  if (granit::succeeded(result)) {
    readback_stage = "提交回读命令";
    result = recorder.submit();
  }
  if (granit::succeeded(result)) {
    readback_stage = "重置回读命令";
    result = recorder.reset();
  }
  if (granit::failed(result)) {
    std::cerr << readback_stage << "失败：" << static_cast<granit_result>(result) << '\n';
    return 1;
  }
  void* mapped = nullptr;
  result = readback.map(0, size * size * 4, &mapped);
  const auto* pixel = static_cast<const std::uint8_t*>(mapped) + (size / 2 * size + size / 2) * 4;
  const std::array<std::uint8_t, 3> center{granit::succeeded(result) ? pixel[0] : std::uint8_t{0},
                                           granit::succeeded(result) ? pixel[1] : std::uint8_t{0},
                                           granit::succeeded(result) ? pixel[2] : std::uint8_t{0}};
  const bool rendered = center[0] != 0 || center[1] != 0 || center[2] != 0;
  if (granit::succeeded(result))
    result = readback.unmap();

  static_cast<void>(granit_render_pipeline_destroy(native_renderer, pipeline));
  static_cast<void>(granit_scene_snapshot_destroy(native_renderer, scene));
  static_cast<void>(granit_material_destroy(native_renderer, material));
  static_cast<void>(granit_mesh_destroy(native_renderer, mesh));
  static_cast<void>(granit_texture_view_destroy(native_renderer, output_view));
  static_cast<void>(granit_texture_destroy(native_renderer, output));
  if (!rendered || granit::failed(result)) {
    std::cerr << "中心像素未被自动渲染路径覆盖\n";
    return 1;
  }
  std::cout << "Render Pipeline 离屏渲染成功，中心像素：" << static_cast<int>(center[0]) << ", "
            << static_cast<int>(center[1]) << ", " << static_cast<int>(center[2]) << '\n';
  return 0;
}
