// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "platform/window/window_backend_internal.h"

#include <windows.h>
#include <windowsx.h>

#include <cmath>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace granit::window::detail {

constexpr wchar_t window_class_name[] = L"GranitWindowComponent";
std::once_flag window_class_once;
bool window_class_registered{};

LRESULT CALLBACK window_proc(HWND hwnd, UINT message, WPARAM word, LPARAM value) {
  auto* record = reinterpret_cast<window_record*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
  if (message == WM_NCCREATE) {
    const auto* create = reinterpret_cast<const CREATESTRUCTW*>(value);
    record = static_cast<window_record*>(create->lpCreateParams);
    SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(record));
  }
  if (record == nullptr)
    return DefWindowProcW(hwnd, message, word, value);
  const auto system = record->system.lock();
  if (!system)
    return DefWindowProcW(hwnd, message, word, value);

  if (message == WM_MOUSEMOVE && !record->pointer_tracking) {
    TRACKMOUSEEVENT tracking{sizeof(TRACKMOUSEEVENT), TME_LEAVE, hwnd, 0};
    record->pointer_tracking = TrackMouseEvent(&tracking) != FALSE;
  } else if (message == WM_MOUSELEAVE) {
    record->pointer_tracking = false;
  }
  if (system->input_native_event != nullptr) {
    auto input_value = value;
    if (message == WM_MOUSEWHEEL || message == WM_MOUSEHWHEEL) {
      POINT point{GET_X_LPARAM(value), GET_Y_LPARAM(value)};
      if (ScreenToClient(hwnd, &point) != FALSE)
        input_value = MAKELPARAM(point.x, point.y);
    }
    const granit_window_input_native_event input_event{GRANIT_WINDOW_INPUT_BACKEND_WIN32,
                                                       message,
                                                       static_cast<std::uintptr_t>(word),
                                                       static_cast<std::intptr_t>(input_value),
                                                       0,
                                                       0,
                                                       0,
                                                       0,
                                                       0,
                                                       0};
    system->input_native_event(system->input_user_data, record->handle, &input_event);
  }

  switch (message) {
  case WM_CLOSE:
    enqueue_event(system, record->handle, GRANIT_WINDOW_EVENT_CLOSE_REQUESTED);
    return 0;
  case WM_SIZE: {
    record->framebuffer_width = static_cast<std::uint32_t>(LOWORD(value));
    record->framebuffer_height = static_cast<std::uint32_t>(HIWORD(value));
    record->width = static_cast<std::uint32_t>(std::lround(
        static_cast<float>(record->framebuffer_width) / record->content_scale_horizontal));
    record->height = static_cast<std::uint32_t>(std::lround(
        static_cast<float>(record->framebuffer_height) / record->content_scale_vertical));
    granit_window_event event = GRANIT_WINDOW_EVENT_INIT;
    event.type = GRANIT_WINDOW_EVENT_RESIZED;
    event.window = record->handle;
    event.timestamp_ns = timestamp_ns();
    event.data.resized.width = static_cast<std::uint32_t>(LOWORD(value));
    event.data.resized.height = static_cast<std::uint32_t>(HIWORD(value));
    system->events.push_back(event);
    return 0;
  }
  case WM_SETFOCUS:
  case WM_KILLFOCUS: {
    granit_window_event event = GRANIT_WINDOW_EVENT_INIT;
    event.type = GRANIT_WINDOW_EVENT_FOCUS_CHANGED;
    event.window = record->handle;
    event.timestamp_ns = timestamp_ns();
    event.data.focus.focused = message == WM_SETFOCUS ? UINT32_C(1) : UINT32_C(0);
    system->events.push_back(event);
    if (message == WM_KILLFOCUS && system->input_focus_lost != nullptr)
      system->input_focus_lost(system->input_user_data, record->handle);
    return 0;
  }
  case WM_DPICHANGED: {
    if (record->high_dpi) {
      const auto* rectangle = reinterpret_cast<const RECT*>(value);
      SetWindowPos(hwnd, nullptr, rectangle->left, rectangle->top,
                   rectangle->right - rectangle->left, rectangle->bottom - rectangle->top,
                   SWP_NOACTIVATE | SWP_NOZORDER);
    }
    RECT client{};
    GetClientRect(hwnd, &client);
    record->content_scale_horizontal = static_cast<float>(LOWORD(word)) / 96.0F;
    record->content_scale_vertical = static_cast<float>(HIWORD(word)) / 96.0F;
    record->framebuffer_width = static_cast<std::uint32_t>(client.right - client.left);
    record->framebuffer_height = static_cast<std::uint32_t>(client.bottom - client.top);
    record->width = static_cast<std::uint32_t>(std::lround(
        static_cast<float>(record->framebuffer_width) / record->content_scale_horizontal));
    record->height = static_cast<std::uint32_t>(std::lround(
        static_cast<float>(record->framebuffer_height) / record->content_scale_vertical));
    granit_window_event event = GRANIT_WINDOW_EVENT_INIT;
    event.type = GRANIT_WINDOW_EVENT_SCALE_CHANGED;
    event.window = record->handle;
    event.timestamp_ns = timestamp_ns();
    event.data.scale.horizontal = record->content_scale_horizontal;
    event.data.scale.vertical = record->content_scale_vertical;
    event.data.scale.width = record->framebuffer_width;
    event.data.scale.height = record->framebuffer_height;
    system->events.push_back(event);
    return 0;
  }
  case WM_UNICHAR:
    if (word == UNICODE_NOCHAR)
      return TRUE;
    return 0;
  default:
    return DefWindowProcW(hwnd, message, word, value);
  }
}

void ensure_window_class() {
  std::call_once(window_class_once, [] {
    WNDCLASSW window_class{};
    window_class.lpfnWndProc = window_proc;
    window_class.hInstance = GetModuleHandleW(nullptr);
    window_class.lpszClassName = window_class_name;
    window_class_registered =
        RegisterClassW(&window_class) != 0 || GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
  });
}

bool utf8_to_wide(const char* text, std::uint32_t length, std::wstring& output) {
  if (length == 0) {
    output = L"Granit";
    return true;
  }
  if (text == nullptr)
    return false;
  const auto required = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text,
                                            static_cast<int>(length), nullptr, 0);
  if (required <= 0)
    return false;
  output.resize(static_cast<std::size_t>(required));
  return MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, static_cast<int>(length),
                             output.data(), required) == required;
}

granit_result create_win32_system(granit_window_system* output) {
  try {
    auto system = std::make_shared<window_system_record>();
    system->owner_thread = std::this_thread::get_id();
    system->backend = GRANIT_WINDOW_BACKEND_WIN32;
    const auto handle = allocate_handle();
    std::lock_guard lock{registry_mutex};
    systems.emplace(handle, std::move(system));
    *output = handle;
    return GRANIT_SUCCESS;
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result destroy_win32_system(granit_window_system handle,
                                   const std::shared_ptr<window_system_record>& system) {
  std::vector<std::shared_ptr<window_record>> windows;
  windows.reserve(system->windows.size());
  for (auto& [unused, window] : system->windows) {
    static_cast<void>(unused);
    windows.push_back(std::move(window));
  }
  system->windows.clear();
  {
    std::lock_guard lock{registry_mutex};
    systems.erase(handle);
  }
  for (const auto& window : windows)
    if (window->window != nullptr)
      DestroyWindow(static_cast<HWND>(window->window));
  return GRANIT_SUCCESS;
}

granit_result poll_win32_event(const std::shared_ptr<window_system_record>& system,
                               granit_window_event* event) {
  MSG message{};
  while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE) != 0) {
    TranslateMessage(&message);
    DispatchMessageW(&message);
  }
  if (system->events.empty())
    return GRANIT_ERROR_NOT_READY;
  *event = system->events.front();
  system->events.pop_front();
  return GRANIT_SUCCESS;
}

granit_result create_win32_window(const std::shared_ptr<window_system_record>& system,
                                  const granit_window_desc* desc, granit_window* output) {
  try {
    ensure_window_class();
    if (!window_class_registered)
      return GRANIT_ERROR_BACKEND_UNAVAILABLE;
    std::wstring title;
    if (!utf8_to_wide(desc->title, desc->title_length, title))
      return GRANIT_ERROR_INVALID_ARGUMENT;
    auto record = std::make_shared<window_record>();
    record->handle = allocate_handle();
    record->system = system;
    record->instance = GetModuleHandleW(nullptr);
    record->high_dpi = (desc->flags & GRANIT_WINDOW_HIGH_DPI_BIT) != 0;
    DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    if ((desc->flags & GRANIT_WINDOW_RESIZABLE_BIT) != 0)
      style |= WS_THICKFRAME | WS_MAXIMIZEBOX;
    RECT rectangle{0, 0, static_cast<LONG>(desc->width), static_cast<LONG>(desc->height)};
    AdjustWindowRect(&rectangle, style, FALSE);
    const auto previous_dpi_context =
        record->high_dpi ? SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2)
                         : nullptr;
    record->window =
        CreateWindowExW(0, window_class_name, title.c_str(), style, CW_USEDEFAULT, CW_USEDEFAULT,
                        rectangle.right - rectangle.left, rectangle.bottom - rectangle.top, nullptr,
                        nullptr, static_cast<HINSTANCE>(record->instance), record.get());
    if (previous_dpi_context != nullptr)
      SetThreadDpiAwarenessContext(previous_dpi_context);
    if (record->window == nullptr)
      return GRANIT_ERROR_BACKEND_UNAVAILABLE;
    RECT client{};
    if (GetClientRect(static_cast<HWND>(record->window), &client) == FALSE) {
      DestroyWindow(static_cast<HWND>(record->window));
      return GRANIT_ERROR_BACKEND_UNAVAILABLE;
    }
    const auto dpi = GetDpiForWindow(static_cast<HWND>(record->window));
    record->content_scale_horizontal = static_cast<float>(dpi) / 96.0F;
    record->content_scale_vertical = record->content_scale_horizontal;
    record->framebuffer_width = static_cast<std::uint32_t>(client.right - client.left);
    record->framebuffer_height = static_cast<std::uint32_t>(client.bottom - client.top);
    record->width = static_cast<std::uint32_t>(std::lround(
        static_cast<float>(record->framebuffer_width) / record->content_scale_horizontal));
    record->height = static_cast<std::uint32_t>(std::lround(
        static_cast<float>(record->framebuffer_height) / record->content_scale_vertical));
    system->windows.emplace(record->handle, record);
    if ((desc->flags & GRANIT_WINDOW_VISIBLE_BIT) != 0)
      ShowWindow(static_cast<HWND>(record->window), SW_SHOW);
    *output = record->handle;
    return GRANIT_SUCCESS;
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result destroy_win32_window(const std::shared_ptr<window_system_record>& system,
                                   granit_window handle) {
  const auto found = system->windows.find(handle);
  auto window = std::move(found->second);
  system->windows.erase(found);
  return DestroyWindow(static_cast<HWND>(window->window)) != FALSE
             ? GRANIT_SUCCESS
             : GRANIT_ERROR_BACKEND_UNAVAILABLE;
}

granit_result get_win32_window(const std::shared_ptr<window_record>& window, void** instance,
                               void** native_window) {
  *instance = window->instance;
  *native_window = window->window;
  return GRANIT_SUCCESS;
}

} // namespace granit::window::detail
