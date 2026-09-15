// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/window/native.h>

granit_result granit_window_native_h_header_test(void) {
  void* instance = 0;
  void* native_window = 0;
  return granit_window_get_win32(GRANIT_NULL_HANDLE, GRANIT_NULL_HANDLE, &instance, &native_window);
}
