// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/window/window.h>

#include "platform/window/input_bridge.hpp"

#if defined(_WIN32)
#include <windows.h>
#include <windowsx.h>
#else
#if defined(GRANIT_WINDOW_HAS_XCB)
#include <xcb/xcb.h>
#endif
#if defined(GRANIT_WINDOW_HAS_WAYLAND)
#include <poll.h>
#include <sys/mman.h>
#include <unistd.h>
#include <wayland-client.h>
#include <xdg-shell-client-protocol.h>
#endif
#endif

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {

struct window_system_record;

struct window_record {
  granit_window handle{};
  std::weak_ptr<window_system_record> system;
#if defined(_WIN32)
  HINSTANCE instance{};
  HWND window{};
  bool high_dpi{};
  bool pointer_tracking{};
#endif
#if defined(GRANIT_WINDOW_HAS_XCB)
  xcb_window_t window{XCB_WINDOW_NONE};
#endif
#if defined(GRANIT_WINDOW_HAS_WAYLAND)
  wl_surface* wayland_surface{};
  xdg_surface* wayland_xdg_surface{};
  xdg_toplevel* wayland_toplevel{};
  std::uint32_t configured_width{};
  std::uint32_t configured_height{};
  bool configured{};
  bool focused{};
#endif
};

struct window_system_record {
  std::thread::id owner_thread;
  std::uint32_t backend{};
  std::unordered_map<granit_window, std::shared_ptr<window_record>> windows;
  std::deque<granit_window_event> events;
  void* input_user_data{};
  granit_window_input_window_callback input_window_destroyed{};
  granit_window_input_window_callback input_focus_lost{};
  granit_window_input_native_event_callback input_native_event{};
#if defined(GRANIT_WINDOW_HAS_XCB)
  xcb_connection_t* connection{};
  xcb_screen_t* screen{};
  xcb_atom_t wm_protocols{XCB_ATOM_NONE};
  xcb_atom_t wm_delete_window{XCB_ATOM_NONE};
  xcb_atom_t wm_size_hints{XCB_ATOM_NONE};
  xcb_atom_t net_wm_name{XCB_ATOM_NONE};
  xcb_atom_t utf8_string{XCB_ATOM_NONE};
#endif
#if defined(GRANIT_WINDOW_HAS_WAYLAND)
  wl_display* display{};
  wl_registry* registry{};
  wl_compositor* compositor{};
  xdg_wm_base* wm_base{};
  std::uint32_t seat_name{};
  std::uint32_t seat_version{};
  wl_seat* seat{};
  wl_keyboard* keyboard{};
  wl_pointer* pointer{};
  granit_window keyboard_window{};
  granit_window pointer_window{};
#endif
};

std::mutex registry_mutex;
std::unordered_map<granit_window_system, std::shared_ptr<window_system_record>> systems;
std::atomic<std::uint64_t> next_handle{1};

std::uint64_t allocate_handle() noexcept {
  return next_handle.fetch_add(1, std::memory_order_relaxed);
}

std::uint64_t timestamp_ns() noexcept {
  return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
                                        std::chrono::steady_clock::now().time_since_epoch())
                                        .count());
}

std::shared_ptr<window_system_record> acquire_system(granit_window_system handle) {
  std::lock_guard lock{registry_mutex};
  const auto found = systems.find(handle);
  return found == systems.end() ? nullptr : found->second;
}

bool on_owner_thread(const window_system_record& system) noexcept {
  return system.owner_thread == std::this_thread::get_id();
}

void enqueue_event(const std::shared_ptr<window_system_record>& system, granit_window window,
                   std::uint32_t type) {
  granit_window_event event = GRANIT_WINDOW_EVENT_INIT;
  event.type = type;
  event.window = window;
  event.timestamp_ns = timestamp_ns();
  system->events.push_back(event);
}

#if defined(_WIN32)
#include "platform/win32/window_backend.inl"
#elif defined(GRANIT_WINDOW_HAS_XCB)
xcb_atom_t intern_atom(xcb_connection_t* connection, const char* name) {
  const auto cookie =
      xcb_intern_atom(connection, 0, static_cast<std::uint16_t>(std::strlen(name)), name);
  std::unique_ptr<xcb_intern_atom_reply_t, decltype(&std::free)> reply{
      xcb_intern_atom_reply(connection, cookie, nullptr), &std::free};
  return reply ? reply->atom : static_cast<xcb_atom_t>(XCB_ATOM_NONE);
}

void pump_xcb_events(const std::shared_ptr<window_system_record>& system) {
  const auto public_handle = [&](xcb_window_t native_window) {
    for (const auto& [handle, window] : system->windows)
      if (window->window == native_window)
        return handle;
    return GRANIT_NULL_HANDLE;
  };
  while (auto* native_event = xcb_poll_for_event(system->connection)) {
    std::unique_ptr<xcb_generic_event_t, decltype(&std::free)> event{native_event, &std::free};
    const auto type = static_cast<std::uint8_t>(event->response_type & UINT8_C(0x7f));
    const auto dispatch_input = [&](granit_window window, std::int16_t x, std::int16_t y,
                                    std::uint16_t state, std::uint8_t detail) {
      if (system->input_native_event == nullptr || window == GRANIT_NULL_HANDLE)
        return;
      const granit_window_input_native_event input_event{
          GRANIT_WINDOW_INPUT_BACKEND_XCB, type, 0, 0, x, y, state, detail, 0, 0};
      system->input_native_event(system->input_user_data, window, &input_event);
    };
    if (type == XCB_KEY_PRESS || type == XCB_KEY_RELEASE) {
      const auto* key = reinterpret_cast<const xcb_key_press_event_t*>(event.get());
      dispatch_input(public_handle(key->event), key->event_x, key->event_y, key->state,
                     key->detail);
    } else if (type == XCB_BUTTON_PRESS || type == XCB_BUTTON_RELEASE) {
      const auto* button = reinterpret_cast<const xcb_button_press_event_t*>(event.get());
      dispatch_input(public_handle(button->event), button->event_x, button->event_y, button->state,
                     button->detail);
    } else if (type == XCB_MOTION_NOTIFY) {
      const auto* motion = reinterpret_cast<const xcb_motion_notify_event_t*>(event.get());
      dispatch_input(public_handle(motion->event), motion->event_x, motion->event_y, motion->state,
                     motion->detail);
    } else if (type == XCB_ENTER_NOTIFY || type == XCB_LEAVE_NOTIFY) {
      const auto* crossing = reinterpret_cast<const xcb_enter_notify_event_t*>(event.get());
      dispatch_input(public_handle(crossing->event), crossing->event_x, crossing->event_y,
                     crossing->state, crossing->detail);
    }
    if (type == XCB_CONFIGURE_NOTIFY) {
      const auto* configured = reinterpret_cast<const xcb_configure_notify_event_t*>(event.get());
      granit_window_event output = GRANIT_WINDOW_EVENT_INIT;
      output.type = GRANIT_WINDOW_EVENT_RESIZED;
      output.window = public_handle(configured->window);
      output.timestamp_ns = timestamp_ns();
      output.data.resized.width = configured->width;
      output.data.resized.height = configured->height;
      system->events.push_back(output);
    } else if (type == XCB_FOCUS_IN || type == XCB_FOCUS_OUT) {
      const auto* focused = reinterpret_cast<const xcb_focus_in_event_t*>(event.get());
      granit_window_event output = GRANIT_WINDOW_EVENT_INIT;
      output.type = GRANIT_WINDOW_EVENT_FOCUS_CHANGED;
      output.window = public_handle(focused->event);
      output.timestamp_ns = timestamp_ns();
      output.data.focus.focused = type == XCB_FOCUS_IN ? UINT32_C(1) : UINT32_C(0);
      system->events.push_back(output);
      if (type == XCB_FOCUS_OUT && system->input_focus_lost != nullptr)
        system->input_focus_lost(system->input_user_data, output.window);
    } else if (type == XCB_CLIENT_MESSAGE) {
      const auto* message = reinterpret_cast<const xcb_client_message_event_t*>(event.get());
      if (message->type == system->wm_protocols &&
          message->data.data32[0] == system->wm_delete_window)
        enqueue_event(system, public_handle(message->window), GRANIT_WINDOW_EVENT_CLOSE_REQUESTED);
    }
  }
}
#endif

#if defined(GRANIT_WINDOW_HAS_WAYLAND)
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
#endif

} // namespace

extern "C" granit_result granit_window_system_create(const granit_window_system_desc* desc,
                                                     granit_window_system* output) {
  if (output == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  *output = GRANIT_NULL_HANDLE;
  if (desc == nullptr || desc->struct_size < GRANIT_WINDOW_SYSTEM_DESC_VERSION_1_SIZE ||
      desc->flags != 0 || desc->reserved != 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
#if defined(_WIN32)
  if (desc->backend != GRANIT_WINDOW_BACKEND_AUTO && desc->backend != GRANIT_WINDOW_BACKEND_WIN32)
    return GRANIT_ERROR_UNSUPPORTED;
  return create_win32_system(output);
#elif defined(GRANIT_WINDOW_HAS_XCB) || defined(GRANIT_WINDOW_HAS_WAYLAND)
#if defined(GRANIT_WINDOW_HAS_WAYLAND)
  if (desc->backend == GRANIT_WINDOW_BACKEND_WAYLAND ||
      (desc->backend == GRANIT_WINDOW_BACKEND_AUTO && std::getenv("WAYLAND_DISPLAY") != nullptr)) {
    try {
      auto system = std::make_shared<window_system_record>();
      const auto result = initialize_wayland_system(*system);
      if (result != GRANIT_SUCCESS && desc->backend == GRANIT_WINDOW_BACKEND_WAYLAND)
        return result;
      if (result == GRANIT_SUCCESS) {
        system->owner_thread = std::this_thread::get_id();
        system->backend = GRANIT_WINDOW_BACKEND_WAYLAND;
        const auto handle = allocate_handle();
        try {
          std::lock_guard lock{registry_mutex};
          systems.emplace(handle, system);
        } catch (...) {
          destroy_wayland_system(*system);
          throw;
        }
        *output = handle;
        return GRANIT_SUCCESS;
      }
    } catch (const std::bad_alloc&) {
      return GRANIT_ERROR_OUT_OF_MEMORY;
    } catch (...) {
      return GRANIT_ERROR_INTERNAL;
    }
  }
#endif
#if defined(GRANIT_WINDOW_HAS_XCB)
  if (desc->backend != GRANIT_WINDOW_BACKEND_AUTO && desc->backend != GRANIT_WINDOW_BACKEND_XCB)
    return GRANIT_ERROR_UNSUPPORTED;
  try {
    auto system = std::make_shared<window_system_record>();
    system->connection = xcb_connect(nullptr, nullptr);
    if (system->connection == nullptr)
      return GRANIT_ERROR_BACKEND_UNAVAILABLE;
    std::unique_ptr<xcb_connection_t, decltype(&xcb_disconnect)> connection_guard{
        system->connection, &xcb_disconnect};
    if (xcb_connection_has_error(system->connection) != 0)
      return GRANIT_ERROR_BACKEND_UNAVAILABLE;
    const auto* setup = xcb_get_setup(system->connection);
    auto screens = xcb_setup_roots_iterator(setup);
    system->screen = screens.data;
    if (system->screen == nullptr)
      return GRANIT_ERROR_BACKEND_UNAVAILABLE;
    system->wm_protocols = intern_atom(system->connection, "WM_PROTOCOLS");
    system->wm_delete_window = intern_atom(system->connection, "WM_DELETE_WINDOW");
    system->wm_size_hints = intern_atom(system->connection, "WM_SIZE_HINTS");
    system->net_wm_name = intern_atom(system->connection, "_NET_WM_NAME");
    system->utf8_string = intern_atom(system->connection, "UTF8_STRING");
    system->owner_thread = std::this_thread::get_id();
    system->backend = GRANIT_WINDOW_BACKEND_XCB;
    const auto handle = allocate_handle();
    std::lock_guard lock{registry_mutex};
    systems.emplace(handle, std::move(system));
    connection_guard.release();
    *output = handle;
    return GRANIT_SUCCESS;
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
#else
  return GRANIT_ERROR_BACKEND_UNAVAILABLE;
#endif
#else
  return GRANIT_ERROR_UNSUPPORTED;
#endif
}

extern "C" granit_result granit_window_system_destroy(granit_window_system handle) {
  auto system = acquire_system(handle);
  if (!system)
    return GRANIT_ERROR_INVALID_HANDLE;
  if (!on_owner_thread(*system))
    return GRANIT_ERROR_INVALID_ARGUMENT;
  if (system->input_user_data != nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
#if defined(_WIN32)
  return destroy_win32_system(handle, system);
#elif defined(GRANIT_WINDOW_HAS_XCB) || defined(GRANIT_WINDOW_HAS_WAYLAND)
#if defined(GRANIT_WINDOW_HAS_WAYLAND)
  if (system->backend == GRANIT_WINDOW_BACKEND_WAYLAND) {
    for (const auto& [unused, window] : system->windows) {
      static_cast<void>(unused);
      destroy_wayland_window(*window);
    }
    system->windows.clear();
    {
      std::lock_guard lock{registry_mutex};
      systems.erase(handle);
    }
    static_cast<void>(wl_display_flush(system->display));
    destroy_wayland_system(*system);
    return GRANIT_SUCCESS;
  }
#endif
#if defined(GRANIT_WINDOW_HAS_XCB)
  for (const auto& [unused, window] : system->windows) {
    static_cast<void>(unused);
    if (window->window != XCB_WINDOW_NONE)
      xcb_destroy_window(system->connection, window->window);
  }
  system->windows.clear();
  {
    std::lock_guard lock{registry_mutex};
    systems.erase(handle);
  }
  xcb_flush(system->connection);
  xcb_disconnect(system->connection);
  return GRANIT_SUCCESS;
#else
  return GRANIT_ERROR_UNSUPPORTED;
#endif
#else
  return GRANIT_ERROR_UNSUPPORTED;
#endif
}

extern "C" granit_result granit_window_poll_event(granit_window_system handle,
                                                  granit_window_event* event) {
  if (event == nullptr || event->struct_size < GRANIT_WINDOW_EVENT_VERSION_1_SIZE)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  auto system = acquire_system(handle);
  if (!system)
    return GRANIT_ERROR_INVALID_HANDLE;
  if (!on_owner_thread(*system))
    return GRANIT_ERROR_INVALID_ARGUMENT;
#if defined(_WIN32)
  return poll_win32_event(system, event);
#elif defined(GRANIT_WINDOW_HAS_XCB) || defined(GRANIT_WINDOW_HAS_WAYLAND)
#if defined(GRANIT_WINDOW_HAS_WAYLAND)
  if (system->backend == GRANIT_WINDOW_BACKEND_WAYLAND) {
    const auto result = pump_wayland_events(*system);
    if (result != GRANIT_SUCCESS)
      return result;
  }
#endif
#if defined(GRANIT_WINDOW_HAS_XCB)
  if (system->backend == GRANIT_WINDOW_BACKEND_XCB) {
    pump_xcb_events(system);
    if (xcb_connection_has_error(system->connection) != 0)
      return GRANIT_ERROR_BACKEND_UNAVAILABLE;
  }
#endif
  if (system->events.empty())
    return GRANIT_ERROR_NOT_READY;
  *event = system->events.front();
  system->events.pop_front();
  return GRANIT_SUCCESS;
#else
  return GRANIT_ERROR_UNSUPPORTED;
#endif
}

extern "C" granit_result granit_window_create(granit_window_system system_handle,
                                              const granit_window_desc* desc,
                                              granit_window* output) {
  if (output == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  *output = GRANIT_NULL_HANDLE;
  if (desc == nullptr || desc->struct_size < GRANIT_WINDOW_DESC_VERSION_1_SIZE ||
      desc->width == 0 || desc->height == 0 || desc->reserved != 0 ||
      (desc->flags & ~(GRANIT_WINDOW_VISIBLE_BIT | GRANIT_WINDOW_RESIZABLE_BIT |
                       GRANIT_WINDOW_HIGH_DPI_BIT)) != 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  auto system = acquire_system(system_handle);
  if (!system)
    return GRANIT_ERROR_INVALID_HANDLE;
  if (!on_owner_thread(*system))
    return GRANIT_ERROR_INVALID_ARGUMENT;
#if defined(_WIN32)
  return create_win32_window(system, desc, output);
#elif defined(GRANIT_WINDOW_HAS_XCB) || defined(GRANIT_WINDOW_HAS_WAYLAND)
#if defined(GRANIT_WINDOW_HAS_WAYLAND)
  if (system->backend == GRANIT_WINDOW_BACKEND_WAYLAND) {
    try {
      if (desc->width > INT32_MAX || desc->height > INT32_MAX)
        return GRANIT_ERROR_INVALID_ARGUMENT;
      if (desc->title_length != 0 && desc->title == nullptr)
        return GRANIT_ERROR_INVALID_ARGUMENT;
      auto record = std::make_shared<window_record>();
      record->handle = allocate_handle();
      record->system = system;
      record->configured_width = desc->width;
      record->configured_height = desc->height;
      record->wayland_surface = wl_compositor_create_surface(system->compositor);
      if (record->wayland_surface == nullptr)
        return GRANIT_ERROR_BACKEND_UNAVAILABLE;
      record->wayland_xdg_surface =
          xdg_wm_base_get_xdg_surface(system->wm_base, record->wayland_surface);
      if (record->wayland_xdg_surface == nullptr) {
        destroy_wayland_window(*record);
        return GRANIT_ERROR_BACKEND_UNAVAILABLE;
      }
      xdg_surface_add_listener(record->wayland_xdg_surface, &wayland_surface_listener,
                               record.get());
      record->wayland_toplevel = xdg_surface_get_toplevel(record->wayland_xdg_surface);
      if (record->wayland_toplevel == nullptr) {
        destroy_wayland_window(*record);
        return GRANIT_ERROR_BACKEND_UNAVAILABLE;
      }
      xdg_toplevel_add_listener(record->wayland_toplevel, &wayland_toplevel_listener, record.get());
      const std::string title = desc->title_length == 0
                                    ? std::string{"Granit"}
                                    : std::string{desc->title, desc->title_length};
      xdg_toplevel_set_title(record->wayland_toplevel, title.c_str());
      if ((desc->flags & GRANIT_WINDOW_RESIZABLE_BIT) == 0) {
        xdg_toplevel_set_min_size(record->wayland_toplevel, static_cast<std::int32_t>(desc->width),
                                  static_cast<std::int32_t>(desc->height));
        xdg_toplevel_set_max_size(record->wayland_toplevel, static_cast<std::int32_t>(desc->width),
                                  static_cast<std::int32_t>(desc->height));
      }
      wl_surface_commit(record->wayland_surface);
      if (wl_display_roundtrip(system->display) < 0 || !record->configured) {
        destroy_wayland_window(*record);
        return GRANIT_ERROR_BACKEND_UNAVAILABLE;
      }
      try {
        system->windows.emplace(record->handle, record);
      } catch (...) {
        destroy_wayland_window(*record);
        throw;
      }
      *output = record->handle;
      return GRANIT_SUCCESS;
    } catch (const std::bad_alloc&) {
      return GRANIT_ERROR_OUT_OF_MEMORY;
    } catch (...) {
      return GRANIT_ERROR_INTERNAL;
    }
  }
#endif
#if defined(GRANIT_WINDOW_HAS_XCB)
  try {
    if (desc->width > UINT16_MAX || desc->height > UINT16_MAX)
      return GRANIT_ERROR_INVALID_ARGUMENT;
    if (desc->title_length != 0 && desc->title == nullptr)
      return GRANIT_ERROR_INVALID_ARGUMENT;
    auto record = std::make_shared<window_record>();
    record->handle = allocate_handle();
    record->system = system;
    record->window = xcb_generate_id(system->connection);
    const std::uint32_t values[] = {
        system->screen->black_pixel,
        XCB_EVENT_MASK_STRUCTURE_NOTIFY | XCB_EVENT_MASK_FOCUS_CHANGE | XCB_EVENT_MASK_KEY_PRESS |
            XCB_EVENT_MASK_KEY_RELEASE | XCB_EVENT_MASK_BUTTON_PRESS |
            XCB_EVENT_MASK_BUTTON_RELEASE | XCB_EVENT_MASK_POINTER_MOTION |
            XCB_EVENT_MASK_ENTER_WINDOW | XCB_EVENT_MASK_LEAVE_WINDOW};
    xcb_create_window(system->connection, XCB_COPY_FROM_PARENT, record->window,
                      system->screen->root, 0, 0, static_cast<std::uint16_t>(desc->width),
                      static_cast<std::uint16_t>(desc->height), 0, XCB_WINDOW_CLASS_INPUT_OUTPUT,
                      system->screen->root_visual, XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK, values);
    const char default_title[] = "Granit";
    const char* title = desc->title_length == 0 ? default_title : desc->title;
    const auto title_length = desc->title_length == 0 ? UINT32_C(6) : desc->title_length;
    xcb_change_property(system->connection, XCB_PROP_MODE_REPLACE, record->window, XCB_ATOM_WM_NAME,
                        XCB_ATOM_STRING, 8, title_length, title);
    if (system->net_wm_name != XCB_ATOM_NONE && system->utf8_string != XCB_ATOM_NONE)
      xcb_change_property(system->connection, XCB_PROP_MODE_REPLACE, record->window,
                          system->net_wm_name, system->utf8_string, 8, title_length, title);
    if (system->wm_protocols != XCB_ATOM_NONE && system->wm_delete_window != XCB_ATOM_NONE)
      xcb_change_property(system->connection, XCB_PROP_MODE_REPLACE, record->window,
                          system->wm_protocols, XCB_ATOM_ATOM, 32, 1, &system->wm_delete_window);
    if ((desc->flags & GRANIT_WINDOW_RESIZABLE_BIT) == 0 &&
        system->wm_size_hints != XCB_ATOM_NONE) {
      std::uint32_t size_hints[18]{};
      size_hints[0] = (UINT32_C(1) << 4) | (UINT32_C(1) << 5);
      size_hints[5] = desc->width;
      size_hints[6] = desc->height;
      size_hints[7] = desc->width;
      size_hints[8] = desc->height;
      xcb_change_property(system->connection, XCB_PROP_MODE_REPLACE, record->window,
                          XCB_ATOM_WM_NORMAL_HINTS, system->wm_size_hints, 32, 18, size_hints);
    }
    if ((desc->flags & GRANIT_WINDOW_VISIBLE_BIT) != 0)
      xcb_map_window(system->connection, record->window);
    system->windows.emplace(record->handle, record);
    if (xcb_flush(system->connection) <= 0 || xcb_connection_has_error(system->connection) != 0) {
      system->windows.erase(record->handle);
      return GRANIT_ERROR_BACKEND_UNAVAILABLE;
    }
    *output = record->handle;
    return GRANIT_SUCCESS;
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
#else
  return GRANIT_ERROR_UNSUPPORTED;
#endif
#else
  return GRANIT_ERROR_UNSUPPORTED;
#endif
}

extern "C" granit_result granit_window_destroy(granit_window_system system_handle,
                                               granit_window window_handle) {
  auto system = acquire_system(system_handle);
  if (!system)
    return GRANIT_ERROR_INVALID_HANDLE;
  if (!on_owner_thread(*system))
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const auto found = system->windows.find(window_handle);
  if (found == system->windows.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  if (system->input_window_destroyed != nullptr)
    system->input_window_destroyed(system->input_user_data, window_handle);
#if defined(_WIN32)
  return destroy_win32_window(system, window_handle);
#elif defined(GRANIT_WINDOW_HAS_XCB) || defined(GRANIT_WINDOW_HAS_WAYLAND)
  const auto window = std::move(found->second);
  system->windows.erase(found);
#if defined(GRANIT_WINDOW_HAS_WAYLAND)
  if (system->backend == GRANIT_WINDOW_BACKEND_WAYLAND) {
    if (system->keyboard_window == window_handle)
      system->keyboard_window = GRANIT_NULL_HANDLE;
    if (system->pointer_window == window_handle)
      system->pointer_window = GRANIT_NULL_HANDLE;
    destroy_wayland_window(*window);
    return wl_display_flush(system->display) >= 0 ? GRANIT_SUCCESS
                                                  : GRANIT_ERROR_BACKEND_UNAVAILABLE;
  }
#endif
#if defined(GRANIT_WINDOW_HAS_XCB)
  xcb_destroy_window(system->connection, window->window);
  return xcb_flush(system->connection) > 0 ? GRANIT_SUCCESS : GRANIT_ERROR_BACKEND_UNAVAILABLE;
#else
  return GRANIT_ERROR_UNSUPPORTED;
#endif
#else
  return GRANIT_ERROR_UNSUPPORTED;
#endif
}

extern "C" granit_result
granit_window_internal_attach_input(granit_window_system handle, void* user_data,
                                    granit_window_input_window_callback window_destroyed,
                                    granit_window_input_window_callback focus_lost,
                                    granit_window_input_native_event_callback native_event) {
  auto system = acquire_system(handle);
  if (!system)
    return GRANIT_ERROR_INVALID_HANDLE;
  if (!on_owner_thread(*system) || user_data == nullptr || window_destroyed == nullptr ||
      focus_lost == nullptr || native_event == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  if (system->input_user_data != nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  system->input_user_data = user_data;
  system->input_window_destroyed = window_destroyed;
  system->input_focus_lost = focus_lost;
  system->input_native_event = native_event;
#if defined(GRANIT_WINDOW_HAS_WAYLAND)
  if (system->backend == GRANIT_WINDOW_BACKEND_WAYLAND) {
#if defined(GRANIT_WINDOW_HAS_WAYLAND_INPUT)
    const auto result = initialize_wayland_input(*system);
    if (result == GRANIT_SUCCESS)
      return GRANIT_SUCCESS;
#else
    const auto result = GRANIT_ERROR_UNSUPPORTED;
#endif
    system->input_user_data = nullptr;
    system->input_window_destroyed = nullptr;
    system->input_focus_lost = nullptr;
    system->input_native_event = nullptr;
    return result;
  }
#endif
  return GRANIT_SUCCESS;
}

extern "C" granit_result granit_window_internal_detach_input(granit_window_system handle,
                                                             void* user_data) {
  auto system = acquire_system(handle);
  if (!system)
    return GRANIT_ERROR_INVALID_HANDLE;
  if (!on_owner_thread(*system) || system->input_user_data != user_data)
    return GRANIT_ERROR_INVALID_ARGUMENT;
#if defined(GRANIT_WINDOW_HAS_WAYLAND)
  if (system->backend == GRANIT_WINDOW_BACKEND_WAYLAND)
    destroy_wayland_input(*system);
#endif
  system->input_user_data = nullptr;
  system->input_window_destroyed = nullptr;
  system->input_focus_lost = nullptr;
  system->input_native_event = nullptr;
  return GRANIT_SUCCESS;
}

extern "C" granit_result granit_window_internal_pump(granit_window_system handle) {
  granit_window_event event = GRANIT_WINDOW_EVENT_INIT;
  const auto result = granit_window_poll_event(handle, &event);
  if (result == GRANIT_SUCCESS) {
    auto system = acquire_system(handle);
    if (system)
      system->events.push_front(event);
    return GRANIT_SUCCESS;
  }
  return result == GRANIT_ERROR_NOT_READY ? GRANIT_SUCCESS : result;
}

extern "C" granit_result granit_window_internal_contains(granit_window_system handle,
                                                         granit_window window) {
  auto system = acquire_system(handle);
  if (!system)
    return GRANIT_ERROR_INVALID_HANDLE;
  if (!on_owner_thread(*system))
    return GRANIT_ERROR_INVALID_ARGUMENT;
  return system->windows.contains(window) ? GRANIT_SUCCESS : GRANIT_ERROR_INVALID_HANDLE;
}

extern "C" granit_result granit_window_get_win32(granit_window_system system_handle,
                                                 granit_window window_handle, void** instance,
                                                 void** native_window) {
  if (instance == nullptr || native_window == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  *instance = nullptr;
  *native_window = nullptr;
  auto system = acquire_system(system_handle);
  if (!system)
    return GRANIT_ERROR_INVALID_HANDLE;
  if (!on_owner_thread(*system))
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const auto found = system->windows.find(window_handle);
  if (found == system->windows.end())
    return GRANIT_ERROR_INVALID_HANDLE;
#if defined(_WIN32)
  return get_win32_window(found->second, instance, native_window);
#else
  return GRANIT_ERROR_UNSUPPORTED;
#endif
}

extern "C" granit_result granit_window_get_xcb(granit_window_system system_handle,
                                               granit_window window_handle, void** connection,
                                               uint32_t* native_window) {
  if (connection == nullptr || native_window == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  *connection = nullptr;
  *native_window = 0;
  auto system = acquire_system(system_handle);
  if (!system)
    return GRANIT_ERROR_INVALID_HANDLE;
  if (!on_owner_thread(*system))
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const auto found = system->windows.find(window_handle);
  if (found == system->windows.end())
    return GRANIT_ERROR_INVALID_HANDLE;
#if defined(GRANIT_WINDOW_HAS_XCB)
  if (system->backend == GRANIT_WINDOW_BACKEND_XCB) {
    *connection = system->connection;
    *native_window = found->second->window;
    return GRANIT_SUCCESS;
  }
#endif
  return GRANIT_ERROR_UNSUPPORTED;
}

extern "C" granit_result granit_window_get_wayland(granit_window_system system_handle,
                                                   granit_window window_handle, void** display,
                                                   void** native_surface) {
  if (display == nullptr || native_surface == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  *display = nullptr;
  *native_surface = nullptr;
  auto system = acquire_system(system_handle);
  if (!system)
    return GRANIT_ERROR_INVALID_HANDLE;
  if (!on_owner_thread(*system))
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const auto found = system->windows.find(window_handle);
  if (found == system->windows.end())
    return GRANIT_ERROR_INVALID_HANDLE;
#if defined(GRANIT_WINDOW_HAS_WAYLAND)
  if (system->backend == GRANIT_WINDOW_BACKEND_WAYLAND) {
    *display = system->display;
    *native_surface = found->second->wayland_surface;
    return GRANIT_SUCCESS;
  }
#endif
  return GRANIT_ERROR_UNSUPPORTED;
}
