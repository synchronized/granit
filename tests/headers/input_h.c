// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/input.h>

static granit_input_system granit_test_input_system;
static granit_input_system_desc granit_test_input_system_desc = GRANIT_INPUT_SYSTEM_DESC_INIT;
static granit_input_event granit_test_input_event = GRANIT_INPUT_EVENT_INIT;
static granit_keyboard_state granit_test_keyboard_state = GRANIT_KEYBOARD_STATE_INIT;
static granit_pointer_state granit_test_pointer_state = GRANIT_POINTER_STATE_INIT;

_Static_assert(sizeof(granit_input_event_data) == 64, "Input 事件负载 ABI 大小必须稳定");
_Static_assert(sizeof(granit_input_event) == 88, "Input 事件 ABI 大小必须稳定");
_Static_assert(sizeof(granit_keyboard_state) == 64, "键盘状态 ABI 大小必须稳定");
_Static_assert(sizeof(granit_pointer_state) == 40, "指针状态 ABI 大小必须稳定");

void granit_input_h_header_test(void) {
  granit_test_input_system = GRANIT_NULL_HANDLE;
  granit_test_input_system_desc.window_system = GRANIT_NULL_HANDLE;
  granit_test_input_event.type = GRANIT_INPUT_EVENT_KEY;
  granit_test_keyboard_state.modifiers = GRANIT_MODIFIER_LEFT_SHIFT_BIT;
  granit_test_pointer_state.buttons = GRANIT_POINTER_PRIMARY_BIT;
}
