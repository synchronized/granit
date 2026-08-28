// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

// 本文件包含 Win32 回调实现，并由 Window Registry 的内部命名空间引入。

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
    granit_window_event event = GRANIT_WINDOW_EVENT_INIT;
    event.type = GRANIT_WINDOW_EVENT_SCALE_CHANGED;
    event.window = record->handle;
    event.timestamp_ns = timestamp_ns();
    event.data.scale.horizontal = static_cast<float>(LOWORD(word)) / 96.0F;
    event.data.scale.vertical = static_cast<float>(HIWORD(word)) / 96.0F;
    event.data.scale.width = static_cast<std::uint32_t>(client.right - client.left);
    event.data.scale.height = static_cast<std::uint32_t>(client.bottom - client.top);
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
