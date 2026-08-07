// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_SWAPCHAIN_H_
#define GRANIT_SWAPCHAIN_H_

#include <stddef.h>
#include <stdint.h>

#include <granit/export.h>
#include <granit/renderer.h>
#include <granit/result.h>
#include <granit/surface.h>
#include <granit/types.h>

/** 窗口交换链句柄。零值无效。 */
typedef granit_handle granit_swapchain;

typedef uint32_t granit_present_mode;
#define GRANIT_PRESENT_MODE_FIFO UINT32_C(0)
#define GRANIT_PRESENT_MODE_MAILBOX UINT32_C(1)
#define GRANIT_PRESENT_MODE_IMMEDIATE UINT32_C(2)

typedef struct granit_swapchain_desc {
  uint32_t struct_size;
  uint32_t width;
  uint32_t height;
  uint32_t minimum_image_count;
  granit_present_mode present_mode;
} granit_swapchain_desc;

#define GRANIT_SWAPCHAIN_DESC_VERSION_1_SIZE                                                       \
  ((uint32_t)(offsetof(granit_swapchain_desc, present_mode) + sizeof(granit_present_mode)))

#define GRANIT_SWAPCHAIN_DESC_INIT                                                                 \
  {(uint32_t)sizeof(granit_swapchain_desc), UINT32_C(1), UINT32_C(1), UINT32_C(0),                 \
   GRANIT_PRESENT_MODE_FIFO}

typedef struct granit_swapchain_info {
  uint32_t struct_size;
  uint32_t width;
  uint32_t height;
  uint32_t image_count;
  granit_present_mode present_mode;
} granit_swapchain_info;

#define GRANIT_SWAPCHAIN_INFO_VERSION_1_SIZE                                                       \
  ((uint32_t)(offsetof(granit_swapchain_info, present_mode) + sizeof(granit_present_mode)))

#define GRANIT_SWAPCHAIN_INFO_INIT                                                                 \
  {(uint32_t)sizeof(granit_swapchain_info), UINT32_C(0), UINT32_C(0), UINT32_C(0),                 \
   GRANIT_PRESENT_MODE_FIFO}

#ifdef __cplusplus
extern "C" {
#endif

/** 为指定 Surface 创建 Swapchain。 */
GRANIT_API granit_result granit_swapchain_create(granit_renderer renderer, granit_surface surface,
                                                 const granit_swapchain_desc* desc,
                                                 granit_swapchain* swapchain);
/** 原子地重建 Swapchain；失败时保留原对象。 */
GRANIT_API granit_result granit_swapchain_recreate(granit_renderer renderer,
                                                   granit_swapchain swapchain,
                                                   const granit_swapchain_desc* desc);
/** 查询实际尺寸、图像数量和呈现模式。 */
GRANIT_API granit_result granit_swapchain_get_info(granit_renderer renderer,
                                                   granit_swapchain swapchain,
                                                   granit_swapchain_info* info);
/** 销毁属于指定 Renderer 的 Swapchain，并使句柄立即失效。 */
GRANIT_API granit_result granit_swapchain_destroy(granit_renderer renderer,
                                                  granit_swapchain swapchain);

#ifdef __cplusplus
}
#endif

#endif
