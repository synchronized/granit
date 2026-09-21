// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "lighting/tone_mapping_resources.h"
#include "material/pbr_material_schema.h"
#include "support/pbr_test_support.h"
#include "support/shader_asset_store.h"
#include "support/tone_mapping_shader_library.h"

#include <granit/granit.hpp>
#include <granit/window.hpp>

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace {

bool shader_encodes_srgb(granit::texture_format format) {
  return format == granit::texture_format::rgba8_unorm ||
         format == granit::texture_format::bgra8_unorm;
}

struct window_hdr_resources {
  granit::texture texture;
  granit::texture_view view;
  granit::texture depth_texture;
  granit::texture_view depth_view;
  granit::lighting::tone_mapping_resources tone_mapping;

  granit::result initialize(granit_renderer renderer, std::uint32_t width, std::uint32_t height,
                            granit::texture_format output_format,
                            const granit::shader_library& shader_library,
                            const granit::shader_content_id& vertex_shader_id,
                            const granit::shader_content_id& fragment_shader_id) {
    auto result = texture.initialize(renderer, {.format = granit::texture_format::rgba16_float,
                                                .usage = granit::texture_usage::color_attachment |
                                                         granit::texture_usage::sampled,
                                                .width = width,
                                                .height = height});
    if (result.ok())
      result = view.initialize(renderer, texture.native_handle());
    if (result.ok()) {
      result = depth_texture.initialize(renderer,
                                        {.format = granit::texture_format::d32_float,
                                         .usage = granit::texture_usage::depth_stencil_attachment,
                                         .width = width,
                                         .height = height});
    }
    if (result.ok())
      result = depth_view.initialize(renderer, depth_texture.native_handle());
    if (result.ok()) {
      result = granit::from_native(tone_mapping.initialize(
          renderer, view.native_handle(), output_format,
          {.exposure_scale = 1.0F, .encode_srgb = shader_encodes_srgb(output_format) ? 1U : 0U},
          shader_library, vertex_shader_id, fragment_shader_id));
    }
    if (result.failed())
      static_cast<void>(reset());
    return result;
  }

  granit::result reset() {
    auto result = granit::from_native(tone_mapping.reset());
    const auto view_result = view.reset();
    if (result.ok())
      result = view_result;
    const auto texture_result = texture.reset();
    if (result.ok())
      result = texture_result;
    const auto depth_view_result = depth_view.reset();
    if (result.ok())
      result = depth_view_result;
    const auto depth_texture_result = depth_texture.reset();
    if (result.ok())
      result = depth_texture_result;
    return result;
  }
};

granit::result render_frame(granit::swapchain& swapchain, granit::frame_context& context,
                            const window_hdr_resources& resources,
                            granit::material::material_template_gpu& pbr_material,
                            granit_graphics_pipeline pbr_pipeline, granit_bind_group material_group,
                            granit_bind_group lighting_group, granit_texture_view shadow_view,
                            std::uint32_t width, std::uint32_t height, bool& needs_recreate) {
  granit::acquired_frame frame;
  auto result = swapchain.acquire(frame);
  if (result.failed())
    return result;
  needs_recreate = frame.needs_recreate;

  granit_texture backbuffer = GRANIT_NULL_HANDLE;
  granit_texture_view backbuffer_view = GRANIT_NULL_HANDLE;
  if (result.ok())
    result = swapchain.backbuffer(frame.image_index, backbuffer, backbuffer_view);
  granit::frame_recording recording;
  if (result.ok())
    result = context.begin(frame, recording);
  auto& recorder = recording.recorder();

  const granit::depth_stencil_attachment_desc shadow_depth{.view = shadow_view,
                                                           .clear_value = {.depth = 1.0F}};
  const granit::rendering_desc shadow_rendering{
      .color_attachments = {}, .depth_stencil_attachment = &shadow_depth, .area = {0, 0, 1, 1}};
  if (result.ok())
    result = recorder.begin_rendering(shadow_rendering);
  if (result.ok())
    result = recorder.end_rendering();

  const granit::color_attachment_desc hdr_color{
      .view = resources.view.native_handle(),
      .clear_value = {.red = 0.03F, .green = 0.03F, .blue = 0.05F, .alpha = 1.0F}};
  const granit::depth_stencil_attachment_desc depth{.view = resources.depth_view.native_handle(),
                                                    .clear_value = {.depth = 1.0F}};
  const granit::rendering_desc hdr_rendering{.color_attachments = std::span{&hdr_color, 1},
                                             .depth_stencil_attachment = &depth,
                                             .area = {0, 0, width, height}};
  if (result.ok())
    result = recorder.bind_graphics_pipeline(pbr_pipeline);
  if (result.ok()) {
    result = recorder.bind_graphics_groups(pbr_material.pipeline_layout(), 1,
                                           std::span{&material_group, 1});
  }
  if (result.ok()) {
    result = recorder.bind_graphics_groups(pbr_material.pipeline_layout(), 3,
                                           std::span{&lighting_group, 1});
  }
  const granit::viewport viewport{0, 0, static_cast<float>(width), static_cast<float>(height),
                                  0, 1};
  const granit::scissor scissor{0, 0, width, height};
  if (result.ok())
    result = recorder.set_viewports(0, std::span{&viewport, 1});
  if (result.ok())
    result = recorder.set_scissors(0, std::span{&scissor, 1});
  if (result.ok())
    result = recorder.begin_rendering(hdr_rendering);
  if (result.ok())
    result = recorder.draw(3);
  if (result.ok())
    result = recorder.end_rendering();

  if (result.ok())
    result = recorder.bind_graphics_pipeline(resources.tone_mapping.pipeline());
  const auto tone_group = resources.tone_mapping.group();
  if (result.ok()) {
    result = recorder.bind_graphics_groups(resources.tone_mapping.pipeline_layout(), 0,
                                           std::span{&tone_group, 1});
  }
  if (result.ok())
    result = recorder.set_viewports(0, std::span{&viewport, 1});
  if (result.ok())
    result = recorder.set_scissors(0, std::span{&scissor, 1});
  const granit::color_attachment_desc output_color{.view = backbuffer_view};
  const granit::rendering_desc output_rendering{.color_attachments = std::span{&output_color, 1},
                                                .area = {0, 0, width, height}};
  if (result.ok())
    result = recorder.begin_rendering(output_rendering);
  if (result.ok())
    result = recorder.draw(3);
  if (result.ok())
    result = recorder.end_rendering();
  if (result.ok())
    result = recording.submit();
  if (result.ok())
    result = swapchain.present(frame);
  needs_recreate = needs_recreate || frame.needs_recreate;
  if (result.failed()) {
    if (recording.valid())
      static_cast<void>(recording.abort());
    if (frame.valid())
      static_cast<void>(swapchain.cancel(frame));
  }
  return result;
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
                               {.title = "Granit HDR Tone Mapping", .width = 800, .height = 600});
  }
  granit::window_state window_state{};
  if (result.ok())
    result = window.get_state(window_state);

  granit::renderer renderer;
  if (result.ok()) {
    result = renderer.initialize({.application_name = "Granit Window HDR",
                                  .enable_validation = true,
                                  .presentation = granit::presentation_mode::enabled});
  }
  granit::surface surface;
  if (result.ok())
    result = window.create_surface(renderer, surface);
  granit::swapchain swapchain;
  if (result.ok()) {
    result = swapchain.initialize(
        renderer.native_handle(), surface.native_handle(),
        {.width = window_state.framebuffer_width, .height = window_state.framebuffer_height});
  }
  granit::swapchain_info info;
  if (result.ok())
    result = swapchain.query_info(info);
  granit::tests::shader_asset_store assets;
  granit::tests::tone_mapping_shader_library tone_shaders;
  if (result.ok() && (!assets.add(std::string{GRANIT_PBR_SHADER_DIR} +
                                  "/pbr_shadow_ibl_lights.vert.grshaderobj") ||
                      !assets.add(std::string{GRANIT_PBR_SHADER_DIR} +
                                  "/pbr_shadow_ibl_lights_untextured.frag.grshaderobj")))
    result = granit::result::initialization_failed;
  std::vector<std::byte> shader_library_bytes;
  granit::shader_library shader_library;
  if (result.ok() &&
      !assets.initialize_library(renderer.native_handle(), shader_library_bytes, shader_library))
    result = granit::result::initialization_failed;
  if (result.ok() && !tone_shaders.initialize(renderer.native_handle()))
    result = granit::result::initialization_failed;

  granit::material::material_package pbr_package;
  if (result.ok() && !granit::test::build_pbr_package(
                         pbr_package,
                         assets.reference(std::string{GRANIT_PBR_SHADER_DIR} +
                                          "/pbr_shadow_ibl_lights.vert.grshaderobj"),
                         assets.reference(std::string{GRANIT_PBR_SHADER_DIR} +
                                          "/pbr_shadow_ibl_lights_untextured.frag.grshaderobj"))) {
    result = granit::result::initialization_failed;
  }
  granit::test::pbr_lighting_resources pbr_lighting;
  if (result.ok())
    result = pbr_lighting.initialize(renderer.native_handle());
  granit::bind_group_layout object_layout;
  if (result.ok())
    result = object_layout.initialize(renderer.native_handle(), {});
  granit::material::material_template_gpu pbr_material;
  if (result.ok()) {
    const std::array additional_layouts{object_layout.native_handle(), pbr_lighting.layout()};
    result = granit::from_native(pbr_material.initialize(
        renderer.native_handle(), pbr_package, additional_layouts, shader_library.native_handle()));
  }
  granit_graphics_pipeline pbr_pipeline = GRANIT_NULL_HANDLE;
  if (result.ok()) {
    const std::array features{granit::material::material_feature_value{
        granit::material::make_feature_id(granit::material::pbr_texture_feature_name), 0}};
    result = granit::from_native(
        pbr_material.acquire_pipeline({.pass = granit::material::make_feature_id("opaque"),
                                       .variant = granit::material::make_variant_key(features),
                                       .color_format = GRANIT_TEXTURE_FORMAT_RGBA16_FLOAT,
                                       .depth_stencil_format = GRANIT_TEXTURE_FORMAT_D32_FLOAT},
                                      pbr_pipeline));
  }
  granit::material::pbr_default_resources pbr_defaults;
  granit::material::material_gpu_instance pbr_instance;
  if (result.ok())
    result = granit::from_native(pbr_defaults.initialize(renderer.native_handle()));
  if (result.ok()) {
    result = granit::test::initialize_pbr_instance(renderer.native_handle(), pbr_material,
                                                   pbr_package, pbr_defaults, pbr_instance);
  }
  window_hdr_resources resources;
  if (result.ok()) {
    result = resources.initialize(renderer.native_handle(), info.width, info.height, info.format,
                                  tone_shaders.library(), tone_shaders.vertex_id(),
                                  tone_shaders.fragment_id());
  }
  granit::frame_context frame_context;
  if (result.ok())
    result = frame_context.initialize(renderer.native_handle());
  if (result.ok()) {
    std::cout << "Swapchain 格式=" << static_cast<std::uint32_t>(info.format)
              << (shader_encodes_srgb(info.format) ? "，Shader 执行 sRGB 编码\n"
                                                   : "，Attachment 执行 sRGB 编码\n");
  }

  bool running = result.ok();
  bool recreate = false;
  std::uint32_t rendered_frames = 0;
  while (running) {
    result = window_system.process_events();
    granit::window_event event{};
    while (result.ok() && (result = window_system.poll(event)).ok()) {
      if (event.window != window.native_handle())
        continue;
      if (event.type == granit::window_event_type::close_requested)
        running = false;
      if (event.type == granit::window_event_type::resized ||
          event.type == granit::window_event_type::scale_changed ||
          event.type == granit::window_event_type::native_handle_changed) {
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
      granit::swapchain_info next_info;
      result = swapchain.query_info(next_info);
      if (result.ok())
        result = resources.reset();
      if (result.ok()) {
        result = resources.initialize(renderer.native_handle(), next_info.width, next_info.height,
                                      next_info.format, tone_shaders.library(),
                                      tone_shaders.vertex_id(), tone_shaders.fragment_id());
      }
      if (result.failed())
        break;
      info = next_info;
      recreate = false;
    }
    result = render_frame(swapchain, frame_context, resources, pbr_material, pbr_pipeline,
                          pbr_instance.bind_group(), pbr_lighting.group(),
                          pbr_lighting.shadow_view(), info.width, info.height, recreate);
    if (result == granit::result::out_of_date) {
      recreate = true;
      continue;
    }
    if (result.failed())
      break;
    if (smoke_test) {
      ++rendered_frames;
      if (rendered_frames == 1)
        recreate = true;
      else if (rendered_frames == 3)
        running = false;
    }
  }

  static_cast<void>(resources.reset());
  static_cast<void>(pbr_lighting.reset());
  if (result.failed())
    std::cerr << "窗口 HDR 渲染失败：" << granit::result_message(result) << '\n';
  return result.failed() ? 1 : 0;
}
