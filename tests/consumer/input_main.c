// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/window.h>

int main(void) {
  granit_window_system_desc window_desc = GRANIT_WINDOW_SYSTEM_DESC_INIT;
  granit_window_system windows = GRANIT_NULL_HANDLE;
  const granit_result window_result = granit_window_system_create(&window_desc, &windows);
  if (window_result == GRANIT_ERROR_UNSUPPORTED ||
      window_result == GRANIT_ERROR_BACKEND_UNAVAILABLE)
    return windows == GRANIT_NULL_HANDLE ? 0 : 1;
  if (window_result != GRANIT_SUCCESS || windows == GRANIT_NULL_HANDLE)
    return 2;

  if (granit_window_system_process_events(windows) != GRANIT_SUCCESS)
    return 3;
  granit_input_event event = GRANIT_INPUT_EVENT_INIT;
  if (granit_window_poll_input_event(windows, &event) != GRANIT_ERROR_NOT_READY)
    return 4;
  if (granit_window_system_destroy(windows) != GRANIT_SUCCESS)
    return 5;
  return granit_window_system_destroy(windows) == GRANIT_ERROR_INVALID_HANDLE ? 0 : 6;
}
