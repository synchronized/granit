// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/input.h>

int main(void) {
  granit_window_system_desc window_desc = GRANIT_WINDOW_SYSTEM_DESC_INIT;
  granit_window_system windows = GRANIT_NULL_HANDLE;
  const granit_result window_result = granit_window_system_create(&window_desc, &windows);
  if (window_result == GRANIT_ERROR_UNSUPPORTED ||
      window_result == GRANIT_ERROR_BACKEND_UNAVAILABLE)
    return windows == GRANIT_NULL_HANDLE ? 0 : 1;
  if (window_result != GRANIT_SUCCESS || windows == GRANIT_NULL_HANDLE)
    return 2;

  granit_input_system_desc input_desc = GRANIT_INPUT_SYSTEM_DESC_INIT;
  input_desc.window_system = windows;
  granit_input_system input = GRANIT_NULL_HANDLE;
  if (granit_input_system_create(&input_desc, &input) != GRANIT_SUCCESS ||
      input == GRANIT_NULL_HANDLE)
    return 3;
  if (granit_input_system_destroy(input) != GRANIT_SUCCESS)
    return 4;
  return granit_window_system_destroy(windows) == GRANIT_SUCCESS ? 0 : 5;
}
