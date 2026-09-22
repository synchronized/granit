// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_WINDOW_BACKEND_INTERNAL_H_
#define GRANIT_WINDOW_BACKEND_INTERNAL_H_

#include <granit/window/native.h>

#include "window/event_queue.h"
#include "window/input/input_state.h"
#include "window/registry.h"

namespace granit::window::detail {

#if defined(_WIN32)
granit_result create_win32_system(granit_window_system* output);
granit_result destroy_win32_system(granit_window_system handle,
                                   const std::shared_ptr<window_system_record>& system);
granit_result process_win32_events(const std::shared_ptr<window_system_record>& system);
granit_result create_win32_window(const std::shared_ptr<window_system_record>& system,
                                  const granit_window_desc* desc, granit_window* output);
granit_result destroy_win32_window(const std::shared_ptr<window_system_record>& system,
                                   granit_window handle);
granit_result get_native_win32(const std::shared_ptr<window_record>& window,
                               granit_window_native_win32& output);
#endif

#if defined(GRANIT_WINDOW_HAS_XCB)
granit_result create_xcb_system(granit_window_system* output);
granit_result destroy_xcb_system(granit_window_system handle,
                                 const std::shared_ptr<window_system_record>& system);
granit_result process_xcb_events(const std::shared_ptr<window_system_record>& system);
granit_result create_xcb_window(const std::shared_ptr<window_system_record>& system,
                                const granit_window_desc* desc, granit_window* output);
granit_result destroy_xcb_window(const std::shared_ptr<window_system_record>& system,
                                 const std::shared_ptr<window_record>& window);
granit_result get_native_xcb(const std::shared_ptr<window_system_record>& system,
                             const std::shared_ptr<window_record>& window,
                             granit_window_native_xcb& output);
void pump_xcb_events(const std::shared_ptr<window_system_record>& system);
#endif

#if defined(GRANIT_WINDOW_HAS_WAYLAND)
void destroy_wayland_window(window_record& window);
void destroy_wayland_input(window_system_record& system);
granit_result initialize_wayland_input(window_system_record& system);
void destroy_wayland_system(window_system_record& system);
granit_result initialize_wayland_system(window_system_record& system);
granit_result pump_wayland_events(window_system_record& system);
granit_result create_wayland_system(granit_window_system* output);
granit_result
destroy_registered_wayland_system(granit_window_system handle,
                                  const std::shared_ptr<window_system_record>& system);
granit_result process_wayland_events(const std::shared_ptr<window_system_record>& system);
granit_result create_wayland_window(const std::shared_ptr<window_system_record>& system,
                                    const granit_window_desc* desc, granit_window* output);
granit_result destroy_registered_wayland_window(const std::shared_ptr<window_system_record>& system,
                                                const std::shared_ptr<window_record>& window,
                                                granit_window handle);
granit_result get_native_wayland(const std::shared_ptr<window_system_record>& system,
                                 const std::shared_ptr<window_record>& window,
                                 granit_window_native_wayland& output);
#endif

#if defined(__EMSCRIPTEN__)
granit_result create_emscripten_system(granit_window_system* output);
granit_result destroy_emscripten_system(granit_window_system handle,
                                        const std::shared_ptr<window_system_record>& system);
granit_result process_emscripten_events(const std::shared_ptr<window_system_record>& system);
granit_result create_emscripten_window(const std::shared_ptr<window_system_record>& system,
                                       const granit_window_desc* desc, granit_window* output);
granit_result destroy_emscripten_window(const std::shared_ptr<window_system_record>& system,
                                        granit_window handle);
granit_result get_native_emscripten(granit_window_native_emscripten& output);
#endif

} // namespace granit::window::detail

#endif
