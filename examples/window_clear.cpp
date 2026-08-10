// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/granit.hpp>

#include <windows.h>

#include <cstdint>
#include <span>

namespace {

LRESULT CALLBACK window_proc(HWND window, UINT message, WPARAM word, LPARAM value) {
  if (message == WM_DESTROY) {
    PostQuitMessage(0);
    return 0;
  }
  return DefWindowProcW(window, message, word, value);
}

granit::result render_frame(granit::swapchain& swapchain, granit::command_recorder& recorder,
                            std::uint32_t width, std::uint32_t height, bool& needs_recreate) {
  granit::acquired_frame frame;
  auto result = swapchain.acquire(frame);
  if (granit::failed(result))
    return result;
  needs_recreate = frame.needs_recreate;

  granit_texture texture = GRANIT_NULL_HANDLE;
  granit_texture_view view = GRANIT_NULL_HANDLE;
  result = swapchain.backbuffer(frame.image_index, texture, view);
  if (granit::succeeded(result))
    result = recorder.begin();
  const granit::color_attachment_desc color{
      .view = view, .clear_value = {.red = 0.04F, .green = 0.12F, .blue = 0.22F, .alpha = 1.0F}};
  const granit::rendering_desc rendering{.color_attachments = std::span{&color, 1},
                                         .area = {0, 0, width, height}};
  if (granit::succeeded(result))
    result = recorder.begin_rendering(rendering);
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

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show_command) {
  constexpr wchar_t class_name[] = L"GranitWindowClearExample";
  WNDCLASSW window_class{};
  window_class.lpfnWndProc = window_proc;
  window_class.hInstance = instance;
  window_class.lpszClassName = class_name;
  if (RegisterClassW(&window_class) == 0)
    return 1;

  HWND window =
      CreateWindowExW(0, class_name, L"Granit 窗口清屏", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT,
                      CW_USEDEFAULT, 800, 600, nullptr, nullptr, instance, nullptr);
  if (window == nullptr)
    return 1;
  ShowWindow(window, show_command);

  granit::renderer renderer;
  auto result = renderer.initialize({.application_name = "Granit Window Clear",
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
  granit::command_recorder recorder;
  if (granit::succeeded(result))
    result = recorder.initialize(renderer.native_handle());
  if (granit::failed(result)) {
    DestroyWindow(window);
    return 1;
  }

  std::uint32_t width = static_cast<std::uint32_t>(client.right);
  std::uint32_t height = static_cast<std::uint32_t>(client.bottom);
  bool running = true;
  bool recreate = false;
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
      if (granit::failed(result))
        break;
      width = next_width;
      height = next_height;
      recreate = false;
    }

    result = render_frame(swapchain, recorder, width, height, recreate);
    if (result == granit::result::out_of_date) {
      recreate = true;
      continue;
    }
    if (granit::failed(result))
      break;
  }

  if (IsWindow(window) != FALSE)
    DestroyWindow(window);
  return granit::failed(result) ? 1 : 0;
}
