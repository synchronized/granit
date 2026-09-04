// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/window.h>

#include "../abi/snapshots/0.1.0/optional_components_identity.h"

_Static_assert(sizeof(granit_window_system_desc) == GRANIT_WINDOW_SYSTEM_DESC_VERSION_1_SIZE,
               "Window System V1 ABI 大小必须稳定");
_Static_assert(sizeof(granit_window_desc) == 40, "Window ABI 大小必须稳定");
_Static_assert(GRANIT_WINDOW_DESC_VERSION_1_SIZE == 36, "Window V1 最低尺寸必须稳定");
_Static_assert(sizeof(granit_window_event) == GRANIT_WINDOW_EVENT_VERSION_1_SIZE,
               "Window Event V1 ABI 大小必须稳定");
_Static_assert(sizeof(granit_window_state) == GRANIT_WINDOW_STATE_VERSION_1_SIZE,
               "Window State V1 ABI 大小必须稳定");

static granit_window_system granit_test_window_system;
static granit_window granit_test_window;
static granit_window_system_desc granit_test_window_system_desc = GRANIT_WINDOW_SYSTEM_DESC_INIT;
static granit_window_desc granit_test_window_desc = GRANIT_WINDOW_DESC_INIT;
static granit_window_event granit_test_window_event = GRANIT_WINDOW_EVENT_INIT;
static granit_window_state granit_test_window_state = GRANIT_WINDOW_STATE_INIT;

void granit_window_h_header_test(void) {
  granit_test_window_system = GRANIT_NULL_HANDLE;
  granit_test_window = GRANIT_NULL_HANDLE;
  granit_test_window_system_desc.backend = GRANIT_WINDOW_BACKEND_AUTO;
  granit_test_window_desc.width = UINT32_C(640);
  granit_test_window_event.type = GRANIT_WINDOW_EVENT_RESIZED;
  granit_test_window_state.width = UINT32_C(640);
}
