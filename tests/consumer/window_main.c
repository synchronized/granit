// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/window.h>

int main(void) {
  granit_window_system_desc desc = GRANIT_WINDOW_SYSTEM_DESC_INIT;
  granit_window_system system = GRANIT_NULL_HANDLE;
  const granit_result result = granit_window_system_create(&desc, &system);
#if defined(_WIN32)
  if (result != GRANIT_SUCCESS || system == GRANIT_NULL_HANDLE)
    return 1;
  return granit_window_system_destroy(system) == GRANIT_SUCCESS ? 0 : 2;
#else
  return (result == GRANIT_ERROR_UNSUPPORTED || result == GRANIT_ERROR_BACKEND_UNAVAILABLE) &&
                 system == GRANIT_NULL_HANDLE
             ? 0
             : 1;
#endif
}
