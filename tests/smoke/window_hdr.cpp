// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "lighting/tone_mapping_resources.h"
#include "material/pbr_material_schema.h"
#include "pbr_test_support.h"

#include <granit/granit.hpp>

#include <windows.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace {

std::vector<std::uint32_t> load_shader(std::string_view name) {
  const auto directory =
      name.starts_with("tone_mapping") ? GRANIT_PIPELINE_SHADER_DIR : GRANIT_PBR_SHADER_DIR;
  std::ifstream stream{std::string{directory} + "/" + std::string{name}, std::ios::binary};
  const std::vector<char> bytes{std::istreambuf_iterator<char>{stream}, {}};
  if (bytes.empty() || bytes.size() % sizeof(std::uint32_t) != 0)
    return {};
  std::vector<std::uint32_t> words(bytes.size() / sizeof(std::uint32_t));
  std::memcpy(words.data(), bytes.data(), bytes.size());
  return words;
}

std::string load_shader_text(std::string_view name) {
  const auto directory =
      name.starts_with("tone_mapping") ? GRANIT_PIPELINE_SHADER_DIR : GRANIT_PBR_SHADER_DIR;
  std::ifstream stream{std::string{directory} + "/" + std::string{name}, std::ios::binary};
  return {std::istreambuf_iterator<char>{stream}, {}};
}

LRESULT CALLBACK window_proc(HWND window, UINT message, WPARAM word, LPARAM value) {
  if (message == WM_DESTROY) {
    PostQuitMessage(0);
    return 0;
  }
  return DefWindowProcW(window, message, word, value);
}

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
                            std::span<const std::uint32_t> vertex_shader,
                            std::span<const std::uint32_t> fragment_shader) {
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
          std::as_bytes(vertex_shader), std::as_bytes(fragment_shader)));
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
  SetConsoleOutputCP(CP_UTF8);
  const bool smoke_test = argument_count == 2 && std::string_view{arguments[1]} == "--smoke-test";
  const auto instance = GetModuleHandleW(nullptr);
  constexpr wchar_t class_name[] = L"GranitWindowHdrExample";
  WNDCLASSW window_class{};
  window_class.lpfnWndProc = window_proc;
  window_class.hInstance = instance;
  window_class.lpszClassName = class_name;
  if (RegisterClassW(&window_class) == 0)
    return 1;
  HWND window =
      CreateWindowExW(0, class_name, L"Granit HDR Tone Mapping", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT,
                      CW_USEDEFAULT, 800, 600, nullptr, nullptr, instance, nullptr);
  if (window == nullptr)
    return 1;
  ShowWindow(window, SW_SHOW);

  granit::renderer renderer;
  auto result = renderer.initialize({.application_name = "Granit Window HDR",
                                     .enable_validation = true,
                                     .surface_types = granit::surface_type::win32});
  granit::surface surface;
  if (result.ok())
    result = surface.initialize_win32(renderer.native_handle(),
                                      {.instance = instance, .window = window});
  RECT client{};
  GetClientRect(window, &client);
  granit::swapchain swapchain;
  if (result.ok()) {
    result = swapchain.initialize(renderer.native_handle(), surface.native_handle(),
                                  {.width = static_cast<std::uint32_t>(client.right),
                                   .height = static_cast<std::uint32_t>(client.bottom)});
  }
  granit::swapchain_info info;
  if (result.ok())
    result = swapchain.query_info(info);
  const auto tone_vertex = load_shader("tone_mapping.vert.spv");
  const auto tone_fragment = load_shader("tone_mapping.frag.spv");
  const auto pbr_vertex = load_shader("pbr_shadow_ibl_lights.vert.spv");
  const auto pbr_fragment = load_shader("pbr_shadow_ibl_lights_untextured.frag.spv");
  const auto pbr_vertex_wgsl = load_shader_text("pbr_shadow_ibl_lights.vert.wgsl");
  const auto pbr_fragment_wgsl =
      load_shader_text("pbr_shadow_ibl_lights_untextured.frag.wgsl");
  if (result.ok() && (tone_vertex.empty() || tone_fragment.empty() ||
                                    pbr_vertex.empty() || pbr_fragment.empty())) {
    result = granit::result::initialization_failed;
  }

  granit::material::material_package pbr_package;
  if (result.ok() &&
      !granit::test::build_pbr_package(pbr_package, pbr_vertex, pbr_vertex_wgsl,
                                           pbr_fragment, pbr_fragment_wgsl)) {
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
    result = granit::from_native(
        pbr_material.initialize(renderer.native_handle(), pbr_package, additional_layouts));
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
                                  tone_vertex, tone_fragment);
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
    MSG message{};
    while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE) != 0) {
      if (message.message == WM_QUIT)
        running = false;
      TranslateMessage(&message);
      DispatchMessageW(&message);
    }
    if (!running)
      break;
    GetClientRect(window, &client);
    const auto width = static_cast<std::uint32_t>(client.right - client.left);
    const auto height = static_cast<std::uint32_t>(client.bottom - client.top);
    if (width == 0 || height == 0) {
      WaitMessage();
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
                                      next_info.format, tone_vertex, tone_fragment);
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
      if (rendered_frames == 1) {
        SetWindowPos(window, nullptr, 0, 0, 640, 480, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
      } else if (rendered_frames == 3) {
        running = false;
      }
    }
  }

  static_cast<void>(resources.reset());
  static_cast<void>(pbr_lighting.reset());
  if (IsWindow(window) != FALSE)
    DestroyWindow(window);
  if (result.failed())
    std::cerr << "窗口 HDR 渲染失败：" << granit::result_message(result) << '\n';
  return result.failed() ? 1 : 0;
}
