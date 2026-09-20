// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "../support/shader_asset_store.h"
#include <granit/granit.hpp>
#include <granit/pipeline/canvas_draw_list.hpp>
#include <granit/pipeline/render_pipeline.h>
#include <granit/window.hpp>

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace {

granit::tests::shader_asset_store& shader_assets() {
  static granit::tests::shader_asset_store store;
  static const bool loaded = store.add(std::string{GRANIT_PIPELINE_ASSET_DIR} +
                                       "/pbr_shadow_ibl_lights.vert.grshaderobj") &&
                             store.add(std::string{GRANIT_PIPELINE_ASSET_DIR} +
                                       "/pbr_shadow_ibl_lights_untextured.frag.grshaderobj");
  if (!loaded)
    std::abort();
  return store;
}

granit_matrix4 identity() {
  granit_matrix4 value{};
  value.elements[0] = 1.0F;
  value.elements[5] = 1.0F;
  value.elements[10] = 1.0F;
  value.elements[15] = 1.0F;
  return value;
}

std::vector<char> load_package() {
  std::ifstream stream{GRANIT_RENDER_PIPELINE_SMOKE_PACKAGE, std::ios::binary};
  return {std::istreambuf_iterator<char>{stream}, {}};
}

granit_result create_scene(granit_renderer renderer, std::uint32_t width, std::uint32_t height,
                           bool with_renderable, granit_scene_snapshot* scene) {
  granit_scene_view view{};
  view.view = identity();
  view.projection = identity();
  view.view_projection = identity();
  view.viewport_width = static_cast<float>(width);
  view.viewport_height = static_cast<float>(height);
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
  granit_scene_snapshot_desc desc = GRANIT_SCENE_SNAPSHOT_DESC_INIT;
  desc.views = &view;
  desc.view_count = 1;
  if (with_renderable) {
    desc.renderables = &renderable;
    desc.renderable_count = 1;
    desc.directional_lights = &light;
    desc.directional_light_count = 1;
  }
  return granit_scene_snapshot_create(renderer, &desc, scene);
}

} // namespace

int main(int argument_count, char** arguments) {
  const bool smoke_test = argument_count == 2 && std::string_view{arguments[1]} == "--smoke-test";
  granit::window_system window_system;
  auto result = window_system.initialize();
  if (result == granit::result::backend_unavailable)
    return 77;
  granit::window window;
  if (result.ok()) {
    result = window.initialize(window_system.native_handle(),
                               {.title = "Granit Render Pipeline", .width = 800, .height = 600});
  }
  granit::window_state window_state{};
  if (result.ok())
    result = window.get_state(window_state);

  granit::renderer renderer;
  if (result.ok()) {
    result = renderer.initialize({.application_name = "Granit Render Pipeline Window",
                                  .enable_validation = true,
                                  .presentation = granit::presentation_mode::enabled});
  }
  std::vector<std::byte> shader_library_bytes;
  granit::shader_library shader_library;
  if (result.ok() && !shader_assets().initialize_library(renderer.native_handle(),
                                                         shader_library_bytes, shader_library))
    result = granit::result::initialization_failed;
  granit::surface surface;
  if (result.ok())
    result = window.create_surface(renderer.native_handle(), surface);
  granit::swapchain swapchain;
  if (result.ok()) {
    result = swapchain.initialize(renderer.native_handle(), surface.native_handle(),
                                  {.width = window_state.framebuffer_width,
                                   .height = window_state.framebuffer_height});
  }
  granit::swapchain_info info;
  if (result.ok())
    result = swapchain.query_info(info);

  granit::texture canvas_texture;
  granit::texture_view canvas_view;
  granit::sampler canvas_sampler;
  granit::canvas_draw_list canvas;
  if (result.ok()) {
    result = canvas_texture.initialize(
        renderer.native_handle(),
        {.format = granit::texture_format::rgba8_unorm,
         .usage = granit::texture_usage::sampled | granit::texture_usage::transfer_destination,
         .width = 1,
         .height = 1});
  }
  constexpr std::array<std::uint8_t, 4> canvas_pixel{32, 160, 255, 255};
  if (result.ok())
    result = canvas_texture.write(std::as_bytes(std::span{canvas_pixel}), {}, {});
  if (result.ok())
    result = canvas_view.initialize(renderer.native_handle(), canvas_texture.native_handle());
  if (result.ok())
    result = canvas_sampler.initialize(renderer.native_handle(), {});
  if (result.ok())
    result = canvas.initialize(renderer.native_handle(), GRANIT_CANVAS_DRAW_LIST_DESC_INIT);
  granit_canvas_rect_desc canvas_rect = GRANIT_CANVAS_RECT_DESC_INIT;
  canvas_rect.x = 12;
  canvas_rect.y = 12;
  canvas_rect.width = 96;
  canvas_rect.height = 48;
  canvas_rect.state.texture = canvas_view.native_handle();
  canvas_rect.state.sampler = canvas_sampler.native_handle();
  if (result.ok())
    result = canvas.append_rect(canvas_rect);

  constexpr std::array<float, 9> positions{-0.65F, -0.65F, 0.5F,  0.65F, -0.65F,
                                           0.5F,   0.0F,   0.65F, 0.5F};
  granit::buffer vertex_buffer;
  if (result.ok()) {
    result =
        vertex_buffer.initialize(renderer.native_handle(),
                                 {.size = sizeof(positions), .usage = granit::buffer_usage::vertex},
                                 std::as_bytes(std::span{positions}));
  }
  const granit_vertex_attribute attribute{0, GRANIT_VERTEX_FORMAT_FLOAT32X3, 0, 0};
  const granit_mesh_vertex_buffer vertex{
      vertex_buffer.native_handle(), 0, {12, GRANIT_VERTEX_STEP_MODE_VERTEX, 1, 0, &attribute}};
  granit_mesh_desc mesh_desc = GRANIT_MESH_DESC_INIT;
  mesh_desc.vertex_buffers = &vertex;
  mesh_desc.vertex_buffer_count = 1;
  mesh_desc.vertex_count = 3;
  granit_mesh mesh = GRANIT_NULL_HANDLE;
  if (result.ok())
    result = granit::from_native(granit_mesh_create(renderer.native_handle(), &mesh_desc, &mesh));

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
  material_desc.shader_library = shader_library.native_handle();
  granit_material material = GRANIT_NULL_HANDLE;
  if (result.ok() && !archive.empty()) {
    result = granit::from_native(
        granit_material_create(renderer.native_handle(), &material_desc, &material));
  } else if (archive.empty()) {
    result = granit::result::initialization_failed;
  }
  granit_render_pipeline pipeline = GRANIT_NULL_HANDLE;
  const granit_render_pipeline_desc pipeline_desc = GRANIT_RENDER_PIPELINE_DESC_INIT;
  if (result.ok()) {
    result = granit::from_native(
        granit_render_pipeline_create(renderer.native_handle(), &pipeline_desc, &pipeline));
  }
  granit_scene_snapshot scene = GRANIT_NULL_HANDLE;
  granit_scene_snapshot empty_scene = GRANIT_NULL_HANDLE;
  if (result.ok()) {
    result = granit::from_native(
        create_scene(renderer.native_handle(), info.width, info.height, true, &scene));
  }
  if (result.ok()) {
    result = granit::from_native(
        create_scene(renderer.native_handle(), info.width, info.height, false, &empty_scene));
  }
  if (result.failed()) {
    std::cerr << "初始化失败：" << granit::result_message(result) << '\n';
    return 1;
  }

  const granit_render_pipeline_draw_binding binding{1, mesh, material, 0};
  bool running = true;
  bool recreate = false;
  std::uint32_t rendered_frames = 0;
  std::uint32_t completed_recreates = 0;
  while (running) {
    result = window_system.process_events();
    granit::window_event event{};
    while (result.ok() && (result = window_system.poll(event)).ok()) {
      if (event.window != window.native_handle())
        continue;
      if (event.type == GRANIT_WINDOW_EVENT_CLOSE_REQUESTED)
        running = false;
      if (event.type == GRANIT_WINDOW_EVENT_RESIZED ||
          event.type == GRANIT_WINDOW_EVENT_SCALE_CHANGED ||
          event.type == GRANIT_WINDOW_EVENT_NATIVE_HANDLE_CHANGED) {
        recreate = true;
      }
    }
    if (result == granit::result::not_ready)
      result = granit::result::success;
    if (result.ok())
      result = window.get_state(window_state);
    if (result.failed())
      break;
    if (!running)
      break;
    const auto width = window_state.framebuffer_width;
    const auto height = window_state.framebuffer_height;
    if (width == 0 || height == 0) {
      std::this_thread::sleep_for(std::chrono::milliseconds{16});
      continue;
    }
    if (recreate || width != info.width || height != info.height) {
      result = swapchain.recreate({.width = width, .height = height});
      if (result == granit::result::not_ready)
        continue;
      if (result.failed())
        break;
      result = swapchain.query_info(info);
      if (result.failed())
        break;
      static_cast<void>(granit_scene_snapshot_destroy(renderer.native_handle(), scene));
      static_cast<void>(granit_scene_snapshot_destroy(renderer.native_handle(), empty_scene));
      scene = GRANIT_NULL_HANDLE;
      empty_scene = GRANIT_NULL_HANDLE;
      result = granit::from_native(
          create_scene(renderer.native_handle(), info.width, info.height, true, &scene));
      if (result.ok()) {
        result = granit::from_native(
            create_scene(renderer.native_handle(), info.width, info.height, false, &empty_scene));
      }
      recreate = false;
      ++completed_recreates;
      if (result.failed())
        break;
    }

    granit::acquired_frame frame;
    result = swapchain.acquire(frame);
    if (result == granit::result::out_of_date) {
      recreate = true;
      continue;
    }
    if (result.failed())
      break;
    recreate = frame.needs_recreate;
    granit_texture backbuffer = GRANIT_NULL_HANDLE;
    granit_texture_view backbuffer_view = GRANIT_NULL_HANDLE;
    result = swapchain.backbuffer(frame.image_index, backbuffer, backbuffer_view);
    if (result.ok()) {
      granit_render_pipeline_render_desc desc = GRANIT_RENDER_PIPELINE_RENDER_DESC_INIT;
      const bool empty_frame = smoke_test && rendered_frames == 0;
      desc.scene = empty_frame ? empty_scene : scene;
      desc.output = backbuffer_view;
      desc.output_format = static_cast<granit_texture_format>(info.format);
      desc.width = info.width;
      desc.height = info.height;
      desc.draw_binding_count = empty_frame ? 0 : 1;
      desc.draw_bindings = empty_frame ? nullptr : &binding;
      desc.frame = frame.handle;
      desc.canvas = canvas.native_handle();
      result = granit::from_native(
          granit_render_pipeline_render(renderer.native_handle(), pipeline, &desc));
    }
    if (result.ok())
      result = swapchain.present(frame);
    recreate = recreate || frame.needs_recreate;
    if (result.failed())
      break;
    ++rendered_frames;
    if (smoke_test && (rendered_frames == 1 || rendered_frames == 3))
      recreate = true;
    else if (smoke_test && rendered_frames >= 5 && completed_recreates >= 2)
      running = false;
  }

  static_cast<void>(granit_render_pipeline_destroy(renderer.native_handle(), pipeline));
  static_cast<void>(granit_scene_snapshot_destroy(renderer.native_handle(), scene));
  static_cast<void>(granit_scene_snapshot_destroy(renderer.native_handle(), empty_scene));
  static_cast<void>(granit_material_destroy(renderer.native_handle(), material));
  static_cast<void>(granit_mesh_destroy(renderer.native_handle(), mesh));
  if (result.failed())
    std::cerr << "渲染失败：" << granit::result_message(result) << '\n';
  if (smoke_test && completed_recreates < 2) {
    std::cerr << "窗口烟雾测试未完成两次尺寸重建\n";
    return 1;
  }
  return result.failed() ? 1 : 0;
}
