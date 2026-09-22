// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/window/native.h>

granit_result granit_window_native_h_header_test(void) {
  granit_window_native_win32 native = GRANIT_WINDOW_NATIVE_WIN32_INIT;
  return granit_window_get_native_win32(GRANIT_NULL_HANDLE, GRANIT_NULL_HANDLE, &native);
}
