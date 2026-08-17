// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_WINDOW_INPUT_BRIDGE_HPP_
#define GRANIT_WINDOW_INPUT_BRIDGE_HPP_

#include <granit/window/export.h>
#include <granit/window/window.h>

extern "C" {

typedef void (*granit_window_input_window_callback)(void* user_data, granit_window window);

GRANIT_WINDOW_API granit_result
granit_window_internal_attach_input(granit_window_system system, void* user_data,
                                    granit_window_input_window_callback window_destroyed,
                                    granit_window_input_window_callback focus_lost);
GRANIT_WINDOW_API granit_result granit_window_internal_detach_input(granit_window_system system,
                                                                    void* user_data);
GRANIT_WINDOW_API granit_result granit_window_internal_pump(granit_window_system system);
GRANIT_WINDOW_API granit_result granit_window_internal_contains(granit_window_system system,
                                                                granit_window window);
}

#endif
