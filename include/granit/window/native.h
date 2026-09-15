// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_WINDOW_NATIVE_H_
#define GRANIT_WINDOW_NATIVE_H_

#include <granit/window/window.h>

#ifdef __cplusplus
extern "C" {
#endif

/** 借用 Win32 原生值；仅在 Window 存活期间有效。 */
GRANIT_WINDOW_API granit_result granit_window_get_win32(granit_window_system window_system,
                                                        granit_window window, void** instance,
                                                        void** native_window);
/** 借用 XCB 原生值；仅在 Window 存活期间有效。 */
GRANIT_WINDOW_API granit_result granit_window_get_xcb(granit_window_system window_system,
                                                      granit_window window, void** connection,
                                                      uint32_t* native_window);
/** 借用 Wayland 原生值；仅在 Window 存活期间有效。 */
GRANIT_WINDOW_API granit_result granit_window_get_wayland(granit_window_system window_system,
                                                          granit_window window, void** display,
                                                          void** native_surface);

#ifdef __cplusplus
}
#endif

#endif
