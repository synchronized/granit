// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_WINDOW_WINDOW_H_
#define GRANIT_WINDOW_WINDOW_H_

#include <stddef.h>
#include <stdint.h>

#include <granit/core/result.h>
#include <granit/core/types.h>
#include <granit/window/export.h>

typedef granit_handle granit_window_system;
typedef granit_handle granit_window;

typedef enum granit_window_backend {
  GRANIT_WINDOW_BACKEND_AUTO = 0,
  GRANIT_WINDOW_BACKEND_WIN32 = 1,
  GRANIT_WINDOW_BACKEND_XCB = 2,
  GRANIT_WINDOW_BACKEND_WAYLAND = 3
} granit_window_backend;

#define GRANIT_WINDOW_VISIBLE_BIT (UINT32_C(1) << 0)
#define GRANIT_WINDOW_RESIZABLE_BIT (UINT32_C(1) << 1)
#define GRANIT_WINDOW_HIGH_DPI_BIT (UINT32_C(1) << 2)

typedef struct granit_window_system_desc {
  uint32_t struct_size;
  uint32_t backend;
  uint32_t flags;
  uint32_t reserved;
} granit_window_system_desc;

#define GRANIT_WINDOW_SYSTEM_DESC_VERSION_1_SIZE                                                   \
  ((uint32_t)(offsetof(granit_window_system_desc, reserved) + sizeof(uint32_t)))
#define GRANIT_WINDOW_SYSTEM_DESC_INIT                                                             \
  {(uint32_t)sizeof(granit_window_system_desc), GRANIT_WINDOW_BACKEND_AUTO, UINT32_C(0),            \
   UINT32_C(0)}

typedef struct granit_window_desc {
  uint32_t struct_size;
  const char* title;
  uint32_t title_length;
  uint32_t width;
  uint32_t height;
  uint32_t flags;
  uint32_t reserved;
} granit_window_desc;

#define GRANIT_WINDOW_DESC_VERSION_1_SIZE                                                          \
  ((uint32_t)(offsetof(granit_window_desc, reserved) + sizeof(uint32_t)))
#define GRANIT_WINDOW_DESC_INIT                                                                    \
  {(uint32_t)sizeof(granit_window_desc), 0, UINT32_C(0), UINT32_C(0), UINT32_C(0),                  \
   GRANIT_WINDOW_VISIBLE_BIT | GRANIT_WINDOW_RESIZABLE_BIT, UINT32_C(0)}

typedef enum granit_window_event_type {
  GRANIT_WINDOW_EVENT_CLOSE_REQUESTED = 1,
  GRANIT_WINDOW_EVENT_RESIZED = 2,
  GRANIT_WINDOW_EVENT_FOCUS_CHANGED = 3,
  GRANIT_WINDOW_EVENT_SCALE_CHANGED = 4,
  GRANIT_WINDOW_EVENT_NATIVE_HANDLE_CHANGED = 5
} granit_window_event_type;

typedef union granit_window_event_data {
  struct {
    uint32_t width;
    uint32_t height;
  } resized;
  struct {
    uint32_t focused;
    uint32_t reserved;
  } focus;
  struct {
    float horizontal;
    float vertical;
    uint32_t width;
    uint32_t height;
  } scale;
  struct {
    uint32_t backend;
    uint32_t reserved;
  } native_handle;
  uint8_t reserved[32];
} granit_window_event_data;

typedef struct granit_window_event {
  uint32_t struct_size;
  uint32_t type;
  granit_window window;
  uint64_t timestamp_ns;
  granit_window_event_data data;
} granit_window_event;

#define GRANIT_WINDOW_EVENT_VERSION_1_SIZE                                                         \
  ((uint32_t)(offsetof(granit_window_event, data) + sizeof(granit_window_event_data)))
#define GRANIT_WINDOW_EVENT_INIT                                                                   \
  {(uint32_t)sizeof(granit_window_event), UINT32_C(0), GRANIT_NULL_HANDLE, UINT64_C(0), {{0, 0}}}

#ifdef __cplusplus
extern "C" {
#endif

GRANIT_WINDOW_API granit_result granit_window_system_create(
    const granit_window_system_desc* desc, granit_window_system* window_system);
GRANIT_WINDOW_API granit_result granit_window_system_destroy(granit_window_system window_system);
GRANIT_WINDOW_API granit_result granit_window_poll_event(granit_window_system window_system,
                                                         granit_window_event* event);
GRANIT_WINDOW_API granit_result granit_window_create(granit_window_system window_system,
                                                     const granit_window_desc* desc,
                                                     granit_window* window);
GRANIT_WINDOW_API granit_result granit_window_destroy(granit_window_system window_system,
                                                      granit_window window);
GRANIT_WINDOW_API granit_result granit_window_get_win32(granit_window_system window_system,
                                                        granit_window window, void** instance,
                                                        void** native_window);
GRANIT_WINDOW_API granit_result granit_window_get_xcb(granit_window_system window_system,
                                                      granit_window window, void** connection,
                                                      uint32_t* native_window);
GRANIT_WINDOW_API granit_result granit_window_get_wayland(granit_window_system window_system,
                                                          granit_window window, void** display,
                                                          void** native_surface);

#ifdef __cplusplus
}
#endif

#endif
