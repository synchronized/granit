// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "lighting/tone_mapping_resources.h"

#include <granit/granit.hpp>

#include <windows.h>

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
  std::ifstream stream{std::string{GRANIT_EXAMPLE_ASSET_DIR} + "/" + std::string{name},
                       std::ios::binary};
  const std::vector<char> bytes{std::istreambuf_iterator<char>{stream}, {}};
  if (bytes.empty() || bytes.size() % sizeof(std::uint32_t) != 0)
    return {};
  std::vector<std::uint32_t> words(bytes.size() / sizeof(std::uint32_t));
  std::memcpy(words.data(), bytes.data(), bytes.size());
  return words;
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
    if (granit::succeeded(result))
      result = view.initialize(renderer, texture.native_handle());
    if (granit::succeeded(result)) {
      result = granit::from_native(tone_mapping.initialize(
          renderer, view.native_handle(), output_format,
          {.exposure_scale = 1.0F, .encode_srgb = shader_encodes_srgb(output_format) ? 1U : 0U},
          std::as_bytes(vertex_shader), std::as_bytes(fragment_shader)));
    }
    if (granit::failed(result))
      static_cast<void>(reset());
    return result;
  }

  granit::result reset() {
    auto result = granit::from_native(tone_mapping.reset());
    const auto view_result = view.reset();
    if (granit::succeeded(result))
      result = view_result;
    const auto texture_result = texture.reset();
    if (granit::succeeded(result))
      result = texture_result;
    return result;
  }
};

granit::result render_frame(granit::swapchain& swapchain, granit::command_recorder& recorder,
                            const window_hdr_resources& resources, std::uint32_t width,
                            std::uint32_t height, bool& needs_recreate) {
  granit::acquired_frame frame;
  auto result = swapchain.acquire(frame);
  if (granit::failed(result))
    return result;
  needs_recreate = frame.needs_recreate;

  granit_texture backbuffer = GRANIT_NULL_HANDLE;
  granit_texture_view backbuffer_view = GRANIT_NULL_HANDLE;
  if (granit::succeeded(result))
    result = swapchain.backbuffer(frame.image_index, backbuffer, backbuffer_view);
  if (granit::succeeded(result))
    result = recorder.begin();

  const granit::color_attachment_desc hdr_color{
      .view = resources.view.native_handle(),
      .clear_value = {.red = 3.0F, .green = 0.4F, .blue = 0.08F, .alpha = 1.0F}};
  const granit::rendering_desc hdr_rendering{.color_attachments = std::span{&hdr_color, 1},
                                             .area = {0, 0, width, height}};
  if (granit::succeeded(result))
    result = recorder.begin_rendering(hdr_rendering);
  if (granit::succeeded(result))
    result = recorder.end_rendering();

  if (granit::succeeded(result))
    result = recorder.bind_graphics_pipeline(resources.tone_mapping.pipeline());
  const auto tone_group = resources.tone_mapping.group();
  if (granit::succeeded(result)) {
    result = recorder.bind_graphics_groups(resources.tone_mapping.pipeline_layout(), 0,
                                           std::span{&tone_group, 1});
  }
  const granit::viewport viewport{0, 0, static_cast<float>(width), static_cast<float>(height),
                                  0, 1};
  const granit::scissor scissor{0, 0, width, height};
  if (granit::succeeded(result))
    result = recorder.set_viewports(0, std::span{&viewport, 1});
  if (granit::succeeded(result))
    result = recorder.set_scissors(0, std::span{&scissor, 1});
  const granit::color_attachment_desc output_color{.view = backbuffer_view};
  const granit::rendering_desc output_rendering{.color_attachments = std::span{&output_color, 1},
                                                .area = {0, 0, width, height}};
  if (granit::succeeded(result))
    result = recorder.begin_rendering(output_rendering);
  if (granit::succeeded(result))
    result = recorder.draw(3);
  if (granit::succeeded(result))
    result = recorder.end_rendering();
  if (granit::succeeded(result))
    result = recorder.end();
  if (granit::succeeded(result))
    result = recorder.submit(frame);
  if (granit::succeeded(result))
    result = swapchain.present(frame);
  needs_recreate = needs_recreate || frame.needs_recreate;
  const auto reset_result = recorder.reset();
  return granit::failed(result) ? result : reset_result;
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
  if (granit::succeeded(result))
    result = surface.initialize_win32(renderer.native_handle(),
                                      {.instance = instance, .window = window});
  RECT client{};
  GetClientRect(window, &client);
  granit::swapchain swapchain;
  if (granit::succeeded(result)) {
    result = swapchain.initialize(renderer.native_handle(), surface.native_handle(),
                                  {.width = static_cast<std::uint32_t>(client.right),
                                   .height = static_cast<std::uint32_t>(client.bottom)});
  }
  granit::swapchain_info info;
  if (granit::succeeded(result))
    result = swapchain.query_info(info);
  const auto tone_vertex = load_shader("tone_mapping.vert.spv");
  const auto tone_fragment = load_shader("tone_mapping.frag.spv");
  if (granit::succeeded(result) && (tone_vertex.empty() || tone_fragment.empty()))
    result = granit::result::initialization_failed;
  window_hdr_resources resources;
  if (granit::succeeded(result)) {
    result = resources.initialize(renderer.native_handle(), info.width, info.height, info.format,
                                  tone_vertex, tone_fragment);
  }
  granit::command_recorder recorder;
  if (granit::succeeded(result))
    result = recorder.initialize(renderer.native_handle());
  if (granit::succeeded(result)) {
    std::cout << "Swapchain 格式=" << static_cast<std::uint32_t>(info.format)
              << (shader_encodes_srgb(info.format) ? "，Shader 执行 sRGB 编码\n"
                                                   : "，Attachment 执行 sRGB 编码\n");
  }

  bool running = granit::succeeded(result);
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
      if (granit::failed(result))
        break;
      granit::swapchain_info next_info;
      result = swapchain.query_info(next_info);
      if (granit::succeeded(result))
        result = resources.reset();
      if (granit::succeeded(result)) {
        result = resources.initialize(renderer.native_handle(), next_info.width, next_info.height,
                                      next_info.format, tone_vertex, tone_fragment);
      }
      if (granit::failed(result))
        break;
      info = next_info;
      recreate = false;
    }
    result = render_frame(swapchain, recorder, resources, info.width, info.height, recreate);
    if (result == granit::result::out_of_date) {
      recreate = true;
      continue;
    }
    if (granit::failed(result))
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
  if (IsWindow(window) != FALSE)
    DestroyWindow(window);
  if (granit::failed(result))
    std::cerr << "窗口 HDR 渲染失败：" << granit::result_message(result) << '\n';
  return granit::failed(result) ? 1 : 0;
}
