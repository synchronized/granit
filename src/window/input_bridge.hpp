// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_PLATFORM_WINDOW_INPUT_BRIDGE_HPP_
#define GRANIT_PLATFORM_WINDOW_INPUT_BRIDGE_HPP_

#include <granit/window/export.h>
#include <granit/window/window.h>

#include <stdint.h>

extern "C" {

typedef void (*granit_window_input_window_callback)(void* user_data, granit_window window);

#define GRANIT_WINDOW_INPUT_BACKEND_WIN32 UINT32_C(1)
#define GRANIT_WINDOW_INPUT_BACKEND_XCB UINT32_C(2)
#define GRANIT_WINDOW_INPUT_BACKEND_WAYLAND UINT32_C(3)

#define GRANIT_WINDOW_INPUT_WAYLAND_KEYMAP UINT32_C(1)
#define GRANIT_WINDOW_INPUT_WAYLAND_KEY UINT32_C(2)
#define GRANIT_WINDOW_INPUT_WAYLAND_MODIFIERS UINT32_C(3)
#define GRANIT_WINDOW_INPUT_WAYLAND_POINTER_ENTER UINT32_C(4)
#define GRANIT_WINDOW_INPUT_WAYLAND_POINTER_LEAVE UINT32_C(5)
#define GRANIT_WINDOW_INPUT_WAYLAND_POINTER_MOTION UINT32_C(6)
#define GRANIT_WINDOW_INPUT_WAYLAND_POINTER_BUTTON UINT32_C(7)
#define GRANIT_WINDOW_INPUT_WAYLAND_POINTER_AXIS UINT32_C(8)

typedef struct granit_window_input_native_event {
  uint32_t backend;
  uint32_t type;
  uintptr_t word;
  intptr_t value;
  int32_t x;
  int32_t y;
  uint32_t state;
  uint32_t detail;
  uint32_t data0;
  uint32_t data1;
} granit_window_input_native_event;

typedef void (*granit_window_input_native_event_callback)(
    void* user_data, granit_window window, const granit_window_input_native_event* event);

GRANIT_WINDOW_API granit_result
granit_window_internal_attach_input(granit_window_system system, void* user_data,
                                    granit_window_input_window_callback window_destroyed,
                                    granit_window_input_window_callback focus_lost,
                                    granit_window_input_native_event_callback native_event);
GRANIT_WINDOW_API granit_result granit_window_internal_detach_input(granit_window_system system,
                                                                    void* user_data);
GRANIT_WINDOW_API granit_result granit_window_internal_pump(granit_window_system system);
GRANIT_WINDOW_API granit_result granit_window_internal_contains(granit_window_system system,
                                                                granit_window window);
}

#endif
