// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_WINDOW_REGISTRY_H_
#define GRANIT_WINDOW_REGISTRY_H_

#include <granit/window/input.h>
#include <granit/window/window.h>

#include "window/input/platform_adapter.h"

#include <atomic>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>

#if defined(GRANIT_WINDOW_HAS_XCB)
struct xcb_connection_t;
struct xcb_screen_t;
#endif
#if defined(GRANIT_WINDOW_HAS_WAYLAND)
struct wl_surface;
struct xdg_surface;
struct xdg_toplevel;
struct wl_display;
struct wl_registry;
struct wl_compositor;
struct xdg_wm_base;
struct wl_seat;
struct wl_keyboard;
struct wl_pointer;
#endif

namespace granit::window::detail {

struct window_system_record;

struct window_record {
  granit_window handle{};
  std::weak_ptr<window_system_record> system;
  std::uint32_t width{};
  std::uint32_t height{};
  std::uint32_t framebuffer_width{};
  std::uint32_t framebuffer_height{};
  float content_scale_horizontal{1.0F};
  float content_scale_vertical{1.0F};
#if defined(_WIN32)
  void* instance{};
  void* window{};
  bool high_dpi{};
  bool pointer_tracking{};
#endif
#if defined(GRANIT_WINDOW_HAS_XCB)
  std::uint32_t window{0};
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
#if defined(__EMSCRIPTEN__)
  std::uint32_t flags{};
  bool focused{};
#endif
};

struct window_system_record {
  std::thread::id owner_thread;
  std::uint32_t backend{};
  std::unordered_map<granit_window, std::shared_ptr<window_record>> windows;
  std::deque<granit_window_event> events;
  std::deque<granit_input_event> input_events;
  std::unordered_map<granit_window, granit_keyboard_state> keyboards;
  std::unordered_map<granit_window, granit_pointer_state> pointers;
  granit::input::detail::platform_input_adapter input_platform;
  bool loop_running{};
#if defined(GRANIT_WINDOW_HAS_XCB)
  xcb_connection_t* connection{};
  xcb_screen_t* screen{};
  std::uint32_t wm_protocols{0};
  std::uint32_t wm_delete_window{0};
  std::uint32_t wm_size_hints{0};
  std::uint32_t net_wm_name{0};
  std::uint32_t utf8_string{0};
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
std::shared_ptr<window_system_record> acquire_system(granit_window_system handle);
bool on_owner_thread(const window_system_record& system) noexcept;
} // namespace granit::window::detail

#endif
