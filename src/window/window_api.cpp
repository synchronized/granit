// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/window/window.h>

extern "C" granit_result granit_window_system_create(const granit_window_system_desc* desc,
                                                     granit_window_system* window_system) {
  if (window_system == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  *window_system = GRANIT_NULL_HANDLE;
  if (desc == nullptr || desc->struct_size < GRANIT_WINDOW_SYSTEM_DESC_VERSION_1_SIZE)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  return GRANIT_ERROR_UNSUPPORTED;
}

extern "C" granit_result granit_window_system_destroy(granit_window_system) {
  return GRANIT_ERROR_INVALID_HANDLE;
}

extern "C" granit_result granit_window_poll_event(granit_window_system,
                                                  granit_window_event* event) {
  if (event == nullptr || event->struct_size < GRANIT_WINDOW_EVENT_VERSION_1_SIZE)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  return GRANIT_ERROR_INVALID_HANDLE;
}

extern "C" granit_result granit_window_create(granit_window_system, const granit_window_desc* desc,
                                              granit_window* window) {
  if (window == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  *window = GRANIT_NULL_HANDLE;
  if (desc == nullptr || desc->struct_size < GRANIT_WINDOW_DESC_VERSION_1_SIZE)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  return GRANIT_ERROR_INVALID_HANDLE;
}

extern "C" granit_result granit_window_destroy(granit_window_system, granit_window) {
  return GRANIT_ERROR_INVALID_HANDLE;
}

extern "C" granit_result granit_window_get_win32(granit_window_system, granit_window,
                                                 void** instance, void** native_window) {
  if (instance == nullptr || native_window == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  *instance = nullptr;
  *native_window = nullptr;
  return GRANIT_ERROR_INVALID_HANDLE;
}

extern "C" granit_result granit_window_get_xcb(granit_window_system, granit_window,
                                               void** connection, uint32_t* native_window) {
  if (connection == nullptr || native_window == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  *connection = nullptr;
  *native_window = 0;
  return GRANIT_ERROR_INVALID_HANDLE;
}

extern "C" granit_result granit_window_get_wayland(granit_window_system, granit_window,
                                                   void** display, void** native_surface) {
  if (display == nullptr || native_surface == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  *display = nullptr;
  *native_surface = nullptr;
  return GRANIT_ERROR_INVALID_HANDLE;
}
