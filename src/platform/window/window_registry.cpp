// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/window/window.h>

#include "platform/window/window_backend_internal.h"

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

namespace granit::window::detail {

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

} // namespace granit::window::detail

using namespace granit::window::detail;

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
    const auto result = create_wayland_system(output);
    if (result == GRANIT_SUCCESS || desc->backend == GRANIT_WINDOW_BACKEND_WAYLAND)
      return result;
  }
#endif
#if defined(GRANIT_WINDOW_HAS_XCB)
  if (desc->backend != GRANIT_WINDOW_BACKEND_AUTO && desc->backend != GRANIT_WINDOW_BACKEND_XCB)
    return GRANIT_ERROR_UNSUPPORTED;
  return create_xcb_system(output);
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
  if (system->backend == GRANIT_WINDOW_BACKEND_WAYLAND)
    return destroy_registered_wayland_system(handle, system);
#endif
#if defined(GRANIT_WINDOW_HAS_XCB)
  return destroy_xcb_system(handle, system);
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
  if (system->backend == GRANIT_WINDOW_BACKEND_WAYLAND)
    return poll_wayland_event(system, event);
#endif
#if defined(GRANIT_WINDOW_HAS_XCB)
  if (system->backend == GRANIT_WINDOW_BACKEND_XCB)
    return poll_xcb_event(system, event);
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
  if (system->backend == GRANIT_WINDOW_BACKEND_WAYLAND)
    return create_wayland_window(system, desc, output);
#endif
#if defined(GRANIT_WINDOW_HAS_XCB)
  return create_xcb_window(system, desc, output);
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
  if (system->backend == GRANIT_WINDOW_BACKEND_WAYLAND)
    return destroy_registered_wayland_window(system, window, window_handle);
#endif
#if defined(GRANIT_WINDOW_HAS_XCB)
  return destroy_xcb_window(system, window);
#else
  return GRANIT_ERROR_UNSUPPORTED;
#endif
#else
  return GRANIT_ERROR_UNSUPPORTED;
#endif
}

extern "C" granit_result granit_window_get_state(granit_window_system system_handle,
                                                 granit_window window_handle,
                                                 granit_window_state* state) {
  if (state == nullptr || state->struct_size < GRANIT_WINDOW_STATE_VERSION_1_SIZE)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  *state = GRANIT_WINDOW_STATE_INIT;
  auto system = acquire_system(system_handle);
  if (!system)
    return GRANIT_ERROR_INVALID_HANDLE;
  if (!on_owner_thread(*system))
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const auto found = system->windows.find(window_handle);
  if (found == system->windows.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  const auto& window = *found->second;
  state->width = window.width;
  state->height = window.height;
  state->framebuffer_width = window.framebuffer_width;
  state->framebuffer_height = window.framebuffer_height;
  state->content_scale_horizontal = window.content_scale_horizontal;
  state->content_scale_vertical = window.content_scale_vertical;
  return GRANIT_SUCCESS;
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
    const auto result = attach_wayland_input(*system);
    if (result == GRANIT_SUCCESS)
      return GRANIT_SUCCESS;
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
    detach_wayland_input(*system);
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
  if (system->backend == GRANIT_WINDOW_BACKEND_XCB)
    return get_xcb_window(system, found->second, connection, native_window);
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
  if (system->backend == GRANIT_WINDOW_BACKEND_WAYLAND)
    return get_wayland_window(system, found->second, display, native_surface);
#endif
  return GRANIT_ERROR_UNSUPPORTED;
}
