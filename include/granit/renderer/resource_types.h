// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_RESOURCE_TYPES_H_
#define GRANIT_RESOURCE_TYPES_H_

#include <stdint.h>

/** 资源的 CPU/GPU 访问意图；不表示具体 Vulkan 内存类型。 */
typedef uint32_t granit_memory_location;
#define GRANIT_MEMORY_LOCATION_AUTOMATIC UINT32_C(0)
#define GRANIT_MEMORY_LOCATION_DEVICE UINT32_C(1)
#define GRANIT_MEMORY_LOCATION_UPLOAD UINT32_C(2)
#define GRANIT_MEMORY_LOCATION_READBACK UINT32_C(3)

/** Buffer 可以参与的操作位集合。 */
typedef uint32_t granit_buffer_usage;
#define GRANIT_BUFFER_USAGE_TRANSFER_SOURCE_BIT (UINT32_C(1) << 0)
#define GRANIT_BUFFER_USAGE_TRANSFER_DESTINATION_BIT (UINT32_C(1) << 1)
#define GRANIT_BUFFER_USAGE_VERTEX_BIT (UINT32_C(1) << 2)
#define GRANIT_BUFFER_USAGE_INDEX_BIT (UINT32_C(1) << 3)
#define GRANIT_BUFFER_USAGE_UNIFORM_BIT (UINT32_C(1) << 4)
#define GRANIT_BUFFER_USAGE_STORAGE_BIT (UINT32_C(1) << 5)
#define GRANIT_BUFFER_USAGE_INDIRECT_BIT (UINT32_C(1) << 6)

/** Texture 或 Texture View 的维度。 */
typedef uint32_t granit_texture_dimension;
#define GRANIT_TEXTURE_DIMENSION_1D UINT32_C(1)
#define GRANIT_TEXTURE_DIMENSION_2D UINT32_C(2)
#define GRANIT_TEXTURE_DIMENSION_3D UINT32_C(3)
#define GRANIT_TEXTURE_DIMENSION_CUBE UINT32_C(4)

/** Texture 可以参与的操作位集合。 */
typedef uint32_t granit_texture_usage;
#define GRANIT_TEXTURE_USAGE_TRANSFER_SOURCE_BIT (UINT32_C(1) << 0)
#define GRANIT_TEXTURE_USAGE_TRANSFER_DESTINATION_BIT (UINT32_C(1) << 1)
#define GRANIT_TEXTURE_USAGE_SAMPLED_BIT (UINT32_C(1) << 2)
#define GRANIT_TEXTURE_USAGE_STORAGE_BIT (UINT32_C(1) << 3)
#define GRANIT_TEXTURE_USAGE_COLOR_ATTACHMENT_BIT (UINT32_C(1) << 4)
#define GRANIT_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT (UINT32_C(1) << 5)

/** 后端无关的像素格式。数值不对应 VkFormat。 */
typedef uint32_t granit_texture_format;
#define GRANIT_TEXTURE_FORMAT_UNDEFINED UINT32_C(0)
#define GRANIT_TEXTURE_FORMAT_R8_UNORM UINT32_C(1)
#define GRANIT_TEXTURE_FORMAT_RG8_UNORM UINT32_C(2)
#define GRANIT_TEXTURE_FORMAT_RGBA8_UNORM UINT32_C(3)
#define GRANIT_TEXTURE_FORMAT_RGBA8_SRGB UINT32_C(4)
#define GRANIT_TEXTURE_FORMAT_BGRA8_UNORM UINT32_C(5)
#define GRANIT_TEXTURE_FORMAT_BGRA8_SRGB UINT32_C(6)
#define GRANIT_TEXTURE_FORMAT_RGBA16_FLOAT UINT32_C(7)
#define GRANIT_TEXTURE_FORMAT_D16_UNORM UINT32_C(8)
#define GRANIT_TEXTURE_FORMAT_D32_FLOAT UINT32_C(9)
#define GRANIT_TEXTURE_FORMAT_D24_UNORM_S8_UINT UINT32_C(10)
#define GRANIT_TEXTURE_FORMAT_D32_FLOAT_S8_UINT UINT32_C(11)

/** 一个格式块的紧密排列信息；当前非压缩格式的块宽高均为 1。 */
typedef struct granit_texture_format_footprint {
  uint32_t struct_size;
  uint32_t block_width;
  uint32_t block_height;
  uint32_t bytes_per_block;
  uint32_t reserved[4];
} granit_texture_format_footprint;
#define GRANIT_TEXTURE_FORMAT_FOOTPRINT_VERSION_1_SIZE UINT32_C(32)
#define GRANIT_TEXTURE_FORMAT_FOOTPRINT_INIT                                                   \
  {GRANIT_TEXTURE_FORMAT_FOOTPRINT_VERSION_1_SIZE, UINT32_C(0), UINT32_C(0), UINT32_C(0), {   \
    UINT32_C(0), UINT32_C(0), UINT32_C(0), UINT32_C(0)                                        \
  }}

/** 每个像素的样本数。 */
typedef uint32_t granit_sample_count;
#define GRANIT_SAMPLE_COUNT_1 UINT32_C(1)
#define GRANIT_SAMPLE_COUNT_2 UINT32_C(2)
#define GRANIT_SAMPLE_COUNT_4 UINT32_C(4)
#define GRANIT_SAMPLE_COUNT_8 UINT32_C(8)

/** Texture View 选择的颜色、深度或模板平面。 */
typedef uint32_t granit_texture_aspect;
#define GRANIT_TEXTURE_ASPECT_AUTOMATIC UINT32_C(0)
#define GRANIT_TEXTURE_ASPECT_COLOR_BIT (UINT32_C(1) << 0)
#define GRANIT_TEXTURE_ASPECT_DEPTH_BIT (UINT32_C(1) << 1)
#define GRANIT_TEXTURE_ASPECT_STENCIL_BIT (UINT32_C(1) << 2)

/** Texture View 采样结果的分量来源；数值不对应 Vulkan 枚举。 */
typedef uint32_t granit_component_swizzle;
#define GRANIT_COMPONENT_SWIZZLE_IDENTITY UINT32_C(0)
#define GRANIT_COMPONENT_SWIZZLE_ZERO UINT32_C(1)
#define GRANIT_COMPONENT_SWIZZLE_ONE UINT32_C(2)
#define GRANIT_COMPONENT_SWIZZLE_RED UINT32_C(3)
#define GRANIT_COMPONENT_SWIZZLE_GREEN UINT32_C(4)
#define GRANIT_COMPONENT_SWIZZLE_BLUE UINT32_C(5)
#define GRANIT_COMPONENT_SWIZZLE_ALPHA UINT32_C(6)

#define GRANIT_REMAINING_MIP_LEVELS UINT32_MAX
#define GRANIT_REMAINING_ARRAY_LAYERS UINT32_MAX

/** 放大或缩小时使用的过滤方式。 */
typedef uint32_t granit_filter;
#define GRANIT_FILTER_NEAREST UINT32_C(0)
#define GRANIT_FILTER_LINEAR UINT32_C(1)

/** mip 层级之间的过滤方式。 */
typedef uint32_t granit_mipmap_filter;
#define GRANIT_MIPMAP_FILTER_NEAREST UINT32_C(0)
#define GRANIT_MIPMAP_FILTER_LINEAR UINT32_C(1)

/** 采样坐标超出标准范围时的处理方式。 */
typedef uint32_t granit_address_mode;
#define GRANIT_ADDRESS_MODE_REPEAT UINT32_C(0)
#define GRANIT_ADDRESS_MODE_MIRRORED_REPEAT UINT32_C(1)
#define GRANIT_ADDRESS_MODE_CLAMP_TO_EDGE UINT32_C(2)

/** 采样比较操作；DISABLED 表示不启用比较。 */
typedef uint32_t granit_compare_operation;
#define GRANIT_COMPARE_OPERATION_DISABLED UINT32_C(0)
#define GRANIT_COMPARE_OPERATION_NEVER UINT32_C(1)
#define GRANIT_COMPARE_OPERATION_LESS UINT32_C(2)
#define GRANIT_COMPARE_OPERATION_EQUAL UINT32_C(3)
#define GRANIT_COMPARE_OPERATION_LESS_EQUAL UINT32_C(4)
#define GRANIT_COMPARE_OPERATION_GREATER UINT32_C(5)
#define GRANIT_COMPARE_OPERATION_NOT_EQUAL UINT32_C(6)
#define GRANIT_COMPARE_OPERATION_GREATER_EQUAL UINT32_C(7)
#define GRANIT_COMPARE_OPERATION_ALWAYS UINT32_C(8)

/** Buffer 值描述。创建函数只在调用期间读取该结构。 */
typedef struct granit_buffer_desc {
  uint32_t struct_size;
  granit_buffer_usage usage;
  granit_memory_location memory_location;
  uint32_t reserved;
  uint64_t size;
  uint64_t reserved_2;
} granit_buffer_desc;
#define GRANIT_BUFFER_DESC_VERSION_1_SIZE UINT32_C(32)
#define GRANIT_BUFFER_DESC_INIT                                                                    \
  {GRANIT_BUFFER_DESC_VERSION_1_SIZE,                                                              \
   UINT32_C(0),                                                                                    \
   GRANIT_MEMORY_LOCATION_AUTOMATIC,                                                               \
   UINT32_C(0),                                                                                    \
   UINT64_C(0),                                                                                    \
   UINT64_C(0)}

/** Texture 存储描述；不包含 View 或初始数据所有权。 */
typedef struct granit_texture_desc {
  uint32_t struct_size;
  granit_texture_dimension dimension;
  granit_texture_format format;
  granit_texture_usage usage;
  granit_memory_location memory_location;
  uint32_t width;
  uint32_t height;
  uint32_t depth;
  uint32_t mip_levels;
  uint32_t array_layers;
  granit_sample_count sample_count;
  uint32_t reserved;
} granit_texture_desc;
#define GRANIT_TEXTURE_DESC_VERSION_1_SIZE UINT32_C(48)
#define GRANIT_TEXTURE_DESC_INIT                                                                   \
  {GRANIT_TEXTURE_DESC_VERSION_1_SIZE,                                                             \
   GRANIT_TEXTURE_DIMENSION_2D,                                                                    \
   GRANIT_TEXTURE_FORMAT_UNDEFINED,                                                                \
   UINT32_C(0),                                                                                    \
   GRANIT_MEMORY_LOCATION_AUTOMATIC,                                                               \
   UINT32_C(1),                                                                                    \
   UINT32_C(1),                                                                                    \
   UINT32_C(1),                                                                                    \
   UINT32_C(1),                                                                                    \
   UINT32_C(1),                                                                                    \
   GRANIT_SAMPLE_COUNT_1,                                                                          \
   UINT32_C(0)}

/** Texture 写入源数据布局；0 行跨度表示按写入宽度或高度紧密排列。 */
typedef struct granit_texture_data_layout {
  uint64_t offset;
  uint32_t bytes_per_row;
  uint32_t rows_per_image;
} granit_texture_data_layout;

/** Texture 写入目标子资源和区域。 */
typedef struct granit_texture_write_region {
  uint32_t mip_level;
  uint32_t base_array_layer;
  uint32_t array_layer_count;
  granit_texture_aspect aspect;
  uint32_t x;
  uint32_t y;
  uint32_t z;
  uint32_t width;
  uint32_t height;
  uint32_t depth;
} granit_texture_write_region;

/** Texture View 使用的 mip 和数组层范围。 */
typedef struct granit_subresource_range {
  granit_texture_aspect aspect;
  uint32_t base_mip_level;
  uint32_t mip_level_count;
  uint32_t base_array_layer;
  uint32_t array_layer_count;
} granit_subresource_range;

/** Texture View 的 RGBA 输出分量映射。 */
typedef struct granit_component_mapping {
  granit_component_swizzle red;
  granit_component_swizzle green;
  granit_component_swizzle blue;
  granit_component_swizzle alpha;
} granit_component_mapping;

/** Texture View 描述；父 Texture 由未来创建函数单独传入。 */
typedef struct granit_texture_view_desc {
  uint32_t struct_size;
  granit_texture_dimension dimension;
  granit_texture_format format;
  uint32_t reserved;
  granit_subresource_range range;
  granit_component_mapping components;
} granit_texture_view_desc;
#define GRANIT_TEXTURE_VIEW_DESC_VERSION_1_SIZE UINT32_C(52)
#define GRANIT_TEXTURE_VIEW_DESC_INIT                                                              \
  {GRANIT_TEXTURE_VIEW_DESC_VERSION_1_SIZE,                                                        \
   GRANIT_TEXTURE_DIMENSION_2D,                                                                    \
   GRANIT_TEXTURE_FORMAT_UNDEFINED,                                                                \
   UINT32_C(0),                                                                                    \
   {GRANIT_TEXTURE_ASPECT_AUTOMATIC, UINT32_C(0), UINT32_C(1), UINT32_C(0), UINT32_C(1)},          \
   {GRANIT_COMPONENT_SWIZZLE_IDENTITY, GRANIT_COMPONENT_SWIZZLE_IDENTITY,                          \
    GRANIT_COMPONENT_SWIZZLE_IDENTITY, GRANIT_COMPONENT_SWIZZLE_IDENTITY}}

/** 独立 Sampler 状态描述。 */
typedef struct granit_sampler_desc {
  uint32_t struct_size;
  granit_filter mag_filter;
  granit_filter min_filter;
  granit_mipmap_filter mipmap_filter;
  granit_address_mode address_mode_u;
  granit_address_mode address_mode_v;
  granit_address_mode address_mode_w;
  granit_compare_operation compare_operation;
  uint32_t anisotropy_enabled;
  float max_anisotropy;
  float lod_bias;
  float min_lod;
  float max_lod;
  uint32_t reserved;
} granit_sampler_desc;
#define GRANIT_SAMPLER_DESC_VERSION_1_SIZE UINT32_C(56)
#define GRANIT_SAMPLER_DESC_INIT                                                                   \
  {GRANIT_SAMPLER_DESC_VERSION_1_SIZE,                                                             \
   GRANIT_FILTER_LINEAR,                                                                           \
   GRANIT_FILTER_LINEAR,                                                                           \
   GRANIT_MIPMAP_FILTER_LINEAR,                                                                    \
   GRANIT_ADDRESS_MODE_REPEAT,                                                                     \
   GRANIT_ADDRESS_MODE_REPEAT,                                                                     \
   GRANIT_ADDRESS_MODE_REPEAT,                                                                     \
   GRANIT_COMPARE_OPERATION_DISABLED,                                                              \
   UINT32_C(0),                                                                                    \
   1.0F,                                                                                           \
   0.0F,                                                                                           \
   0.0F,                                                                                           \
   0.0F,                                                                                           \
   UINT32_C(0)}

#endif
