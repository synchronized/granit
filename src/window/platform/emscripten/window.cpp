// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#if defined(__EMSCRIPTEN__)

#include "window/platform/backend.h"

#include <emscripten/html5.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <memory>
#include <mutex>
#include <new>
#include <thread>
#include <vector>

namespace granit::window::detail {
namespace {

constexpr char canvas_selector[] = "#canvas";
std::weak_ptr<window_record> active_window;

std::uint32_t dom_modifiers(bool control, bool shift, bool alt, bool meta) noexcept {
  std::uint32_t result = 0;
  if (control)
    result |= GRANIT_MODIFIER_LEFT_CONTROL_BIT;
  if (shift)
    result |= GRANIT_MODIFIER_LEFT_SHIFT_BIT;
  if (alt)
    result |= GRANIT_MODIFIER_LEFT_ALT_BIT;
  if (meta)
    result |= GRANIT_MODIFIER_LEFT_SUPER_BIT;
  return result;
}

bool valid_dimension(double value) noexcept {
  return std::isfinite(value) && value >= 1.0 &&
         value <= static_cast<double>(std::numeric_limits<int>::max());
}

void enqueue_focus_event(const std::shared_ptr<window_system_record>& system,
                         const window_record& window, bool focused) {
  granit_window_event event = GRANIT_WINDOW_EVENT_INIT;
  event.type = GRANIT_WINDOW_EVENT_FOCUS_CHANGED;
  event.window = window.handle;
  event.timestamp_ns = timestamp_ns();
  event.data.focus.focused = focused ? UINT32_C(1) : UINT32_C(0);
  system->events.push_back(event);
}

granit_result refresh_geometry(window_record& window, bool emit_events) {
  double css_width = 0.0;
  double css_height = 0.0;
  if (emscripten_get_element_css_size(canvas_selector, &css_width, &css_height) !=
          EMSCRIPTEN_RESULT_SUCCESS ||
      !valid_dimension(css_width) || !valid_dimension(css_height)) {
    return GRANIT_ERROR_SURFACE_LOST;
  }

  const auto logical_width = static_cast<std::uint32_t>(std::lround(css_width));
  const auto logical_height = static_cast<std::uint32_t>(std::lround(css_height));
  int current_width = 0;
  int current_height = 0;
  if (emscripten_get_canvas_element_size(canvas_selector, &current_width, &current_height) !=
          EMSCRIPTEN_RESULT_SUCCESS ||
      current_width <= 0 || current_height <= 0) {
    return GRANIT_ERROR_SURFACE_LOST;
  }
  const auto framebuffer_width = static_cast<std::uint32_t>(current_width);
  const auto framebuffer_height = static_cast<std::uint32_t>(current_height);
  const auto horizontal_scale = static_cast<float>(static_cast<double>(current_width) / css_width);
  const auto vertical_scale = static_cast<float>(static_cast<double>(current_height) / css_height);

  const bool size_changed = window.width != logical_width || window.height != logical_height ||
                            window.framebuffer_width != framebuffer_width ||
                            window.framebuffer_height != framebuffer_height;
  const bool scale_changed = window.content_scale_horizontal != horizontal_scale ||
                             window.content_scale_vertical != vertical_scale;
  window.width = logical_width;
  window.height = logical_height;
  window.framebuffer_width = framebuffer_width;
  window.framebuffer_height = framebuffer_height;
  window.content_scale_horizontal = horizontal_scale;
  window.content_scale_vertical = vertical_scale;

  const auto system = window.system.lock();
  if (!emit_events || !system)
    return GRANIT_SUCCESS;
  if (scale_changed) {
    granit_window_event event = GRANIT_WINDOW_EVENT_INIT;
    event.type = GRANIT_WINDOW_EVENT_SCALE_CHANGED;
    event.window = window.handle;
    event.timestamp_ns = timestamp_ns();
    event.data.scale.horizontal = window.content_scale_horizontal;
    event.data.scale.vertical = window.content_scale_vertical;
    system->events.push_back(event);
  }
  if (size_changed) {
    granit_window_event event = GRANIT_WINDOW_EVENT_INIT;
    event.type = GRANIT_WINDOW_EVENT_RESIZED;
    event.window = window.handle;
    event.timestamp_ns = timestamp_ns();
    event.data.resized.width = window.width;
    event.data.resized.height = window.height;
    system->events.push_back(event);
  }
  return GRANIT_SUCCESS;
}

EM_BOOL key_callback(int event_type, const EmscriptenKeyboardEvent* source, void* user_data) {
  auto* window = static_cast<window_record*>(user_data);
  const auto system = window->system.lock();
  if (!system)
    return EM_FALSE;
  granit_window_input_native_event event{};
  event.backend = GRANIT_WINDOW_INPUT_BACKEND_EMSCRIPTEN;
  event.type = event_type == EMSCRIPTEN_EVENT_KEYDOWN ? GRANIT_WINDOW_INPUT_EMSCRIPTEN_KEY_DOWN
                                                      : GRANIT_WINDOW_INPUT_EMSCRIPTEN_KEY_UP;
  event.word = reinterpret_cast<std::uintptr_t>(source->code);
  event.value = reinterpret_cast<std::intptr_t>(source->key);
  event.state = dom_modifiers(source->ctrlKey, source->shiftKey, source->altKey, source->metaKey);
  event.data0 = source->repeat ? UINT32_C(1) : UINT32_C(0);
  handle_native_input(*system, window->handle, event);
  return EM_FALSE;
}

EM_BOOL text_callback(int, const EmscriptenKeyboardEvent* source, void* user_data) {
  auto* window = static_cast<window_record*>(user_data);
  const auto system = window->system.lock();
  if (!system)
    return EM_FALSE;
  const char* text = source->charValue[0] != '\0' ? source->charValue : source->key;
  const auto length = std::strlen(text);
  if (length == 0)
    return EM_FALSE;
  granit_window_input_native_event event{};
  event.backend = GRANIT_WINDOW_INPUT_BACKEND_EMSCRIPTEN;
  event.type = GRANIT_WINDOW_INPUT_EMSCRIPTEN_TEXT;
  event.word = reinterpret_cast<std::uintptr_t>(text);
  event.value = static_cast<std::intptr_t>(length);
  handle_native_input(*system, window->handle, event);
  return EM_TRUE;
}

EM_BOOL mouse_callback(int event_type, const EmscriptenMouseEvent* source, void* user_data) {
  auto* window = static_cast<window_record*>(user_data);
  const auto system = window->system.lock();
  if (!system)
    return EM_FALSE;
  granit_window_input_native_event event{};
  event.backend = GRANIT_WINDOW_INPUT_BACKEND_EMSCRIPTEN;
  switch (event_type) {
  case EMSCRIPTEN_EVENT_MOUSEMOVE:
    event.type = GRANIT_WINDOW_INPUT_EMSCRIPTEN_MOUSE_MOVE;
    break;
  case EMSCRIPTEN_EVENT_MOUSEDOWN:
    event.type = GRANIT_WINDOW_INPUT_EMSCRIPTEN_MOUSE_DOWN;
    break;
  case EMSCRIPTEN_EVENT_MOUSEUP:
    event.type = GRANIT_WINDOW_INPUT_EMSCRIPTEN_MOUSE_UP;
    break;
  case EMSCRIPTEN_EVENT_MOUSEENTER:
    event.type = GRANIT_WINDOW_INPUT_EMSCRIPTEN_MOUSE_ENTER;
    break;
  case EMSCRIPTEN_EVENT_MOUSELEAVE:
    event.type = GRANIT_WINDOW_INPUT_EMSCRIPTEN_MOUSE_LEAVE;
    break;
  default:
    return EM_FALSE;
  }
  event.word = source->button;
  event.x = source->targetX;
  event.y = source->targetY;
  event.detail = source->buttons;
  handle_native_input(*system, window->handle, event);
  return EM_TRUE;
}

EM_BOOL wheel_callback(int, const EmscriptenWheelEvent* source, void* user_data) {
  auto* window = static_cast<window_record*>(user_data);
  const auto system = window->system.lock();
  if (!system)
    return EM_FALSE;
  double divisor = 1.0;
  if (source->deltaMode == DOM_DELTA_PIXEL)
    divisor = 100.0;
  else if (source->deltaMode == DOM_DELTA_LINE)
    divisor = 3.0;
  granit_window_input_native_event event{};
  event.backend = GRANIT_WINDOW_INPUT_BACKEND_EMSCRIPTEN;
  event.type = GRANIT_WINDOW_INPUT_EMSCRIPTEN_WHEEL;
  event.x = source->mouse.targetX;
  event.y = source->mouse.targetY;
  event.detail = source->mouse.buttons;
  event.data0 = static_cast<std::uint32_t>(
      static_cast<std::int32_t>(std::lround(-source->deltaX / divisor * 256.0)));
  event.data1 = static_cast<std::uint32_t>(
      static_cast<std::int32_t>(std::lround(-source->deltaY / divisor * 256.0)));
  handle_native_input(*system, window->handle, event);
  return EM_TRUE;
}

EM_BOOL focus_callback(int event_type, const EmscriptenFocusEvent*, void* user_data) {
  auto* window = static_cast<window_record*>(user_data);
  const auto system = window->system.lock();
  if (!system)
    return EM_FALSE;
  const bool focused = event_type == EMSCRIPTEN_EVENT_FOCUS;
  if (window->focused == focused)
    return EM_FALSE;
  window->focused = focused;
  if (!focused)
    clear_input_focus(*system, window->handle);
  enqueue_focus_event(system, *window, focused);
  return EM_FALSE;
}

void unbind_callbacks() noexcept {
  static_cast<void>(
      emscripten_set_keydown_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, nullptr, false, nullptr));
  static_cast<void>(
      emscripten_set_keyup_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, nullptr, false, nullptr));
  static_cast<void>(
      emscripten_set_keypress_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, nullptr, false, nullptr));
  static_cast<void>(
      emscripten_set_focus_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, nullptr, false, nullptr));
  static_cast<void>(
      emscripten_set_blur_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, nullptr, false, nullptr));
  static_cast<void>(emscripten_set_mousedown_callback(canvas_selector, nullptr, false, nullptr));
  static_cast<void>(emscripten_set_mouseup_callback(canvas_selector, nullptr, false, nullptr));
  static_cast<void>(emscripten_set_mousemove_callback(canvas_selector, nullptr, false, nullptr));
  static_cast<void>(emscripten_set_mouseenter_callback(canvas_selector, nullptr, false, nullptr));
  static_cast<void>(emscripten_set_mouseleave_callback(canvas_selector, nullptr, false, nullptr));
  static_cast<void>(emscripten_set_wheel_callback(canvas_selector, nullptr, false, nullptr));
}

granit_result bind_callbacks(window_record& window) {
  const auto failed = [&](EMSCRIPTEN_RESULT result) { return result != EMSCRIPTEN_RESULT_SUCCESS; };
  if (failed(emscripten_set_keydown_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, &window, false,
                                             key_callback)) ||
      failed(emscripten_set_keyup_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, &window, false,
                                           key_callback)) ||
      failed(emscripten_set_keypress_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, &window, false,
                                              text_callback)) ||
      failed(emscripten_set_focus_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, &window, false,
                                           focus_callback)) ||
      failed(emscripten_set_blur_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, &window, false,
                                          focus_callback)) ||
      failed(emscripten_set_mousedown_callback(canvas_selector, &window, false, mouse_callback)) ||
      failed(emscripten_set_mouseup_callback(canvas_selector, &window, false, mouse_callback)) ||
      failed(emscripten_set_mousemove_callback(canvas_selector, &window, false, mouse_callback)) ||
      failed(emscripten_set_mouseenter_callback(canvas_selector, &window, false, mouse_callback)) ||
      failed(emscripten_set_mouseleave_callback(canvas_selector, &window, false, mouse_callback)) ||
      failed(emscripten_set_wheel_callback(canvas_selector, &window, false, wheel_callback))) {
    unbind_callbacks();
    return GRANIT_ERROR_INITIALIZATION_FAILED;
  }
  return GRANIT_SUCCESS;
}

} // namespace

granit_result create_emscripten_system(granit_window_system* output) {
  try {
    auto system = std::make_shared<window_system_record>();
    system->owner_thread = std::this_thread::get_id();
    system->backend = GRANIT_WINDOW_BACKEND_EMSCRIPTEN;
    const auto handle = allocate_handle();
    {
      std::lock_guard lock{registry_mutex};
      systems.emplace(handle, std::move(system));
    }
    *output = handle;
    return GRANIT_SUCCESS;
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result destroy_emscripten_system(granit_window_system handle,
                                        const std::shared_ptr<window_system_record>& system) {
  if (!system->windows.empty()) {
    unbind_callbacks();
    active_window.reset();
  }
  system->windows.clear();
  std::lock_guard lock{registry_mutex};
  systems.erase(handle);
  return GRANIT_SUCCESS;
}

granit_result process_emscripten_events(const std::shared_ptr<window_system_record>& system) {
  for (const auto& [unused, window] : system->windows) {
    static_cast<void>(unused);
    if (window != nullptr) {
      const auto result = refresh_geometry(*window, true);
      if (result != GRANIT_SUCCESS)
        return result;
    }
  }
  return GRANIT_SUCCESS;
}

granit_result create_emscripten_window(const std::shared_ptr<window_system_record>& system,
                                       const granit_window_desc* desc, granit_window* output) {
  if (!system->windows.empty() || !active_window.expired())
    return GRANIT_ERROR_RESOURCE_IN_USE;
  if (!valid_dimension(desc->width) || !valid_dimension(desc->height))
    return GRANIT_ERROR_INVALID_ARGUMENT;
  if (emscripten_set_element_css_size(canvas_selector, static_cast<double>(desc->width),
                                      static_cast<double>(desc->height)) !=
      EMSCRIPTEN_RESULT_SUCCESS) {
    return GRANIT_ERROR_BACKEND_UNAVAILABLE;
  }
  const auto scale = (desc->flags & GRANIT_WINDOW_HIGH_DPI_BIT) != 0
                         ? std::max(1.0, emscripten_get_device_pixel_ratio())
                         : 1.0;
  const auto framebuffer_width = std::round(static_cast<double>(desc->width) * scale);
  const auto framebuffer_height = std::round(static_cast<double>(desc->height) * scale);
  if (!valid_dimension(framebuffer_width) || !valid_dimension(framebuffer_height))
    return GRANIT_ERROR_INVALID_ARGUMENT;
  if (emscripten_set_canvas_element_size(canvas_selector, static_cast<int>(framebuffer_width),
                                         static_cast<int>(framebuffer_height)) !=
      EMSCRIPTEN_RESULT_SUCCESS) {
    return GRANIT_ERROR_BACKEND_UNAVAILABLE;
  }
  try {
    auto window = std::make_shared<window_record>();
    window->handle = allocate_handle();
    window->system = system;
    window->flags = desc->flags;
    const auto geometry_result = refresh_geometry(*window, false);
    if (geometry_result != GRANIT_SUCCESS)
      return geometry_result;
    const auto callback_result = bind_callbacks(*window);
    if (callback_result != GRANIT_SUCCESS)
      return callback_result;
    system->windows.emplace(window->handle, window);
    active_window = window;
    *output = window->handle;
    return GRANIT_SUCCESS;
  } catch (const std::bad_alloc&) {
    unbind_callbacks();
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    unbind_callbacks();
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result destroy_emscripten_window(const std::shared_ptr<window_system_record>& system,
                                        granit_window handle) {
  const auto found = system->windows.find(handle);
  if (found == system->windows.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  unbind_callbacks();
  active_window.reset();
  system->windows.erase(found);
  return GRANIT_SUCCESS;
}

granit_result get_native_emscripten(granit_window_native_emscripten& output) {
  output.canvas_selector = canvas_selector;
  output.canvas_selector_length = static_cast<std::uint32_t>(sizeof(canvas_selector) - 1);
  return GRANIT_SUCCESS;
}

} // namespace granit::window::detail

#endif
