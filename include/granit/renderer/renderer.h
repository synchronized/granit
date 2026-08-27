// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_RENDERER_H_
#define GRANIT_RENDERER_H_

#include <stddef.h>
#include <stdint.h>

#include <granit/core/diagnostic.h>
#include <granit/core/export.h>
#include <granit/core/result.h>
#include <granit/core/types.h>

/** Renderer 对象句柄。零值无效。 */
typedef granit_handle granit_renderer;

#define GRANIT_RENDERER_API_VERSION_1 UINT32_C(1)
#define GRANIT_RENDERER_API_VERSION_CURRENT GRANIT_RENDERER_API_VERSION_1

#define GRANIT_RENDERER_ENABLE_VALIDATION_BIT (UINT32_C(1) << 0)

#define GRANIT_SURFACE_TYPE_WIN32_BIT (UINT32_C(1) << 0)
#define GRANIT_SURFACE_TYPE_XCB_BIT (UINT32_C(1) << 1)
#define GRANIT_SURFACE_TYPE_WAYLAND_BIT (UINT32_C(1) << 2)

#define GRANIT_DEFAULT_FRAMES_IN_FLIGHT UINT32_C(2)
#define GRANIT_MAX_FRAMES_IN_FLIGHT UINT32_C(4)

/** Renderer 对应设备的公开限制快照。 */
typedef struct granit_renderer_limits {
  uint32_t struct_size;
  uint32_t reserved;
  uint64_t uniform_buffer_offset_alignment;
  uint64_t max_uniform_buffer_binding_size;
} granit_renderer_limits;

#define GRANIT_RENDERER_LIMITS_VERSION_1_SIZE                                                      \
  ((uint32_t)(offsetof(granit_renderer_limits, max_uniform_buffer_binding_size) + sizeof(uint64_t)))

#define GRANIT_RENDERER_LIMITS_INIT                                                                \
  {(uint32_t)sizeof(granit_renderer_limits), UINT32_C(0), UINT64_C(0), UINT64_C(0)}

/** Renderer 创建描述。字符串以显式长度表示，不要求调用者提供结尾零字符。 */
typedef struct granit_renderer_desc {
  uint32_t struct_size;
  uint32_t api_version;
  const char* application_name;
  uint32_t application_name_length;
  uint32_t flags;
  uint32_t surface_types;
  uint32_t frames_in_flight;
  uint32_t reserved;
  granit_diagnostic_callback diagnostic_callback;
  void* diagnostic_user_data;
} granit_renderer_desc;

#define GRANIT_RENDERER_DESC_VERSION_1_SIZE                                                        \
  ((uint32_t)(offsetof(granit_renderer_desc, flags) + sizeof(uint32_t)))
#define GRANIT_RENDERER_DESC_VERSION_2_SIZE                                                        \
  ((uint32_t)(offsetof(granit_renderer_desc, surface_types) + sizeof(uint32_t)))
#define GRANIT_RENDERER_DESC_VERSION_3_SIZE                                                        \
  ((uint32_t)(offsetof(granit_renderer_desc, reserved) + sizeof(uint32_t)))
#define GRANIT_RENDERER_DESC_VERSION_4_SIZE                                                        \
  ((uint32_t)(offsetof(granit_renderer_desc, diagnostic_user_data) + sizeof(void*)))

#define GRANIT_RENDERER_DESC_INIT                                                                  \
  {(uint32_t)sizeof(granit_renderer_desc),                                                         \
   GRANIT_RENDERER_API_VERSION_CURRENT,                                                            \
   0,                                                                                              \
   UINT32_C(0),                                                                                    \
   UINT32_C(0),                                                                                    \
   UINT32_C(0),                                                                                    \
   GRANIT_DEFAULT_FRAMES_IN_FLIGHT,                                                                \
   UINT32_C(0),                                                                                    \
   0,                                                                                              \
   0}

#ifdef __cplusplus
extern "C" {
#endif

/** 创建 renderer。成功后由调用者通过 granit_renderer_destroy 销毁。 */
GRANIT_API granit_result granit_renderer_create(const granit_renderer_desc* desc,
                                                granit_renderer* renderer);

/** 销毁 renderer，并使句柄立即失效。 */
GRANIT_API granit_result granit_renderer_destroy(granit_renderer renderer);

/** 查询 Renderer 对应设备的限制；调用者须先设置 limits->struct_size。 */
GRANIT_API granit_result granit_renderer_get_limits(granit_renderer renderer,
                                                    granit_renderer_limits* limits);

/** 为公开 GPU 对象设置 UTF-8 调试名称；名称仅在调用期间借用。 */
GRANIT_API granit_result granit_renderer_set_object_name(granit_renderer renderer,
                                                         granit_handle object, const char* name,
                                                         uint32_t name_length);

/** 将驱动相关的临时 Pipeline Cache 数据合并到 Renderer；调用返回后不保留 data。 */
GRANIT_API granit_result granit_renderer_pipeline_cache_import(granit_renderer renderer,
                                                               const void* data, uint64_t size);

/** 导出 Pipeline Cache；data 为空时查询所需大小，size 同时作为容量输入和实际大小输出。 */
GRANIT_API granit_result granit_renderer_pipeline_cache_export(granit_renderer renderer, void* data,
                                                               uint64_t* size);

#ifdef __cplusplus
}
#endif

#endif
