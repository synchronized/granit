// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_RENDERER_NATIVE_SURFACE_H_
#define GRANIT_RENDERER_NATIVE_SURFACE_H_

#include <stddef.h>
#include <stdint.h>

#include <granit/renderer/surface.h>

#define GRANIT_SURFACE_TYPE_WIN32_BIT (UINT32_C(1) << 0)
#define GRANIT_SURFACE_TYPE_XCB_BIT (UINT32_C(1) << 1)
#define GRANIT_SURFACE_TYPE_WAYLAND_BIT (UINT32_C(1) << 2)
#define GRANIT_SURFACE_TYPE_CANVAS_BIT (UINT32_C(1) << 3)

/** Win32 Surface 来源。instance 与 window 必须保持有效直到 Surface 销毁。 */
typedef struct granit_win32_surface_source {
  void* instance;
  void* window;
} granit_win32_surface_source;

/** XCB Surface 来源。connection 与 window 必须保持有效直到 Surface 销毁。 */
typedef struct granit_xcb_surface_source {
  void* connection;
  uint32_t window;
  uint32_t reserved;
} granit_xcb_surface_source;

/** Wayland Surface 来源。display 与 surface 必须保持有效直到 Surface 销毁。 */
typedef struct granit_wayland_surface_source {
  void* display;
  void* surface;
} granit_wayland_surface_source;

/** 浏览器 Canvas Surface 来源；selector 仅在创建调用期间借用。 */
typedef struct granit_canvas_surface_source {
  const char* selector;
  uint32_t selector_length;
  uint32_t reserved;
} granit_canvas_surface_source;

/** Surface 平台来源。reserved_data 仅用于固定联合体尺寸。 */
typedef union granit_surface_source {
  uint64_t reserved_data[4];
  granit_win32_surface_source win32;
  granit_xcb_surface_source xcb;
  granit_wayland_surface_source wayland;
  granit_canvas_surface_source canvas;
} granit_surface_source;

/** 统一 Surface 创建描述；surface_type 必须恰好包含一个 GRANIT_SURFACE_TYPE_*_BIT。 */
typedef struct granit_surface_desc {
  uint32_t struct_size;
  uint32_t surface_type;
  uint32_t flags;
  uint32_t reserved;
  granit_surface_source source;
} granit_surface_desc;

#define GRANIT_SURFACE_DESC_VERSION_1_SIZE                                                         \
  ((uint32_t)(offsetof(granit_surface_desc, source) + sizeof(granit_surface_source)))
#define GRANIT_SURFACE_DESC_INIT                                                                   \
  {                                                                                                \
    (uint32_t)sizeof(granit_surface_desc), UINT32_C(0), UINT32_C(0), UINT32_C(0), {                \
      {                                                                                            \
        UINT64_C(0)                                                                                \
      }                                                                                            \
    }                                                                                              \
  }

#ifdef __cplusplus
extern "C" {
#endif

/** 从描述的平台窗口或 Canvas 创建 Surface。Renderer 必须预先启用对应来源支持。 */
GRANIT_API granit_result granit_surface_create(granit_renderer renderer,
                                               const granit_surface_desc* desc,
                                               granit_surface* surface);

#ifdef __cplusplus
}
#endif

#endif
