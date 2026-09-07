// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_TEXTURE_ASSET_H_
#define GRANIT_TEXTURE_ASSET_H_

#include <stdint.h>

#include <granit/core/export.h>
#include <granit/core/result.h>
#include <granit/renderer/renderer.h>
#include <granit/renderer/resource_types.h>
#include <granit/renderer/texture.h>
#include <granit/renderer/upload_batch.h>

#define GRANIT_TEXTURE_ASSET_ID_SIZE UINT32_C(32)
#define GRANIT_TEXTURE_ASSET_SCHEMA_VERSION UINT32_C(1)

/** Texture Asset 中单个 GPU 格式变体的只读摘要。 */
typedef struct granit_texture_asset_variant_info {
  granit_texture_format format;
  granit_texture_usage usage;
  uint32_t first_subresource;
  uint32_t subresource_count;
  uint64_t payload_offset;
  uint64_t payload_size;
  uint8_t payload_digest[GRANIT_TEXTURE_ASSET_ID_SIZE];
  uint32_t reserved[2];
} granit_texture_asset_variant_info;

/** Texture Asset 中一个完整 mip 与数组层的源数据布局。 */
typedef struct granit_texture_asset_subresource_info {
  uint32_t mip_level;
  uint32_t array_layer;
  uint64_t data_offset;
  uint64_t data_size;
  uint32_t bytes_per_row;
  uint32_t rows_per_image;
  uint32_t reserved[2];
} granit_texture_asset_subresource_info;

/** 已验证 Texture Asset Manifest 摘要；数组内存由调用方提供并持有。 */
typedef struct granit_texture_asset_info {
  uint32_t struct_size;
  uint32_t schema_version;
  uint8_t content_id[GRANIT_TEXTURE_ASSET_ID_SIZE];
  granit_texture_dimension dimension;
  uint32_t width;
  uint32_t height;
  uint32_t depth;
  uint32_t array_layers;
  uint32_t mip_levels;
  uint32_t variant_count;
  uint32_t subresource_count;
  granit_texture_asset_variant_info* variants;
  uint32_t variant_capacity;
  uint32_t reserved;
  granit_texture_asset_subresource_info* subresources;
  uint32_t subresource_capacity;
  uint32_t reserved_2;
} granit_texture_asset_info;

#define GRANIT_TEXTURE_ASSET_INFO_SIZE ((uint32_t)sizeof(granit_texture_asset_info))
#define GRANIT_TEXTURE_ASSET_INFO_INIT                                                             \
  {GRANIT_TEXTURE_ASSET_INFO_SIZE,                                                                 \
   UINT32_C(0),                                                                                    \
   {0},                                                                                            \
   UINT32_C(0),                                                                                    \
   UINT32_C(0),                                                                                    \
   UINT32_C(0),                                                                                    \
   UINT32_C(0),                                                                                    \
   UINT32_C(0),                                                                                    \
   UINT32_C(0),                                                                                    \
   UINT32_C(0),                                                                                    \
   UINT32_C(0),                                                                                    \
   0,                                                                                              \
   UINT32_C(0),                                                                                    \
   UINT32_C(0),                                                                                    \
   0,                                                                                              \
   UINT32_C(0),                                                                                    \
   UINT32_C(0)}

/** 变体选择条件；required_usage 必须全部由设备支持。 */
typedef struct granit_texture_asset_selection_desc {
  uint32_t struct_size;
  granit_texture_usage required_usage;
  granit_texture_format_feature_flags required_features;
  uint32_t reserved;
} granit_texture_asset_selection_desc;

#define GRANIT_TEXTURE_ASSET_SELECTION_DESC_SIZE                                                   \
  ((uint32_t)sizeof(granit_texture_asset_selection_desc))
#define GRANIT_TEXTURE_ASSET_SELECTION_DESC_INIT                                                   \
  {GRANIT_TEXTURE_ASSET_SELECTION_DESC_SIZE, GRANIT_TEXTURE_USAGE_SAMPLED_BIT, UINT32_C(0),        \
   UINT32_C(0)}

/** 设备选择出的变体；variant_index 对应 Manifest 顺序。 */
typedef struct granit_texture_asset_selection {
  uint32_t struct_size;
  uint32_t variant_index;
  granit_texture_format format;
  uint32_t reserved;
  uint64_t payload_offset;
  uint64_t payload_size;
} granit_texture_asset_selection;

#define GRANIT_TEXTURE_ASSET_SELECTION_SIZE ((uint32_t)sizeof(granit_texture_asset_selection))
#define GRANIT_TEXTURE_ASSET_SELECTION_INIT                                                        \
  {GRANIT_TEXTURE_ASSET_SELECTION_SIZE,                                                            \
   UINT32_MAX,                                                                                     \
   GRANIT_TEXTURE_FORMAT_UNDEFINED,                                                                \
   UINT32_C(0),                                                                                    \
   UINT64_C(0),                                                                                    \
   UINT64_C(0)}

#ifdef __cplusplus
extern "C" {
#endif

/** 严格校验 Manifest 并返回元数据；首次可传空数组查询所需数量。 */
GRANIT_API granit_result granit_texture_asset_inspect(const void* manifest_data,
                                                      uint64_t manifest_size,
                                                      granit_texture_asset_info* info);

/**
 * 将调用方提供的元数据编码为确定性 v1 Manifest。manifest_data 为 NULL 时只返回所需容量；
 * manifest_size 输入容量并始终返回所需容量。
 */
GRANIT_API granit_result granit_texture_asset_encode(const granit_texture_asset_info* info,
                                                     void* manifest_data,
                                                     uint64_t* manifest_size);

/** 按 Manifest 顺序选择首个满足当前设备能力及调用方条件的变体。 */
GRANIT_API granit_result granit_renderer_select_texture_asset_variant(
    granit_renderer renderer, const void* manifest_data, uint64_t manifest_size,
    const granit_texture_asset_selection_desc* desc, granit_texture_asset_selection* selection);

/**
 * 校验所选变体负载，并把指定 mip 范围原子加入空 Upload Batch。
 * 函数成功后不再引用 manifest_data 或 payload_data；提交、取消和背压沿用 Batch 契约。
 */
GRANIT_API granit_result granit_upload_batch_write_texture_asset_mips(
    granit_renderer renderer, granit_upload_batch batch, granit_texture texture,
    const void* manifest_data, uint64_t manifest_size, const void* payload_data,
    uint64_t payload_size, uint32_t variant_index, uint32_t first_mip, uint32_t mip_count);

#ifdef __cplusplus
}
#endif

#endif
