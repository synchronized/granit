// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/granit.hpp>

#include <windows.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <iterator>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace {

struct vertex {
  float x;
  float y;
  float red;
  float green;
  float blue;
};

std::vector<std::byte> load_shader(const char* name) {
  std::ifstream stream{std::string{GRANIT_EXAMPLE_ASSET_DIR} + "/" + name, std::ios::binary};
  const std::vector<char> bytes{std::istreambuf_iterator<char>{stream}, {}};
  std::vector<std::byte> result(bytes.size());
  for (std::size_t index = 0; index < bytes.size(); ++index)
    result[index] = static_cast<std::byte>(bytes[index]);
  return result;
}

LRESULT CALLBACK window_proc(HWND window, UINT message, WPARAM word, LPARAM value) {
  if (message == WM_DESTROY) {
    PostQuitMessage(0);
    return 0;
  }
  return DefWindowProcW(window, message, word, value);
}

granit::result initialize_pipeline(granit_renderer renderer, granit::pipeline_layout& layout,
                                   granit::shader& vertex_shader, granit::shader& fragment_shader,
                                   granit::texture_format format,
                                   granit::graphics_pipeline& pipeline) {
  const std::array attributes{granit::vertex_attribute{.location = 0,
                                                       .format = granit::vertex_format::float32x2,
                                                       .offset = offsetof(vertex, x)},
                              granit::vertex_attribute{.location = 1,
                                                       .format = granit::vertex_format::float32x3,
                                                       .offset = offsetof(vertex, red)}};
  const granit::vertex_buffer_layout vertex_layout{.stride = sizeof(vertex),
                                                   .step_mode = granit::vertex_step_mode::vertex,
                                                   .attributes = attributes};
  return pipeline.initialize(renderer, {.layout = layout.native_handle(),
                                        .vertex_shader = vertex_shader.native_handle(),
                                        .fragment_shader = fragment_shader.native_handle(),
                                        .color_formats = std::span{&format, 1},
                                        .vertex_buffers = std::span{&vertex_layout, 1},
                                        .primitive = {},
                                        .depth = {},
                                        .color_blends = {},
                                        .depth_bias = std::nullopt});
}

granit::result render_frame(granit::swapchain& swapchain, granit::frame_context& context,
                            granit::graphics_pipeline& pipeline, granit::buffer& vertex_buffer,
                            std::uint32_t width, std::uint32_t height, bool& needs_recreate) {
  granit::acquired_frame frame;
  auto result = swapchain.acquire(frame);
  if (result.failed())
    return result;
  needs_recreate = frame.needs_recreate;

  granit_texture texture = GRANIT_NULL_HANDLE;
  granit_texture_view view = GRANIT_NULL_HANDLE;
  if (result.ok())
    result = swapchain.backbuffer(frame.image_index, texture, view);
  granit::frame_recording recording;
  if (result.ok())
    result = context.begin(frame, recording);
  auto& recorder = recording.recorder();
  if (result.ok())
    result = recorder.bind_graphics_pipeline(pipeline.native_handle());
  const granit::viewport viewport{0, 0, static_cast<float>(width), static_cast<float>(height),
                                  0, 1};
  const granit::scissor scissor{0, 0, width, height};
  if (result.ok())
    result = recorder.set_viewports(0, std::span{&viewport, 1});
  if (result.ok())
    result = recorder.set_scissors(0, std::span{&scissor, 1});
  const granit::vertex_buffer_binding binding{vertex_buffer.native_handle(), 0};
  if (result.ok())
    result = recorder.bind_vertex_buffers(0, std::span{&binding, 1});
  const granit::color_attachment_desc color{
      .view = view, .clear_value = {.red = 0.025F, .green = 0.035F, .blue = 0.06F, .alpha = 1.0F}};
  const granit::rendering_desc rendering{.color_attachments = std::span{&color, 1},
                                         .area = {0, 0, width, height}};
  if (result.ok())
    result = recorder.begin_rendering(rendering);
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
  std::cout << "Granit 窗口三角形示例正在启动……\n";
  const auto instance = GetModuleHandleW(nullptr);
  constexpr wchar_t class_name[] = L"GranitWindowTriangleExample";
  WNDCLASSW window_class{};
  window_class.lpfnWndProc = window_proc;
  window_class.hInstance = instance;
  window_class.lpszClassName = class_name;
  if (RegisterClassW(&window_class) == 0) {
    std::cerr << "注册窗口类失败\n";
    return 1;
  }
  HWND window =
      CreateWindowExW(0, class_name, L"Granit Vertex Buffer 彩色三角形", WS_OVERLAPPEDWINDOW,
                      CW_USEDEFAULT, CW_USEDEFAULT, 800, 600, nullptr, nullptr, instance, nullptr);
  if (window == nullptr) {
    std::cerr << "创建窗口失败\n";
    return 1;
  }
  ShowWindow(window, SW_SHOW);

  granit::renderer renderer;
  auto result = renderer.initialize({.application_name = "Granit Window Triangle",
                                     .enable_validation = true,
                                     .surface_types = granit::surface_type::win32});
  granit::surface surface;
  if (result.ok())
    result = surface.initialize_win32(renderer.native_handle(),
                                      {.instance = instance, .window = window});
  RECT client{};
  GetClientRect(window, &client);
  granit::swapchain swapchain;
  if (result.ok())
    result = swapchain.initialize(renderer.native_handle(), surface.native_handle(),
                                  {.width = static_cast<std::uint32_t>(client.right),
                                   .height = static_cast<std::uint32_t>(client.bottom)});
  granit::swapchain_info swapchain_info;
  if (result.ok())
    result = swapchain.query_info(swapchain_info);

  const auto vertex_code = load_shader("window_triangle.vert.spv");
  const auto fragment_code = load_shader("window_triangle.frag.spv");
  if (result.ok() && (vertex_code.empty() || fragment_code.empty())) {
    std::cerr << "无法读取窗口三角形 Shader\n";
    result = granit::result::initialization_failed;
  }
  granit::shader vertex_shader;
  granit::shader fragment_shader;
  if (result.ok())
    result = vertex_shader.initialize(renderer.native_handle(),
                                      {.stage = granit::shader_stage::vertex, .code = vertex_code});
  if (result.ok())
    result = fragment_shader.initialize(
        renderer.native_handle(), {.stage = granit::shader_stage::fragment, .code = fragment_code});
  granit::pipeline_layout layout;
  if (result.ok())
    result = layout.initialize(renderer.native_handle());
  granit::graphics_pipeline pipeline;
  if (result.ok())
    result = initialize_pipeline(renderer.native_handle(), layout, vertex_shader, fragment_shader,
                                 swapchain_info.format, pipeline);

  constexpr std::array vertices{
      vertex{0.0F, -0.65F, 1.0F, 0.15F, 0.1F},
      vertex{0.65F, 0.55F, 0.1F, 1.0F, 0.2F},
      vertex{-0.65F, 0.55F, 0.15F, 0.3F, 1.0F},
  };
  granit::buffer vertex_buffer;
  if (result.ok())
    result = vertex_buffer.initialize(
        renderer.native_handle(), {.size = sizeof(vertices), .usage = granit::buffer_usage::vertex},
        std::as_bytes(std::span{vertices}));
  granit::frame_context frame_context;
  if (result.ok())
    result = frame_context.initialize(renderer.native_handle());
  if (result.failed()) {
    std::cerr << "初始化失败：" << granit::result_message(result) << '\n';
    DestroyWindow(window);
    return 1;
  }

  std::cout << "初始化完成；关闭窗口即可退出。\n";
  std::uint32_t width = swapchain_info.width;
  std::uint32_t height = swapchain_info.height;
  bool running = true;
  bool recreate = false;
  std::uint32_t rendered_frames{};
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
    const auto next_width = static_cast<std::uint32_t>(client.right - client.left);
    const auto next_height = static_cast<std::uint32_t>(client.bottom - client.top);
    if (next_width == 0 || next_height == 0) {
      WaitMessage();
      continue;
    }
    if (recreate || next_width != width || next_height != height) {
      result = swapchain.recreate({.width = next_width, .height = next_height});
      if (result == granit::result::not_ready)
        continue;
      if (result.failed())
        break;
      granit::swapchain_info next_info;
      result = swapchain.query_info(next_info);
      if (result.failed())
        break;
      if (next_info.format != swapchain_info.format) {
        result = pipeline.reset();
        if (result.ok())
          result = initialize_pipeline(renderer.native_handle(), layout, vertex_shader,
                                       fragment_shader, next_info.format, pipeline);
        if (result.failed())
          break;
      }
      swapchain_info = next_info;
      width = next_info.width;
      height = next_info.height;
      recreate = false;
    }
    result =
        render_frame(swapchain, frame_context, pipeline, vertex_buffer, width, height, recreate);
    if (result == granit::result::out_of_date) {
      recreate = true;
      continue;
    }
    if (result.failed())
      break;
    ++rendered_frames;
    if (smoke_test && rendered_frames == 3)
      running = false;
  }
  if (IsWindow(window) != FALSE)
    DestroyWindow(window);
  if (result.failed())
    std::cerr << "渲染失败：" << granit::result_message(result) << '\n';
  else
    std::cout << "示例已正常退出。\n";
  return result.failed() ? 1 : 0;
}
