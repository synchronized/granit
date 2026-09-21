// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "window/input/platform_adapter.h"

#if defined(GRANIT_WINDOW_HAS_WAYLAND_INPUT)
#include "window/platform/wayland/input.h"
#endif
#if defined(_WIN32)
#include "window/platform/win32/input.h"
#endif
#if defined(GRANIT_WINDOW_HAS_XCB)
#include "window/platform/xcb/input.h"
#endif
#if defined(__EMSCRIPTEN__)
#include "window/platform/emscripten/input.h"
#endif

#include <cstddef>

namespace granit::input::detail {

struct platform_input_adapter::implementation {
#if defined(_WIN32)
  win32_input_adapter win32;
#endif
#if defined(GRANIT_WINDOW_HAS_WAYLAND_INPUT)
  wayland_input_adapter wayland;
#endif
#if defined(__EMSCRIPTEN__)
  emscripten_input_adapter emscripten;
#endif
};

platform_input_adapter::platform_input_adapter()
    : implementation_(std::make_unique<implementation>()) {}

platform_input_adapter::~platform_input_adapter() = default;

void platform_input_adapter::handle(granit_window window,
                                    const granit_window_input_native_event& event,
                                    const platform_input_sink& sink) {
#if defined(_WIN32)
  if (event.backend == GRANIT_WINDOW_INPUT_BACKEND_WIN32) {
    const win32_input_sink native_sink{sink.user_data, sink.keyboard, sink.pointer, sink.event,
                                       sink.text};
    implementation_->win32.handle(window, event.type, event.word, event.value, native_sink);
    return;
  }
#endif
#if defined(GRANIT_WINDOW_HAS_XCB)
  if (event.backend == GRANIT_WINDOW_INPUT_BACKEND_XCB) {
    const xcb_input_sink native_sink{sink.user_data, sink.keyboard, sink.pointer, sink.event};
    handle_xcb_input(window, {event.type, event.x, event.y, event.state, event.detail},
                     native_sink);
    return;
  }
#endif
#if defined(GRANIT_WINDOW_HAS_WAYLAND_INPUT)
  if (event.backend == GRANIT_WINDOW_INPUT_BACKEND_WAYLAND) {
    const wayland_input_sink native_sink{sink.user_data, sink.keyboard, sink.pointer, sink.event,
                                         sink.text};
    switch (event.type) {
    case GRANIT_WINDOW_INPUT_WAYLAND_KEYMAP:
      static_cast<void>(implementation_->wayland.set_keymap(
          reinterpret_cast<const char*>(event.word), static_cast<std::size_t>(event.value)));
      break;
    case GRANIT_WINDOW_INPUT_WAYLAND_KEY:
      implementation_->wayland.key(window, event.detail, event.state != 0, native_sink);
      break;
    case GRANIT_WINDOW_INPUT_WAYLAND_MODIFIERS:
      implementation_->wayland.modifiers(window, event.state, event.detail, event.data0,
                                         event.data1, native_sink);
      break;
    case GRANIT_WINDOW_INPUT_WAYLAND_POINTER_ENTER:
      implementation_->wayland.pointer_enter(window, static_cast<float>(event.x) / 256.0F,
                                             static_cast<float>(event.y) / 256.0F, native_sink);
      break;
    case GRANIT_WINDOW_INPUT_WAYLAND_POINTER_LEAVE:
      implementation_->wayland.pointer_leave(window, native_sink);
      break;
    case GRANIT_WINDOW_INPUT_WAYLAND_POINTER_MOTION:
      implementation_->wayland.pointer_motion(window, static_cast<float>(event.x) / 256.0F,
                                              static_cast<float>(event.y) / 256.0F, native_sink);
      break;
    case GRANIT_WINDOW_INPUT_WAYLAND_POINTER_BUTTON:
      implementation_->wayland.pointer_button(window, event.detail, event.state != 0, native_sink);
      break;
    case GRANIT_WINDOW_INPUT_WAYLAND_POINTER_AXIS:
      implementation_->wayland.pointer_axis(window, event.detail,
                                            static_cast<float>(event.value) / 256.0F, native_sink);
      break;
    default:
      break;
    }
    return;
  }
#endif
#if defined(__EMSCRIPTEN__)
  if (event.backend == GRANIT_WINDOW_INPUT_BACKEND_EMSCRIPTEN) {
    const emscripten_input_sink native_sink{sink.user_data, sink.keyboard, sink.pointer, sink.event,
                                            sink.text};
    implementation_->emscripten.handle(window, event, native_sink);
    return;
  }
#endif

  static_cast<void>(window);
  static_cast<void>(event);
  static_cast<void>(sink);
}

void platform_input_adapter::clear_window(granit_window window) noexcept {
#if defined(_WIN32)
  implementation_->win32.clear_window(window);
#endif
#if defined(GRANIT_WINDOW_HAS_WAYLAND_INPUT)
  implementation_->wayland.clear_window(window);
#endif
#if defined(__EMSCRIPTEN__)
  implementation_->emscripten.clear_window(window);
#endif
#if !defined(_WIN32) && !defined(GRANIT_WINDOW_HAS_WAYLAND_INPUT)
  static_cast<void>(window);
#endif
}

} // namespace granit::input::detail
