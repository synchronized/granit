// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_PLATFORM_WINDOW_BACKEND_INTERNAL_H_
#define GRANIT_PLATFORM_WINDOW_BACKEND_INTERNAL_H_

#include <granit/window/window.h>

#include "platform/window/input_bridge.hpp"

#if defined(GRANIT_WINDOW_HAS_XCB)
#include <xcb/xcb.h>
#endif

#if defined(GRANIT_WINDOW_HAS_WAYLAND)
void destroy_wayland_window(window_record& window);
void destroy_wayland_input(window_system_record& system);
granit_result initialize_wayland_input(window_system_record& system);
void destroy_wayland_system(window_system_record& system);
granit_result initialize_wayland_system(window_system_record& system);
granit_result pump_wayland_events(window_system_record& system);
#endif
#if defined(GRANIT_WINDOW_HAS_WAYLAND)
#include <wayland-client.h>
#include <xdg-shell-client-protocol.h>
#endif

#include <atomic>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>

namespace granit::window::detail {

struct window_system_record;

struct window_record {
  granit_window handle{};
  std::weak_ptr<window_system_record> system;
#if defined(_WIN32)
  void* instance{};
  void* window{};
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

extern std::mutex registry_mutex;
extern std::unordered_map<granit_window_system, std::shared_ptr<window_system_record>> systems;
extern std::atomic<std::uint64_t> next_handle;

std::uint64_t allocate_handle() noexcept;
std::uint64_t timestamp_ns() noexcept;
void enqueue_event(const std::shared_ptr<window_system_record>& system, granit_window window,
                   std::uint32_t type);

#if defined(_WIN32)
granit_result create_win32_system(granit_window_system* output);
granit_result destroy_win32_system(granit_window_system handle,
                                   const std::shared_ptr<window_system_record>& system);
granit_result poll_win32_event(const std::shared_ptr<window_system_record>& system,
                               granit_window_event* event);
granit_result create_win32_window(const std::shared_ptr<window_system_record>& system,
                                  const granit_window_desc* desc, granit_window* output);
granit_result destroy_win32_window(const std::shared_ptr<window_system_record>& system,
                                   granit_window handle);
granit_result get_win32_window(const std::shared_ptr<window_record>& window, void** instance,
                               void** native_window);
#endif

#if defined(GRANIT_WINDOW_HAS_XCB)
granit_result create_xcb_system(granit_window_system* output);
granit_result destroy_xcb_system(granit_window_system handle,
                                 const std::shared_ptr<window_system_record>& system);
granit_result poll_xcb_event(const std::shared_ptr<window_system_record>& system,
                             granit_window_event* event);
granit_result create_xcb_window(const std::shared_ptr<window_system_record>& system,
                                const granit_window_desc* desc, granit_window* output);
granit_result destroy_xcb_window(const std::shared_ptr<window_system_record>& system,
                                 const std::shared_ptr<window_record>& window);
granit_result get_xcb_window(const std::shared_ptr<window_system_record>& system,
                             const std::shared_ptr<window_record>& window, void** connection,
                             std::uint32_t* native_window);
void pump_xcb_events(const std::shared_ptr<window_system_record>& system);
#endif

} // namespace granit::window::detail

#endif
