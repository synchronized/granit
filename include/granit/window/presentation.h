// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_WINDOW_PRESENTATION_H_
#define GRANIT_WINDOW_PRESENTATION_H_

#include <granit/renderer/surface.h>
#include <granit/window/window.h>

#ifdef __cplusplus
extern "C" {
#endif

/** 从指定 Window 创建 Renderer 拥有的 Surface；Surface 必须先于 Window 销毁。 */
GRANIT_WINDOW_API granit_result granit_window_create_surface(granit_window_system window_system,
                                                             granit_window window,
                                                             granit_renderer renderer,
                                                             granit_surface* surface);

#ifdef __cplusplus
}
#endif

#endif
