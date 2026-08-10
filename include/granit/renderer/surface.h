// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_SURFACE_H_
#define GRANIT_SURFACE_H_

#include <stddef.h>
#include <stdint.h>

#include <granit/core/export.h>
#include <granit/renderer/renderer.h>
#include <granit/core/result.h>
#include <granit/core/types.h>

/** 窗口输出 Surface 句柄。零值无效。 */
typedef granit_handle granit_surface;

/** Win32 Surface 创建描述。原生句柄只在函数调用期间借用。 */
typedef struct granit_win32_surface_desc {
  uint32_t struct_size;
  void* instance;
  void* window;
} granit_win32_surface_desc;

#define GRANIT_WIN32_SURFACE_DESC_VERSION_1_SIZE                                                   \
  ((uint32_t)(offsetof(granit_win32_surface_desc, window) + sizeof(void*)))

#define GRANIT_WIN32_SURFACE_DESC_INIT {(uint32_t)sizeof(granit_win32_surface_desc), 0, 0}

#ifdef __cplusplus
extern "C" {
#endif

/** 从 Win32 HINSTANCE 与 HWND 创建 Surface。Renderer 必须预先启用对应支持。 */
GRANIT_API granit_result granit_surface_create_win32(granit_renderer renderer,
                                                     const granit_win32_surface_desc* desc,
                                                     granit_surface* surface);

/** 销毁属于指定 Renderer 的 Surface，并使句柄立即失效。 */
GRANIT_API granit_result granit_surface_destroy(granit_renderer renderer, granit_surface surface);

#ifdef __cplusplus
}
#endif

#endif
