// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_WINDOW_NATIVE_H_
#define GRANIT_WINDOW_NATIVE_H_

#include <stddef.h>
#include <stdint.h>

#include <granit/window/window.h>

/** 同一次查询获得的 Win32 原生值快照；所有指针均为借用。 */
typedef struct granit_window_native_win32 {
  uint32_t struct_size;
  uint32_t reserved;
  void* instance;
  void* window;
} granit_window_native_win32;

#define GRANIT_WINDOW_NATIVE_WIN32_VERSION_1_SIZE                                                  \
  ((uint32_t)(offsetof(granit_window_native_win32, window) + sizeof(void*)))
#define GRANIT_WINDOW_NATIVE_WIN32_INIT                                                            \
  {(uint32_t)sizeof(granit_window_native_win32), UINT32_C(0), 0, 0}

/** 同一次查询获得的 XCB 原生值快照；connection 为借用，window 为 XCB 整数标识。 */
typedef struct granit_window_native_xcb {
  uint32_t struct_size;
  uint32_t window;
  void* connection;
} granit_window_native_xcb;

#define GRANIT_WINDOW_NATIVE_XCB_VERSION_1_SIZE                                                    \
  ((uint32_t)(offsetof(granit_window_native_xcb, connection) + sizeof(void*)))
#define GRANIT_WINDOW_NATIVE_XCB_INIT {(uint32_t)sizeof(granit_window_native_xcb), UINT32_C(0), 0}

/** 同一次查询获得的 Wayland 原生值快照；所有指针均为借用。 */
typedef struct granit_window_native_wayland {
  uint32_t struct_size;
  uint32_t reserved;
  void* display;
  void* surface;
} granit_window_native_wayland;

#define GRANIT_WINDOW_NATIVE_WAYLAND_VERSION_1_SIZE                                                \
  ((uint32_t)(offsetof(granit_window_native_wayland, surface) + sizeof(void*)))
#define GRANIT_WINDOW_NATIVE_WAYLAND_INIT                                                          \
  {(uint32_t)sizeof(granit_window_native_wayland), UINT32_C(0), 0, 0}

/** Emscripten Canvas 身份快照；selector 是未必以空字符结尾的借用字节序列。 */
typedef struct granit_window_native_emscripten {
  uint32_t struct_size;
  uint32_t canvas_selector_length;
  const char* canvas_selector;
} granit_window_native_emscripten;

#define GRANIT_WINDOW_NATIVE_EMSCRIPTEN_VERSION_1_SIZE                                             \
  ((uint32_t)(offsetof(granit_window_native_emscripten, canvas_selector) + sizeof(const char*)))
#define GRANIT_WINDOW_NATIVE_EMSCRIPTEN_INIT                                                       \
  {(uint32_t)sizeof(granit_window_native_emscripten), UINT32_C(0), 0}

#ifdef __cplusplus
extern "C" {
#endif

/** 返回借用的 Win32 原生值；输出仅在 Window 存活期间有效。 */
GRANIT_WINDOW_API granit_result granit_window_get_native_win32(granit_window_system window_system,
                                                               granit_window window,
                                                               granit_window_native_win32* output);

/** 返回借用的 XCB 原生值；输出仅在 Window 存活期间有效。 */
GRANIT_WINDOW_API granit_result granit_window_get_native_xcb(granit_window_system window_system,
                                                             granit_window window,
                                                             granit_window_native_xcb* output);

/** 返回借用的 Wayland 原生值；输出仅在 Window 存活期间有效。 */
GRANIT_WINDOW_API granit_result granit_window_get_native_wayland(
    granit_window_system window_system, granit_window window, granit_window_native_wayland* output);

/** 返回借用的 Emscripten Canvas selector；输出仅在 Window 存活期间有效。 */
GRANIT_WINDOW_API granit_result
granit_window_get_native_emscripten(granit_window_system window_system, granit_window window,
                                    granit_window_native_emscripten* output);

#ifdef __cplusplus
}
#endif

#endif
