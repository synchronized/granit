// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_RESOURCE_TABLE_H_
#define GRANIT_RESOURCE_TABLE_H_

#include <stdint.h>

#include <granit/core/export.h>
#include <granit/core/result.h>
#include <granit/core/types.h>
#include <granit/renderer/renderer.h>

/** Resource Table 实验性句柄；零值无效，当前 0.x 版本不保证兼容。 */
typedef granit_handle granit_resource_table;

typedef uint32_t granit_resource_table_binding_mode;
#define GRANIT_RESOURCE_TABLE_BINDING_MODE_AUTO UINT32_C(0)
#define GRANIT_RESOURCE_TABLE_BINDING_MODE_EXPLICIT UINT32_C(1)

typedef uint32_t granit_resource_table_support;
#define GRANIT_RESOURCE_TABLE_SUPPORT_UNSUPPORTED UINT32_C(0)
/** 仅表示公共契约和 CPU 生命周期可用，不表示已接入 GPU 绑定。 */
#define GRANIT_RESOURCE_TABLE_SUPPORT_LOGICAL UINT32_C(1)
#define GRANIT_RESOURCE_TABLE_SUPPORT_NATIVE UINT32_C(2)
#define GRANIT_RESOURCE_TABLE_SUPPORT_FALLBACK UINT32_C(3)

typedef uint32_t granit_resource_table_resource_type;
#define GRANIT_RESOURCE_TABLE_RESOURCE_SAMPLED_TEXTURE_VIEW UINT32_C(1)
#define GRANIT_RESOURCE_TABLE_RESOURCE_SAMPLER UINT32_C(2)
#define GRANIT_RESOURCE_TABLE_RESOURCE_STORAGE_BUFFER UINT32_C(3)

#define GRANIT_RESOURCE_TABLE_RESOURCE_SAMPLED_TEXTURE_VIEW_BIT (UINT32_C(1) << 0)
#define GRANIT_RESOURCE_TABLE_RESOURCE_SAMPLER_BIT (UINT32_C(1) << 1)
#define GRANIT_RESOURCE_TABLE_RESOURCE_STORAGE_BUFFER_BIT (UINT32_C(1) << 2)

typedef struct granit_resource_table_capabilities {
  uint32_t struct_size;
  granit_resource_table_support support;
  uint32_t resource_types;
  uint32_t max_sampled_texture_views;
  uint32_t max_samplers;
  uint32_t max_storage_buffers;
  uint32_t reserved[2];
} granit_resource_table_capabilities;

#define GRANIT_RESOURCE_TABLE_CAPABILITIES_VERSION_1_SIZE UINT32_C(32)
#define GRANIT_RESOURCE_TABLE_CAPABILITIES_INIT                                               \
  {GRANIT_RESOURCE_TABLE_CAPABILITIES_VERSION_1_SIZE, GRANIT_RESOURCE_TABLE_SUPPORT_UNSUPPORTED, \
   UINT32_C(0), UINT32_C(0), UINT32_C(0), UINT32_C(0), {UINT32_C(0), UINT32_C(0)}}

/** 创建逻辑 Resource Table；显式模式在后端不支持时返回 GRANIT_ERROR_UNSUPPORTED。 */
typedef struct granit_resource_table_desc {
  uint32_t struct_size;
  granit_resource_table_binding_mode binding_mode;
  uint32_t resource_types;
  uint32_t sampled_texture_view_capacity;
  uint32_t sampler_capacity;
  uint32_t storage_buffer_capacity;
  uint32_t reserved[2];
} granit_resource_table_desc;

#define GRANIT_RESOURCE_TABLE_DESC_VERSION_1_SIZE UINT32_C(32)
#define GRANIT_RESOURCE_TABLE_DESC_INIT                                                          \
  {GRANIT_RESOURCE_TABLE_DESC_VERSION_1_SIZE, GRANIT_RESOURCE_TABLE_BINDING_MODE_AUTO,          \
   UINT32_C(0), UINT32_C(0), UINT32_C(0), UINT32_C(0), {UINT32_C(0), UINT32_C(0)}}

/** 一次槽位更新；resource 为零表示清空槽位。Storage Buffer 的范围以字节表示。 */
typedef struct granit_resource_table_update_desc {
  granit_resource_table_resource_type type;
  uint32_t slot;
  granit_handle resource;
  uint64_t offset;
  uint64_t size;
} granit_resource_table_update_desc;

#ifdef __cplusplus
extern "C" {
#endif

GRANIT_API granit_result granit_renderer_get_resource_table_capabilities(
    granit_renderer renderer, granit_resource_table_capabilities* capabilities);

GRANIT_API granit_result granit_resource_table_create(
    granit_renderer renderer, const granit_resource_table_desc* desc,
    granit_resource_table* table);

GRANIT_API granit_result granit_resource_table_update(
    granit_renderer renderer, granit_resource_table table,
    const granit_resource_table_update_desc* update);

GRANIT_API granit_result granit_resource_table_update_batch(
    granit_renderer renderer, granit_resource_table table,
    const granit_resource_table_update_desc* updates, uint32_t update_count);

GRANIT_API granit_result granit_resource_table_destroy(granit_renderer renderer,
                                                       granit_resource_table table);

#ifdef __cplusplus
}
#endif

#endif
