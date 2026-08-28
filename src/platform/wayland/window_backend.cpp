// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "platform/window/window_backend_internal.h"

#include <poll.h>
#include <sys/mman.h>
#include <unistd.h>
#include <wayland-client.h>
#include <xdg-shell-client-protocol.h>

#include <algorithm>
#include <cstring>

namespace granit::window::detail {

granit_window wayland_window_handle(const window_system_record& system, wl_surface* surface) {
  for (const auto& [handle, window] : system.windows)
    if (window->wayland_surface == surface)
      return handle;
  return GRANIT_NULL_HANDLE;
}

void dispatch_wayland_input(window_system_record& system, granit_window window, std::uint32_t type,
                            std::uintptr_t word = 0, std::intptr_t value = 0, std::int32_t x = 0,
                            std::int32_t y = 0, std::uint32_t state = 0, std::uint32_t detail = 0,
                            std::uint32_t data0 = 0, std::uint32_t data1 = 0) {
  if (system.input_native_event == nullptr)
    return;
  const granit_window_input_native_event event{
      GRANIT_WINDOW_INPUT_BACKEND_WAYLAND, type, word, value, x, y, state, detail, data0, data1};
  system.input_native_event(system.input_user_data, window, &event);
}

void wayland_registry_global(void* data, wl_registry* registry, std::uint32_t name,
                             const char* interface, std::uint32_t version) {
  auto& system = *static_cast<window_system_record*>(data);
  if (std::strcmp(interface, wl_compositor_interface.name) == 0) {
    system.compositor = static_cast<wl_compositor*>(
        wl_registry_bind(registry, name, &wl_compositor_interface, std::min(version, UINT32_C(4))));
  } else if (std::strcmp(interface, xdg_wm_base_interface.name) == 0) {
    system.wm_base = static_cast<xdg_wm_base*>(
        wl_registry_bind(registry, name, &xdg_wm_base_interface, UINT32_C(1)));
  } else if (std::strcmp(interface, wl_seat_interface.name) == 0) {
    system.seat_name = name;
    system.seat_version = std::min(version, UINT32_C(5));
  }
}

void wayland_registry_remove(void*, wl_registry*, std::uint32_t) {}

extern const wl_keyboard_listener wayland_keyboard_listener;
extern const wl_pointer_listener wayland_pointer_listener;

void wayland_keyboard_keymap(void* data, wl_keyboard*, std::uint32_t format, std::int32_t fd,
                             std::uint32_t size) {
  auto& system = *static_cast<window_system_record*>(data);
  if (format == WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1 && size != 0) {
    void* mapping = mmap(nullptr, size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (mapping != MAP_FAILED) {
      dispatch_wayland_input(system, GRANIT_NULL_HANDLE, GRANIT_WINDOW_INPUT_WAYLAND_KEYMAP,
                             reinterpret_cast<std::uintptr_t>(mapping), size);
      munmap(mapping, size);
    }
  }
  close(fd);
}

void wayland_keyboard_enter(void* data, wl_keyboard*, std::uint32_t, wl_surface* surface,
                            wl_array*) {
  auto& system = *static_cast<window_system_record*>(data);
  system.keyboard_window = wayland_window_handle(system, surface);
}

void wayland_keyboard_leave(void* data, wl_keyboard*, std::uint32_t, wl_surface*) {
  auto& system = *static_cast<window_system_record*>(data);
  if (system.input_focus_lost != nullptr && system.keyboard_window != GRANIT_NULL_HANDLE)
    system.input_focus_lost(system.input_user_data, system.keyboard_window);
  system.keyboard_window = GRANIT_NULL_HANDLE;
}

void wayland_keyboard_key(void* data, wl_keyboard*, std::uint32_t, std::uint32_t, std::uint32_t key,
                          std::uint32_t state) {
  auto& system = *static_cast<window_system_record*>(data);
  dispatch_wayland_input(system, system.keyboard_window, GRANIT_WINDOW_INPUT_WAYLAND_KEY, 0, 0, 0,
                         0, state == WL_KEYBOARD_KEY_STATE_PRESSED ? UINT32_C(1) : UINT32_C(0),
                         key);
}

void wayland_keyboard_modifiers(void* data, wl_keyboard*, std::uint32_t, std::uint32_t depressed,
                                std::uint32_t latched, std::uint32_t locked, std::uint32_t group) {
  auto& system = *static_cast<window_system_record*>(data);
  dispatch_wayland_input(system, system.keyboard_window, GRANIT_WINDOW_INPUT_WAYLAND_MODIFIERS, 0,
                         0, 0, 0, depressed, latched, locked, group);
}

void wayland_keyboard_repeat_info(void*, wl_keyboard*, std::int32_t, std::int32_t) {}

void wayland_pointer_enter(void* data, wl_pointer*, std::uint32_t, wl_surface* surface,
                           wl_fixed_t x, wl_fixed_t y) {
  auto& system = *static_cast<window_system_record*>(data);
  system.pointer_window = wayland_window_handle(system, surface);
  dispatch_wayland_input(system, system.pointer_window, GRANIT_WINDOW_INPUT_WAYLAND_POINTER_ENTER,
                         0, 0, x, y);
}

void wayland_pointer_leave(void* data, wl_pointer*, std::uint32_t, wl_surface*) {
  auto& system = *static_cast<window_system_record*>(data);
  dispatch_wayland_input(system, system.pointer_window, GRANIT_WINDOW_INPUT_WAYLAND_POINTER_LEAVE);
  system.pointer_window = GRANIT_NULL_HANDLE;
}

void wayland_pointer_motion(void* data, wl_pointer*, std::uint32_t, wl_fixed_t x, wl_fixed_t y) {
  auto& system = *static_cast<window_system_record*>(data);
  dispatch_wayland_input(system, system.pointer_window, GRANIT_WINDOW_INPUT_WAYLAND_POINTER_MOTION,
                         0, 0, x, y);
}

void wayland_pointer_button(void* data, wl_pointer*, std::uint32_t, std::uint32_t,
                            std::uint32_t button, std::uint32_t state) {
  auto& system = *static_cast<window_system_record*>(data);
  dispatch_wayland_input(
      system, system.pointer_window, GRANIT_WINDOW_INPUT_WAYLAND_POINTER_BUTTON, 0, 0, 0, 0,
      state == WL_POINTER_BUTTON_STATE_PRESSED ? UINT32_C(1) : UINT32_C(0), button);
}

void wayland_pointer_axis(void* data, wl_pointer*, std::uint32_t, std::uint32_t axis,
                          wl_fixed_t value) {
  auto& system = *static_cast<window_system_record*>(data);
  dispatch_wayland_input(system, system.pointer_window, GRANIT_WINDOW_INPUT_WAYLAND_POINTER_AXIS, 0,
                         value, 0, 0, 0, axis);
}

void wayland_seat_capabilities(void* data, wl_seat* seat, std::uint32_t capabilities) {
  auto& system = *static_cast<window_system_record*>(data);
  if ((capabilities & WL_SEAT_CAPABILITY_KEYBOARD) != 0 && system.keyboard == nullptr) {
    system.keyboard = wl_seat_get_keyboard(seat);
    wl_keyboard_add_listener(system.keyboard, &wayland_keyboard_listener, &system);
  } else if ((capabilities & WL_SEAT_CAPABILITY_KEYBOARD) == 0 && system.keyboard != nullptr) {
    wl_keyboard_destroy(system.keyboard);
    system.keyboard = nullptr;
  }
  if ((capabilities & WL_SEAT_CAPABILITY_POINTER) != 0 && system.pointer == nullptr) {
    system.pointer = wl_seat_get_pointer(seat);
    wl_pointer_add_listener(system.pointer, &wayland_pointer_listener, &system);
  } else if ((capabilities & WL_SEAT_CAPABILITY_POINTER) == 0 && system.pointer != nullptr) {
    wl_pointer_destroy(system.pointer);
    system.pointer = nullptr;
  }
}

void wayland_seat_name(void*, wl_seat*, const char*) {}

void wayland_wm_base_ping(void*, xdg_wm_base* wm_base, std::uint32_t serial) {
  xdg_wm_base_pong(wm_base, serial);
}

void wayland_surface_configure(void* data, xdg_surface* surface, std::uint32_t serial) {
  auto& window = *static_cast<window_record*>(data);
  xdg_surface_ack_configure(surface, serial);
  window.configured = true;
}

void wayland_toplevel_configure(void* data, xdg_toplevel*, std::int32_t width, std::int32_t height,
                                wl_array* states) {
  auto& window = *static_cast<window_record*>(data);
  const auto system = window.system.lock();
  if (!system)
    return;
  if (width > 0 && height > 0 &&
      (window.configured_width != static_cast<std::uint32_t>(width) ||
       window.configured_height != static_cast<std::uint32_t>(height))) {
    window.configured_width = static_cast<std::uint32_t>(width);
    window.configured_height = static_cast<std::uint32_t>(height);
    granit_window_event event = GRANIT_WINDOW_EVENT_INIT;
    event.type = GRANIT_WINDOW_EVENT_RESIZED;
    event.window = window.handle;
    event.timestamp_ns = timestamp_ns();
    event.data.resized.width = window.configured_width;
    event.data.resized.height = window.configured_height;
    system->events.push_back(event);
  }
  bool focused = false;
  const auto* state = static_cast<const std::uint32_t*>(states->data);
  const auto state_count = states->size / sizeof(std::uint32_t);
  for (std::size_t index = 0; index < state_count; ++index)
    focused = focused || state[index] == XDG_TOPLEVEL_STATE_ACTIVATED;
  if (focused != window.focused) {
    window.focused = focused;
    granit_window_event event = GRANIT_WINDOW_EVENT_INIT;
    event.type = GRANIT_WINDOW_EVENT_FOCUS_CHANGED;
    event.window = window.handle;
    event.timestamp_ns = timestamp_ns();
    event.data.focus.focused = focused ? UINT32_C(1) : UINT32_C(0);
    system->events.push_back(event);
  }
}

void wayland_toplevel_close(void* data, xdg_toplevel*) {
  auto& window = *static_cast<window_record*>(data);
  if (const auto system = window.system.lock())
    enqueue_event(system, window.handle, GRANIT_WINDOW_EVENT_CLOSE_REQUESTED);
}

constexpr wl_registry_listener wayland_registry_listener{.global = wayland_registry_global,
                                                         .global_remove = wayland_registry_remove};
const wl_keyboard_listener wayland_keyboard_listener = [] {
  wl_keyboard_listener listener{};
  listener.keymap = wayland_keyboard_keymap;
  listener.enter = wayland_keyboard_enter;
  listener.leave = wayland_keyboard_leave;
  listener.key = wayland_keyboard_key;
  listener.modifiers = wayland_keyboard_modifiers;
  listener.repeat_info = wayland_keyboard_repeat_info;
  return listener;
}();
const wl_pointer_listener wayland_pointer_listener = [] {
  wl_pointer_listener listener{};
  listener.enter = wayland_pointer_enter;
  listener.leave = wayland_pointer_leave;
  listener.motion = wayland_pointer_motion;
  listener.button = wayland_pointer_button;
  listener.axis = wayland_pointer_axis;
  return listener;
}();
const wl_seat_listener wayland_seat_listener = [] {
  wl_seat_listener listener{};
  listener.capabilities = wayland_seat_capabilities;
  listener.name = wayland_seat_name;
  return listener;
}();
constexpr xdg_wm_base_listener wayland_wm_base_listener{.ping = wayland_wm_base_ping};
constexpr xdg_surface_listener wayland_surface_listener{.configure = wayland_surface_configure};
const xdg_toplevel_listener wayland_toplevel_listener = [] {
  xdg_toplevel_listener listener{};
  listener.configure = wayland_toplevel_configure;
  listener.close = wayland_toplevel_close;
  return listener;
}();

void destroy_wayland_window(window_record& window) {
  if (window.wayland_toplevel != nullptr)
    xdg_toplevel_destroy(window.wayland_toplevel);
  if (window.wayland_xdg_surface != nullptr)
    xdg_surface_destroy(window.wayland_xdg_surface);
  if (window.wayland_surface != nullptr)
    wl_surface_destroy(window.wayland_surface);
}

void destroy_wayland_input(window_system_record& system) {
  if (system.pointer != nullptr)
    wl_pointer_destroy(system.pointer);
  if (system.keyboard != nullptr)
    wl_keyboard_destroy(system.keyboard);
  if (system.seat != nullptr)
    wl_seat_destroy(system.seat);
  system.pointer = nullptr;
  system.keyboard = nullptr;
  system.seat = nullptr;
  system.pointer_window = GRANIT_NULL_HANDLE;
  system.keyboard_window = GRANIT_NULL_HANDLE;
}

granit_result initialize_wayland_input(window_system_record& system) {
  if (system.seat_name == 0)
    return GRANIT_ERROR_BACKEND_UNAVAILABLE;
  system.seat = static_cast<wl_seat*>(
      wl_registry_bind(system.registry, system.seat_name, &wl_seat_interface, system.seat_version));
  if (system.seat == nullptr)
    return GRANIT_ERROR_BACKEND_UNAVAILABLE;
  wl_seat_add_listener(system.seat, &wayland_seat_listener, &system);
  if (wl_display_roundtrip(system.display) < 0) {
    destroy_wayland_input(system);
    return GRANIT_ERROR_BACKEND_UNAVAILABLE;
  }
  return GRANIT_SUCCESS;
}

void destroy_wayland_system(window_system_record& system) {
  destroy_wayland_input(system);
  if (system.wm_base != nullptr)
    xdg_wm_base_destroy(system.wm_base);
  if (system.compositor != nullptr)
    wl_compositor_destroy(system.compositor);
  if (system.registry != nullptr)
    wl_registry_destroy(system.registry);
  if (system.display != nullptr)
    wl_display_disconnect(system.display);
}

granit_result initialize_wayland_system(window_system_record& system) {
  system.display = wl_display_connect(nullptr);
  if (system.display == nullptr)
    return GRANIT_ERROR_BACKEND_UNAVAILABLE;
  system.registry = wl_display_get_registry(system.display);
  if (system.registry == nullptr) {
    destroy_wayland_system(system);
    return GRANIT_ERROR_BACKEND_UNAVAILABLE;
  }
  wl_registry_add_listener(system.registry, &wayland_registry_listener, &system);
  if (wl_display_roundtrip(system.display) < 0 || system.compositor == nullptr ||
      system.wm_base == nullptr) {
    destroy_wayland_system(system);
    return GRANIT_ERROR_BACKEND_UNAVAILABLE;
  }
  xdg_wm_base_add_listener(system.wm_base, &wayland_wm_base_listener, &system);
  return GRANIT_SUCCESS;
}

granit_result pump_wayland_events(window_system_record& system) {
  while (wl_display_prepare_read(system.display) != 0)
    if (wl_display_dispatch_pending(system.display) < 0)
      return GRANIT_ERROR_BACKEND_UNAVAILABLE;
  static_cast<void>(wl_display_flush(system.display));
  pollfd descriptor{.fd = wl_display_get_fd(system.display), .events = POLLIN, .revents = 0};
  const auto poll_result = poll(&descriptor, 1, 0);
  if (poll_result > 0 && (descriptor.revents & POLLIN) != 0) {
    if (wl_display_read_events(system.display) < 0)
      return GRANIT_ERROR_BACKEND_UNAVAILABLE;
  } else {
    wl_display_cancel_read(system.display);
  }
  return wl_display_dispatch_pending(system.display) < 0 ? GRANIT_ERROR_BACKEND_UNAVAILABLE
                                                         : GRANIT_SUCCESS;
}

} // namespace granit::window::detail
