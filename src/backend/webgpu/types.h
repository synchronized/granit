// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_WEBGPU_TYPES_H_
#define GRANIT_WEBGPU_TYPES_H_

#include <cstdint>
#include <functional>
#include <type_traits>

#include <granit/core/diagnostic.h>
#include <granit/core/result.h>

#define GRANIT_WEBGPU_SURFACE_TYPE_WIN32_BIT UINT32_C(0x00000001)
#define GRANIT_WEBGPU_SURFACE_TYPE_XCB_BIT UINT32_C(0x00000002)
#define GRANIT_WEBGPU_SURFACE_TYPE_WAYLAND_BIT UINT32_C(0x00000004)
#define GRANIT_WEBGPU_SURFACE_TYPE_CANVAS_BIT UINT32_C(0x00000008)

/** Context 内部资源句柄；Tag 阻止不同资源类型相互混用。 */
template <typename Tag> struct webgpu_handle {
  std::uint64_t value{};

  constexpr webgpu_handle() noexcept = default;
  constexpr webgpu_handle(std::uint64_t handle) noexcept : value(handle) {}

  [[nodiscard]] explicit constexpr operator std::uint64_t() const noexcept { return value; }
  [[nodiscard]] constexpr bool operator==(const webgpu_handle&) const noexcept = default;
};

namespace std {

template <typename Tag> struct hash<webgpu_handle<Tag>> {
  [[nodiscard]] constexpr std::size_t operator()(webgpu_handle<Tag> handle) const noexcept {
    return std::hash<std::uint64_t>{}(handle.value);
  }
};

} // namespace std

using webgpu_instance_handle = webgpu_handle<struct webgpu_instance_tag>;
using webgpu_buffer = webgpu_handle<struct webgpu_buffer_tag>;
using webgpu_texture = webgpu_handle<struct webgpu_texture_tag>;
using webgpu_texture_view = webgpu_handle<struct webgpu_texture_view_tag>;
using webgpu_sampler = webgpu_handle<struct webgpu_sampler_tag>;
using webgpu_bind_group_layout = webgpu_handle<struct webgpu_bind_group_layout_tag>;
using webgpu_bind_group = webgpu_handle<struct webgpu_bind_group_tag>;
using webgpu_shader = webgpu_handle<struct webgpu_shader_tag>;
using webgpu_pipeline_layout = webgpu_handle<struct webgpu_pipeline_layout_tag>;
using webgpu_render_pipeline = webgpu_handle<struct webgpu_render_pipeline_tag>;
using webgpu_compute_pipeline = webgpu_handle<struct webgpu_compute_pipeline_tag>;
using webgpu_command_recorder = webgpu_handle<struct webgpu_command_recorder_tag>;
using webgpu_command_buffer = webgpu_handle<struct webgpu_command_buffer_tag>;
using webgpu_surface = webgpu_handle<struct webgpu_surface_tag>;
using webgpu_swapchain = webgpu_handle<struct webgpu_swapchain_tag>;
using webgpu_timestamp_query_pool = webgpu_handle<struct webgpu_timestamp_query_pool_tag>;
using webgpu_readback = webgpu_handle<struct webgpu_readback_tag>;
using webgpu_pipeline_warmup = webgpu_handle<struct webgpu_pipeline_warmup_tag>;

static_assert(!std::is_convertible_v<webgpu_buffer, webgpu_texture>);
static_assert(!std::is_convertible_v<webgpu_shader, webgpu_render_pipeline>);

#define GRANIT_WEBGPU_FEATURE_TIMESTAMP_QUERY_BIT (UINT64_C(1) << 0)

/** Draw 调用期间借用的顶点 Buffer 绑定。 */
typedef struct webgpu_vertex_buffer_binding {
  webgpu_buffer buffer;
  uint64_t offset;
} webgpu_vertex_buffer_binding;

typedef struct webgpu_viewport {
  float x;
  float y;
  float width;
  float height;
  float min_depth;
  float max_depth;
} webgpu_viewport;

typedef struct webgpu_scissor {
  uint32_t x;
  uint32_t y;
  uint32_t width;
  uint32_t height;
} webgpu_scissor;

typedef uint32_t webgpu_index_format;
#define GRANIT_WEBGPU_INDEX_FORMAT_UINT16 UINT32_C(1)
#define GRANIT_WEBGPU_INDEX_FORMAT_UINT32 UINT32_C(2)

typedef uint32_t webgpu_load_operation;
#define GRANIT_WEBGPU_LOAD_OPERATION_LOAD UINT32_C(1)
#define GRANIT_WEBGPU_LOAD_OPERATION_CLEAR UINT32_C(2)
#define GRANIT_WEBGPU_LOAD_OPERATION_DISCARD UINT32_C(3)
typedef uint32_t webgpu_store_operation;
#define GRANIT_WEBGPU_STORE_OPERATION_STORE UINT32_C(1)
#define GRANIT_WEBGPU_STORE_OPERATION_DISCARD UINT32_C(2)

typedef uint32_t webgpu_instance_state;
#define GRANIT_WEBGPU_INSTANCE_STATE_INITIALIZING UINT32_C(1)
#define GRANIT_WEBGPU_INSTANCE_STATE_READY UINT32_C(2)
#define GRANIT_WEBGPU_INSTANCE_STATE_FAILED UINT32_C(3)
#define GRANIT_WEBGPU_INSTANCE_STATE_DEVICE_LOST UINT32_C(4)

/** WebGPU 实例当前生命周期快照；failure_result 仅在失败或设备丢失状态下非成功。 */
typedef struct webgpu_instance_status {
  uint32_t struct_size;
  webgpu_instance_state state;
  granit_result failure_result;
  uint32_t reserved;
} webgpu_instance_status;

typedef uint32_t webgpu_buffer_usage;
#define GRANIT_WEBGPU_BUFFER_USAGE_MAP_READ_BIT UINT32_C(0x00000001)
#define GRANIT_WEBGPU_BUFFER_USAGE_COPY_SRC_BIT UINT32_C(0x00000002)
#define GRANIT_WEBGPU_BUFFER_USAGE_COPY_DST_BIT UINT32_C(0x00000004)
#define GRANIT_WEBGPU_BUFFER_USAGE_VERTEX_BIT UINT32_C(0x00000008)
#define GRANIT_WEBGPU_BUFFER_USAGE_INDEX_BIT UINT32_C(0x00000010)
#define GRANIT_WEBGPU_BUFFER_USAGE_UNIFORM_BIT UINT32_C(0x00000020)
#define GRANIT_WEBGPU_BUFFER_USAGE_STORAGE_BIT UINT32_C(0x00000040)

/** Buffer 由创建它的 Context 拥有；size 必须非零。 */
typedef struct webgpu_buffer_desc {
  uint32_t struct_size;
  uint32_t reserved;
  uint64_t size;
  webgpu_buffer_usage usage;
  uint32_t reserved_flags;
} webgpu_buffer_desc;

typedef uint32_t webgpu_texture_usage;
#define GRANIT_WEBGPU_TEXTURE_USAGE_COPY_SRC_BIT UINT32_C(0x00000001)
#define GRANIT_WEBGPU_TEXTURE_USAGE_COPY_DST_BIT UINT32_C(0x00000002)
#define GRANIT_WEBGPU_TEXTURE_USAGE_SAMPLED_BIT UINT32_C(0x00000004)
#define GRANIT_WEBGPU_TEXTURE_USAGE_RENDER_ATTACHMENT_BIT UINT32_C(0x00000008)

typedef uint32_t webgpu_texture_format;

typedef uint32_t webgpu_texture_dimension;
#define GRANIT_WEBGPU_TEXTURE_DIMENSION_2D UINT32_C(1)
#define GRANIT_WEBGPU_TEXTURE_DIMENSION_CUBE UINT32_C(2)

typedef struct webgpu_texture_desc {
  uint32_t struct_size;
  uint32_t reserved;
  uint32_t width;
  uint32_t height;
  webgpu_texture_usage usage;
  webgpu_texture_format format;
  uint32_t mip_level_count;
  webgpu_texture_dimension dimension;
  uint32_t array_layer_count;
  uint32_t sample_count;
} webgpu_texture_desc;

typedef struct webgpu_texture_view_desc {
  uint32_t struct_size;
  webgpu_texture_format format;
  uint32_t base_mip_level;
  uint32_t mip_level_count;
  webgpu_texture_dimension dimension;
  uint32_t base_array_layer;
  uint32_t array_layer_count;
} webgpu_texture_view_desc;

/** 数据指针从首个有效字节开始；行跨度为零表示按写入宽度紧密排列。 */
typedef struct webgpu_texture_write_desc {
  uint32_t struct_size;
  uint32_t mip_level;
  uint32_t x;
  uint32_t y;
  uint32_t width;
  uint32_t height;
  uint32_t bytes_per_row;
  uint32_t rows_per_image;
  uint32_t base_array_layer;
  uint32_t array_layer_count;
} webgpu_texture_write_desc;

typedef uint32_t webgpu_upload_type;
#define GRANIT_WEBGPU_UPLOAD_TYPE_BUFFER UINT32_C(1)
#define GRANIT_WEBGPU_UPLOAD_TYPE_TEXTURE UINT32_C(2)

/** 批量上传调用期间借用 data；未使用的资源句柄和字段必须为零。 */
typedef struct webgpu_upload_operation {
  uint32_t struct_size;
  webgpu_upload_type type;
  webgpu_buffer buffer;
  webgpu_texture texture;
  uint64_t destination_offset;
  webgpu_texture_write_desc texture_write;
  const void* data;
  uint64_t size;
  uint64_t reserved;
} webgpu_upload_operation;

typedef uint32_t webgpu_filter;
#define GRANIT_WEBGPU_FILTER_NEAREST UINT32_C(1)
#define GRANIT_WEBGPU_FILTER_LINEAR UINT32_C(2)

typedef uint32_t webgpu_address_mode;
#define GRANIT_WEBGPU_ADDRESS_MODE_REPEAT UINT32_C(1)
#define GRANIT_WEBGPU_ADDRESS_MODE_MIRROR_REPEAT UINT32_C(2)
#define GRANIT_WEBGPU_ADDRESS_MODE_CLAMP_TO_EDGE UINT32_C(3)

typedef uint32_t webgpu_compare_operation;
#define GRANIT_WEBGPU_COMPARE_OPERATION_DISABLED UINT32_C(0)
#define GRANIT_WEBGPU_COMPARE_OPERATION_NEVER UINT32_C(1)
#define GRANIT_WEBGPU_COMPARE_OPERATION_LESS UINT32_C(2)
#define GRANIT_WEBGPU_COMPARE_OPERATION_EQUAL UINT32_C(3)
#define GRANIT_WEBGPU_COMPARE_OPERATION_LESS_EQUAL UINT32_C(4)
#define GRANIT_WEBGPU_COMPARE_OPERATION_GREATER UINT32_C(5)
#define GRANIT_WEBGPU_COMPARE_OPERATION_NOT_EQUAL UINT32_C(6)
#define GRANIT_WEBGPU_COMPARE_OPERATION_GREATER_EQUAL UINT32_C(7)
#define GRANIT_WEBGPU_COMPARE_OPERATION_ALWAYS UINT32_C(8)

typedef struct webgpu_sampler_desc {
  uint32_t struct_size;
  uint32_t reserved;
  webgpu_filter min_filter;
  webgpu_filter mag_filter;
  webgpu_filter mipmap_filter;
  webgpu_address_mode address_mode_u;
  webgpu_address_mode address_mode_v;
  webgpu_address_mode address_mode_w;
  webgpu_compare_operation compare_operation;
  uint32_t max_anisotropy;
  float min_lod;
  float max_lod;
  uint32_t reserved_2[2];
} webgpu_sampler_desc;

typedef uint32_t webgpu_binding_type;
#define GRANIT_WEBGPU_BINDING_TYPE_UNIFORM_BUFFER UINT32_C(1)
#define GRANIT_WEBGPU_BINDING_TYPE_DYNAMIC_UNIFORM_BUFFER UINT32_C(2)
#define GRANIT_WEBGPU_BINDING_TYPE_STORAGE_BUFFER UINT32_C(3)
#define GRANIT_WEBGPU_BINDING_TYPE_SAMPLED_TEXTURE UINT32_C(4)
#define GRANIT_WEBGPU_BINDING_TYPE_SAMPLED_TEXTURE_CUBE UINT32_C(6)
#define GRANIT_WEBGPU_BINDING_TYPE_SAMPLER UINT32_C(5)
#define GRANIT_WEBGPU_BINDING_TYPE_COMPARISON_SAMPLER UINT32_C(7)
#define GRANIT_WEBGPU_BINDING_TYPE_SAMPLED_DEPTH_TEXTURE UINT32_C(8)

typedef struct webgpu_bind_group_layout_entry {
  uint32_t binding;
  webgpu_binding_type type;
  uint32_t visibility;
  uint32_t array_count;
} webgpu_bind_group_layout_entry;

typedef struct webgpu_bind_group_layout_desc {
  uint32_t struct_size;
  uint32_t entry_count;
  const webgpu_bind_group_layout_entry* entries;
  uint64_t reserved;
} webgpu_bind_group_layout_desc;

typedef struct webgpu_bind_group_entry {
  uint32_t binding;
  webgpu_binding_type type;
  webgpu_buffer buffer;
  webgpu_texture_view texture_view;
  webgpu_sampler sampler;
  uint64_t offset;
  uint64_t size;
} webgpu_bind_group_entry;

typedef struct webgpu_bind_group_desc {
  uint32_t struct_size;
  uint32_t entry_count;
  webgpu_bind_group_layout layout;
  const webgpu_bind_group_entry* entries;
  uint64_t reserved;
} webgpu_bind_group_desc;

typedef uint32_t webgpu_shader_stage;
#define GRANIT_WEBGPU_SHADER_STAGE_VERTEX UINT32_C(1)
#define GRANIT_WEBGPU_SHADER_STAGE_FRAGMENT UINT32_C(2)
#define GRANIT_WEBGPU_SHADER_STAGE_COMPUTE UINT32_C(3)

/** WGSL 字节和入口点仅在调用期间有效，Context 必须复制所需内容。 */
typedef struct webgpu_shader_desc {
  uint32_t struct_size;
  webgpu_shader_stage stage;
  const char* wgsl;
  uint64_t wgsl_length;
  const char* entry_point;
  uint64_t entry_point_length;
} webgpu_shader_desc;

typedef uint32_t webgpu_vertex_format;
#define GRANIT_WEBGPU_VERTEX_FORMAT_FLOAT32 UINT32_C(1)
#define GRANIT_WEBGPU_VERTEX_FORMAT_FLOAT32X2 UINT32_C(2)
#define GRANIT_WEBGPU_VERTEX_FORMAT_FLOAT32X3 UINT32_C(3)
#define GRANIT_WEBGPU_VERTEX_FORMAT_FLOAT32X4 UINT32_C(4)
#define GRANIT_WEBGPU_VERTEX_FORMAT_UINT32 UINT32_C(5)
#define GRANIT_WEBGPU_VERTEX_FORMAT_UINT32X2 UINT32_C(6)
#define GRANIT_WEBGPU_VERTEX_FORMAT_UINT32X3 UINT32_C(7)
#define GRANIT_WEBGPU_VERTEX_FORMAT_UINT32X4 UINT32_C(8)
#define GRANIT_WEBGPU_VERTEX_FORMAT_SINT32 UINT32_C(9)
#define GRANIT_WEBGPU_VERTEX_FORMAT_SINT32X2 UINT32_C(10)
#define GRANIT_WEBGPU_VERTEX_FORMAT_SINT32X3 UINT32_C(11)
#define GRANIT_WEBGPU_VERTEX_FORMAT_SINT32X4 UINT32_C(12)

typedef uint32_t webgpu_vertex_step_mode;
#define GRANIT_WEBGPU_VERTEX_STEP_MODE_VERTEX UINT32_C(1)
#define GRANIT_WEBGPU_VERTEX_STEP_MODE_INSTANCE UINT32_C(2)

typedef struct webgpu_vertex_attribute {
  uint32_t location;
  webgpu_vertex_format format;
  uint32_t offset;
  uint32_t reserved;
} webgpu_vertex_attribute;

typedef struct webgpu_vertex_buffer_layout {
  uint32_t stride;
  webgpu_vertex_step_mode step_mode;
  uint32_t attribute_count;
  uint32_t reserved;
  const webgpu_vertex_attribute* attributes;
} webgpu_vertex_buffer_layout;

typedef uint32_t webgpu_blend_factor;
#define GRANIT_WEBGPU_BLEND_FACTOR_ZERO UINT32_C(1)
#define GRANIT_WEBGPU_BLEND_FACTOR_ONE UINT32_C(2)
#define GRANIT_WEBGPU_BLEND_FACTOR_SOURCE_COLOR UINT32_C(3)
#define GRANIT_WEBGPU_BLEND_FACTOR_ONE_MINUS_SOURCE_COLOR UINT32_C(4)
#define GRANIT_WEBGPU_BLEND_FACTOR_SOURCE_ALPHA UINT32_C(5)
#define GRANIT_WEBGPU_BLEND_FACTOR_ONE_MINUS_SOURCE_ALPHA UINT32_C(6)
#define GRANIT_WEBGPU_BLEND_FACTOR_DESTINATION_COLOR UINT32_C(7)
#define GRANIT_WEBGPU_BLEND_FACTOR_ONE_MINUS_DESTINATION_COLOR UINT32_C(8)
#define GRANIT_WEBGPU_BLEND_FACTOR_DESTINATION_ALPHA UINT32_C(9)
#define GRANIT_WEBGPU_BLEND_FACTOR_ONE_MINUS_DESTINATION_ALPHA UINT32_C(10)

typedef uint32_t webgpu_blend_operation;
#define GRANIT_WEBGPU_BLEND_OPERATION_ADD UINT32_C(1)
#define GRANIT_WEBGPU_BLEND_OPERATION_SUBTRACT UINT32_C(2)
#define GRANIT_WEBGPU_BLEND_OPERATION_REVERSE_SUBTRACT UINT32_C(3)
#define GRANIT_WEBGPU_BLEND_OPERATION_MIN UINT32_C(4)
#define GRANIT_WEBGPU_BLEND_OPERATION_MAX UINT32_C(5)

#define GRANIT_WEBGPU_COLOR_WRITE_RED_BIT (UINT32_C(1) << 0)
#define GRANIT_WEBGPU_COLOR_WRITE_GREEN_BIT (UINT32_C(1) << 1)
#define GRANIT_WEBGPU_COLOR_WRITE_BLUE_BIT (UINT32_C(1) << 2)
#define GRANIT_WEBGPU_COLOR_WRITE_ALPHA_BIT (UINT32_C(1) << 3)
#define GRANIT_WEBGPU_COLOR_WRITE_ALL_BITS                                                         \
  (GRANIT_WEBGPU_COLOR_WRITE_RED_BIT | GRANIT_WEBGPU_COLOR_WRITE_GREEN_BIT |                       \
   GRANIT_WEBGPU_COLOR_WRITE_BLUE_BIT | GRANIT_WEBGPU_COLOR_WRITE_ALPHA_BIT)

typedef uint32_t webgpu_primitive_topology;
#define GRANIT_WEBGPU_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST UINT32_C(1)

typedef uint32_t webgpu_front_face;
#define GRANIT_WEBGPU_FRONT_FACE_COUNTER_CLOCKWISE UINT32_C(1)
#define GRANIT_WEBGPU_FRONT_FACE_CLOCKWISE UINT32_C(2)

typedef uint32_t webgpu_cull_mode;
#define GRANIT_WEBGPU_CULL_MODE_NONE UINT32_C(1)
#define GRANIT_WEBGPU_CULL_MODE_FRONT UINT32_C(2)
#define GRANIT_WEBGPU_CULL_MODE_BACK UINT32_C(3)

typedef uint32_t webgpu_polygon_mode;
#define GRANIT_WEBGPU_POLYGON_MODE_FILL UINT32_C(1)

typedef struct webgpu_render_pipeline_desc {
  uint32_t struct_size;
  uint32_t reserved;
  webgpu_pipeline_layout layout;
  webgpu_shader vertex_shader;
  webgpu_shader fragment_shader;
  uint32_t color_format;
  uint32_t vertex_buffer_layout_count;
  const webgpu_vertex_buffer_layout* vertex_buffer_layouts;
  webgpu_texture_format depth_stencil_format;
  uint32_t depth_test_enabled;
  uint32_t depth_write_enabled;
  webgpu_compare_operation depth_compare;
  int32_t depth_bias_constant;
  float depth_bias_slope_scale;
  float depth_bias_clamp;
  uint32_t blend_enabled;
  webgpu_blend_factor source_color_factor;
  webgpu_blend_factor destination_color_factor;
  webgpu_blend_operation color_operation;
  webgpu_blend_factor source_alpha_factor;
  webgpu_blend_factor destination_alpha_factor;
  webgpu_blend_operation alpha_operation;
  uint32_t color_write_mask;
  webgpu_primitive_topology topology;
  webgpu_front_face front_face;
  webgpu_cull_mode cull_mode;
  webgpu_polygon_mode polygon_mode;
  uint32_t sample_count;
} webgpu_render_pipeline_desc;

typedef uint32_t webgpu_present_mode;
#define GRANIT_WEBGPU_PRESENT_MODE_FIFO UINT32_C(0)
#define GRANIT_WEBGPU_PRESENT_MODE_MAILBOX UINT32_C(1)
#define GRANIT_WEBGPU_PRESENT_MODE_IMMEDIATE UINT32_C(2)

#define GRANIT_WEBGPU_TEXTURE_FORMAT_RGBA8_UNORM UINT32_C(1)
#define GRANIT_WEBGPU_TEXTURE_FORMAT_BGRA8_UNORM UINT32_C(2)
#define GRANIT_WEBGPU_TEXTURE_FORMAT_R8_UNORM UINT32_C(3)
#define GRANIT_WEBGPU_TEXTURE_FORMAT_RG8_UNORM UINT32_C(4)
#define GRANIT_WEBGPU_TEXTURE_FORMAT_RGBA8_SRGB UINT32_C(5)
#define GRANIT_WEBGPU_TEXTURE_FORMAT_D32_FLOAT UINT32_C(6)
#define GRANIT_WEBGPU_TEXTURE_FORMAT_RGBA16_FLOAT UINT32_C(7)
#define GRANIT_WEBGPU_TEXTURE_FORMAT_BC1_RGBA_UNORM UINT32_C(8)
#define GRANIT_WEBGPU_TEXTURE_FORMAT_BC1_RGBA_SRGB UINT32_C(9)
#define GRANIT_WEBGPU_TEXTURE_FORMAT_BC3_RGBA_UNORM UINT32_C(10)
#define GRANIT_WEBGPU_TEXTURE_FORMAT_BC3_RGBA_SRGB UINT32_C(11)
#define GRANIT_WEBGPU_TEXTURE_FORMAT_BC5_RG_UNORM UINT32_C(12)
#define GRANIT_WEBGPU_TEXTURE_FORMAT_BC7_RGBA_UNORM UINT32_C(13)
#define GRANIT_WEBGPU_TEXTURE_FORMAT_BC7_RGBA_SRGB UINT32_C(14)
#define GRANIT_WEBGPU_TEXTURE_FORMAT_ETC2_RGBA8_UNORM UINT32_C(15)
#define GRANIT_WEBGPU_TEXTURE_FORMAT_ETC2_RGBA8_SRGB UINT32_C(16)
#define GRANIT_WEBGPU_TEXTURE_FORMAT_ASTC_4X4_UNORM UINT32_C(17)
#define GRANIT_WEBGPU_TEXTURE_FORMAT_ASTC_4X4_SRGB UINT32_C(18)

#define GRANIT_WEBGPU_TEXTURE_COMPRESSION_BC_BIT (UINT32_C(1) << 0)
#define GRANIT_WEBGPU_TEXTURE_COMPRESSION_ETC2_BIT (UINT32_C(1) << 1)
#define GRANIT_WEBGPU_TEXTURE_COMPRESSION_ASTC_BIT (UINT32_C(1) << 2)

typedef struct webgpu_swapchain_desc {
  uint32_t struct_size;
  uint32_t width;
  uint32_t height;
  uint32_t minimum_image_count;
  webgpu_present_mode present_mode;
} webgpu_swapchain_desc;

/** Swapchain 实际采用的配置；不支持请求模式时 Context 必须报告降级后的模式。 */
typedef struct webgpu_swapchain_info {
  uint32_t struct_size;
  uint32_t width;
  uint32_t height;
  uint32_t image_count;
  webgpu_present_mode present_mode;
  webgpu_texture_format format;
} webgpu_swapchain_info;

/** Acquire 返回的 Texture/View 由 Swapchain 借出，在帧结束或重建后失效。 */
typedef struct webgpu_acquired_frame {
  uint32_t struct_size;
  uint32_t image_index;
  uint32_t needs_recreate;
  uint32_t reserved;
  webgpu_texture texture;
  webgpu_texture_view view;
} webgpu_acquired_frame;

/** WebGPU 实例创建后固定的能力快照。 */
typedef struct webgpu_capabilities {
  uint32_t struct_size;
  uint32_t reserved;
  uint64_t uniform_buffer_offset_alignment;
  uint64_t storage_buffer_offset_alignment;
  uint64_t max_uniform_buffer_binding_size;
  uint64_t max_storage_buffer_binding_size;
  uint64_t max_buffer_size;
  uint32_t max_texture_dimension_2d;
  uint32_t max_bind_groups;
  uint32_t max_color_attachments;
  uint32_t surface_types;
  uint32_t reserved_2;
  uint32_t framebuffer_sample_counts;
  float max_sampler_anisotropy;
  uint64_t renderer_features;
  uint32_t texture_compression_features;
  uint32_t reserved_3;
} webgpu_capabilities;

typedef void* (*webgpu_allocate_fn)(uint64_t size, uint64_t alignment, void* user_data);
typedef void (*webgpu_deallocate_fn)(void* memory, uint64_t size, uint64_t alignment,
                                     void* user_data);

/**
 * Host 服务及其 user_data 在 WebGPU Context 销毁前保持有效。
 *
 * 回调可能由后端工作线程并发调用。allocate 和 deallocate 必须成对提供；Context 只能用 deallocate
 * 释放同一 Host allocate 返回的内存。
 */
typedef struct webgpu_host_api {
  uint32_t struct_size;
  uint32_t reserved;
  granit_diagnostic_callback diagnostic_callback;
  void* diagnostic_user_data;
  webgpu_allocate_fn allocate;
  webgpu_deallocate_fn deallocate;
  void* allocator_user_data;
} webgpu_host_api;

typedef struct webgpu_pipeline_layout_desc {
  uint32_t struct_size;
  uint32_t bind_group_layout_count;
  const webgpu_bind_group_layout* bind_group_layouts;
  uint64_t reserved;
} webgpu_pipeline_layout_desc;
typedef struct webgpu_compute_pipeline_desc {
  uint32_t struct_size;
  uint32_t reserved;
  webgpu_pipeline_layout layout;
  webgpu_shader shader;
} webgpu_compute_pipeline_desc;
typedef uint32_t webgpu_texture_aspect;
#define GRANIT_WEBGPU_TEXTURE_ASPECT_ALL UINT32_C(0)
#define GRANIT_WEBGPU_TEXTURE_ASPECT_DEPTH UINT32_C(1)
#define GRANIT_WEBGPU_TEXTURE_ASPECT_STENCIL UINT32_C(2)

typedef struct webgpu_buffer_copy_region {
  uint64_t source_offset;
  uint64_t destination_offset;
  uint64_t size;
} webgpu_buffer_copy_region;

/** Buffer 与 Texture 复制区域；布局偏移相对于 Buffer 起始位置。 */
typedef struct webgpu_texture_buffer_copy {
  uint64_t buffer_offset;
  uint32_t bytes_per_row;
  uint32_t rows_per_image;
  uint32_t mip_level;
  uint32_t base_array_layer;
  uint32_t array_layer_count;
  webgpu_texture_aspect aspect;
  uint32_t x;
  uint32_t y;
  uint32_t z;
  uint32_t width;
  uint32_t height;
  uint32_t depth;
} webgpu_texture_buffer_copy;

typedef struct webgpu_texture_copy_region {
  uint32_t source_mip_level;
  uint32_t source_base_array_layer;
  uint32_t destination_mip_level;
  uint32_t destination_base_array_layer;
  uint32_t array_layer_count;
  webgpu_texture_aspect aspect;
  uint32_t source_x;
  uint32_t source_y;
  uint32_t source_z;
  uint32_t destination_x;
  uint32_t destination_y;
  uint32_t destination_z;
  uint32_t width;
  uint32_t height;
  uint32_t depth;
} webgpu_texture_copy_region;

typedef struct webgpu_texture_mipmap_range {
  uint32_t base_mip_level;
  uint32_t level_count;
  uint32_t base_array_layer;
  uint32_t array_layer_count;
} webgpu_texture_mipmap_range;

#endif
