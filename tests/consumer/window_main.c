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
  granit_window_desc window_desc = GRANIT_WINDOW_DESC_INIT;
  window_desc.width = UINT32_C(320);
  window_desc.height = UINT32_C(240);
  granit_window window = GRANIT_NULL_HANDLE;
  if (granit_window_create(system, &window_desc, &window) != GRANIT_SUCCESS ||
      window == GRANIT_NULL_HANDLE)
    return 2;
  granit_window_state state = GRANIT_WINDOW_STATE_INIT;
  if (granit_window_get_state(system, window, &state) != GRANIT_SUCCESS || state.width == 0 ||
      state.height == 0 || state.framebuffer_width == 0 || state.framebuffer_height == 0 ||
      state.content_scale_horizontal <= 0.0F || state.content_scale_vertical <= 0.0F)
    return 3;
  if (granit_window_destroy(system, window) != GRANIT_SUCCESS)
    return 4;
  if (granit_window_system_destroy(system) != GRANIT_SUCCESS)
    return 5;
  return granit_window_system_destroy(system) == GRANIT_ERROR_INVALID_HANDLE ? 0 : 6;
#else
  return (result == GRANIT_ERROR_UNSUPPORTED || result == GRANIT_ERROR_BACKEND_UNAVAILABLE) &&
                 system == GRANIT_NULL_HANDLE
             ? 0
             : 1;
#endif
}
