// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_WINDOW_INPUT_STATE_H_
#define GRANIT_WINDOW_INPUT_STATE_H_

#include "window/input/native_event.h"
#include "window/registry.h"

namespace granit::window::detail {

void handle_native_input(window_system_record& system, granit_window window,
                         const granit_window_input_native_event& event);
void clear_window_input(window_system_record& system, granit_window window);
void clear_input_focus(window_system_record& system, granit_window window);

} // namespace granit::window::detail

#endif
