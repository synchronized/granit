// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_RENDERER_H_
#define GRANIT_RENDERER_H_

#include <stddef.h>
#include <stdint.h>

#include <granit/export.h>
#include <granit/result.h>
#include <granit/types.h>

/** Renderer 对象句柄。零值无效。 */
typedef granit_handle granit_renderer;

#define GRANIT_RENDERER_API_VERSION_1 UINT32_C(1)
#define GRANIT_RENDERER_API_VERSION_CURRENT GRANIT_RENDERER_API_VERSION_1

#define GRANIT_RENDERER_ENABLE_VALIDATION_BIT (UINT32_C(1) << 0)

#define GRANIT_SURFACE_TYPE_WIN32_BIT (UINT32_C(1) << 0)

#define GRANIT_DEFAULT_FRAMES_IN_FLIGHT UINT32_C(2)
#define GRANIT_MAX_FRAMES_IN_FLIGHT UINT32_C(4)

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
} granit_renderer_desc;

#define GRANIT_RENDERER_DESC_VERSION_1_SIZE                                                        \
  ((uint32_t)(offsetof(granit_renderer_desc, flags) + sizeof(uint32_t)))
#define GRANIT_RENDERER_DESC_VERSION_2_SIZE                                                        \
  ((uint32_t)(offsetof(granit_renderer_desc, surface_types) + sizeof(uint32_t)))
#define GRANIT_RENDERER_DESC_VERSION_3_SIZE                                                        \
  ((uint32_t)(offsetof(granit_renderer_desc, reserved) + sizeof(uint32_t)))

#define GRANIT_RENDERER_DESC_INIT                                                                  \
  {(uint32_t)sizeof(granit_renderer_desc),                                                         \
   GRANIT_RENDERER_API_VERSION_CURRENT,                                                            \
   0,                                                                                              \
   UINT32_C(0),                                                                                    \
   UINT32_C(0),                                                                                    \
   UINT32_C(0),                                                                                    \
   GRANIT_DEFAULT_FRAMES_IN_FLIGHT,                                                                \
   UINT32_C(0)}

#ifdef __cplusplus
extern "C" {
#endif

/** 创建 renderer。成功后由调用者通过 granit_renderer_destroy 销毁。 */
GRANIT_API granit_result granit_renderer_create(const granit_renderer_desc* desc,
                                                granit_renderer* renderer);

/** 销毁 renderer，并使句柄立即失效。 */
GRANIT_API granit_result granit_renderer_destroy(granit_renderer renderer);

#ifdef __cplusplus
}
#endif

#endif
