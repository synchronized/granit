// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <stddef.h>
#include <stdint.h>

#include <granit/window.h>

#include "snapshots/0.36.0/window_layout.h"

#define GRANIT_ABI_ASSERT(name, expression) typedef char name[(expression) ? 1 : -1]

#if UINTPTR_MAX == UINT64_MAX
GRANIT_ABI_ASSERT(granit_036_window_system_desc_size,
                  sizeof(granit_window_system_desc) == GRANIT_ABI_036_WINDOW_SYSTEM_DESC_SIZE);
GRANIT_ABI_ASSERT(granit_036_window_desc_size,
                  sizeof(granit_window_desc) == GRANIT_ABI_036_WINDOW_DESC_SIZE);
GRANIT_ABI_ASSERT(granit_036_window_desc_target,
                  offsetof(granit_window_desc, target) == GRANIT_ABI_036_WINDOW_DESC_TARGET);
GRANIT_ABI_ASSERT(granit_036_window_event_size,
                  sizeof(granit_window_event) == GRANIT_ABI_036_WINDOW_EVENT_SIZE);
GRANIT_ABI_ASSERT(granit_036_input_event_size,
                  sizeof(granit_input_event) == GRANIT_ABI_036_INPUT_EVENT_SIZE);
GRANIT_ABI_ASSERT(granit_036_keyboard_state_size,
                  sizeof(granit_keyboard_state) == GRANIT_ABI_036_KEYBOARD_STATE_SIZE);
GRANIT_ABI_ASSERT(granit_036_pointer_state_size,
                  sizeof(granit_pointer_state) == GRANIT_ABI_036_POINTER_STATE_SIZE);
#endif

#undef GRANIT_ABI_ASSERT
