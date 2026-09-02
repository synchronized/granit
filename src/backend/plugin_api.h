// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_BACKEND_PLUGIN_API_H_
#define GRANIT_BACKEND_PLUGIN_API_H_

#include <stdint.h>

#include <granit/core/diagnostic.h>
#include <granit/core/result.h>

#define GRANIT_BACKEND_PLUGIN_ABI_VERSION UINT32_C(23)
#define GRANIT_BACKEND_PLUGIN_KIND_WEBGPU UINT32_C(1)
#define GRANIT_BACKEND_PLUGIN_QUERY_SYMBOL "granit_backend_plugin_query"
#define GRANIT_BACKEND_PLUGIN_SURFACE_TYPE_WIN32_BIT UINT32_C(0x00000001)
#define GRANIT_BACKEND_PLUGIN_SURFACE_TYPE_XCB_BIT UINT32_C(0x00000002)
#define GRANIT_BACKEND_PLUGIN_SURFACE_TYPE_WAYLAND_BIT UINT32_C(0x00000004)
#define GRANIT_BACKEND_PLUGIN_SURFACE_TYPE_CANVAS_BIT UINT32_C(0x00000008)

typedef uint32_t granit_backend_plugin_kind;
typedef uint64_t granit_backend_plugin_instance;
typedef uint64_t granit_backend_plugin_buffer;
typedef uint64_t granit_backend_plugin_texture;
typedef uint64_t granit_backend_plugin_texture_view;
typedef uint64_t granit_backend_plugin_sampler;
typedef uint64_t granit_backend_plugin_bind_group_layout;
typedef uint64_t granit_backend_plugin_bind_group;
typedef uint64_t granit_backend_plugin_shader;
typedef uint64_t granit_backend_plugin_pipeline_layout;
typedef uint64_t granit_backend_plugin_render_pipeline;
typedef uint64_t granit_backend_plugin_compute_pipeline;
typedef uint64_t granit_backend_plugin_command_recorder;
typedef uint64_t granit_backend_plugin_command_buffer;
typedef uint64_t granit_backend_plugin_surface;
typedef uint64_t granit_backend_plugin_swapchain;

/** Draw 调用期间借用的顶点 Buffer 绑定。 */
typedef struct granit_backend_plugin_vertex_buffer_binding {
  granit_backend_plugin_buffer buffer;
  uint64_t offset;
} granit_backend_plugin_vertex_buffer_binding;

typedef struct granit_backend_plugin_viewport {
  float x;
  float y;
  float width;
  float height;
  float min_depth;
  float max_depth;
} granit_backend_plugin_viewport;

typedef struct granit_backend_plugin_scissor {
  uint32_t x;
  uint32_t y;
  uint32_t width;
  uint32_t height;
} granit_backend_plugin_scissor;

typedef uint32_t granit_backend_plugin_index_format;
#define GRANIT_BACKEND_PLUGIN_INDEX_FORMAT_UINT16 UINT32_C(1)
#define GRANIT_BACKEND_PLUGIN_INDEX_FORMAT_UINT32 UINT32_C(2)

typedef uint32_t granit_backend_plugin_load_operation;
#define GRANIT_BACKEND_PLUGIN_LOAD_OPERATION_LOAD UINT32_C(1)
#define GRANIT_BACKEND_PLUGIN_LOAD_OPERATION_CLEAR UINT32_C(2)
#define GRANIT_BACKEND_PLUGIN_LOAD_OPERATION_DISCARD UINT32_C(3)
typedef uint32_t granit_backend_plugin_store_operation;
#define GRANIT_BACKEND_PLUGIN_STORE_OPERATION_STORE UINT32_C(1)
#define GRANIT_BACKEND_PLUGIN_STORE_OPERATION_DISCARD UINT32_C(2)

typedef uint32_t granit_backend_plugin_instance_state;
#define GRANIT_BACKEND_PLUGIN_INSTANCE_STATE_INITIALIZING UINT32_C(1)
#define GRANIT_BACKEND_PLUGIN_INSTANCE_STATE_READY UINT32_C(2)
#define GRANIT_BACKEND_PLUGIN_INSTANCE_STATE_FAILED UINT32_C(3)
#define GRANIT_BACKEND_PLUGIN_INSTANCE_STATE_DEVICE_LOST UINT32_C(4)

/** 插件实例当前生命周期快照；failure_result 仅在失败或设备丢失状态下非成功。 */
typedef struct granit_backend_plugin_instance_status {
  uint32_t struct_size;
  granit_backend_plugin_instance_state state;
  granit_result failure_result;
  uint32_t reserved;
} granit_backend_plugin_instance_status;

typedef uint32_t granit_backend_plugin_buffer_usage;
#define GRANIT_BACKEND_PLUGIN_BUFFER_USAGE_MAP_READ_BIT UINT32_C(0x00000001)
#define GRANIT_BACKEND_PLUGIN_BUFFER_USAGE_COPY_SRC_BIT UINT32_C(0x00000002)
#define GRANIT_BACKEND_PLUGIN_BUFFER_USAGE_COPY_DST_BIT UINT32_C(0x00000004)
#define GRANIT_BACKEND_PLUGIN_BUFFER_USAGE_VERTEX_BIT UINT32_C(0x00000008)
#define GRANIT_BACKEND_PLUGIN_BUFFER_USAGE_INDEX_BIT UINT32_C(0x00000010)
#define GRANIT_BACKEND_PLUGIN_BUFFER_USAGE_UNIFORM_BIT UINT32_C(0x00000020)
#define GRANIT_BACKEND_PLUGIN_BUFFER_USAGE_STORAGE_BIT UINT32_C(0x00000040)

/** Buffer 由创建它的插件实例拥有；size 必须非零。 */
typedef struct granit_backend_plugin_buffer_desc {
  uint32_t struct_size;
  uint32_t reserved;
  uint64_t size;
  granit_backend_plugin_buffer_usage usage;
  uint32_t reserved_flags;
} granit_backend_plugin_buffer_desc;

typedef uint32_t granit_backend_plugin_texture_usage;
#define GRANIT_BACKEND_PLUGIN_TEXTURE_USAGE_COPY_SRC_BIT UINT32_C(0x00000001)
#define GRANIT_BACKEND_PLUGIN_TEXTURE_USAGE_COPY_DST_BIT UINT32_C(0x00000002)
#define GRANIT_BACKEND_PLUGIN_TEXTURE_USAGE_SAMPLED_BIT UINT32_C(0x00000004)
#define GRANIT_BACKEND_PLUGIN_TEXTURE_USAGE_RENDER_ATTACHMENT_BIT UINT32_C(0x00000008)

typedef uint32_t granit_backend_plugin_texture_format;

typedef uint32_t granit_backend_plugin_texture_dimension;
#define GRANIT_BACKEND_PLUGIN_TEXTURE_DIMENSION_2D UINT32_C(1)
#define GRANIT_BACKEND_PLUGIN_TEXTURE_DIMENSION_CUBE UINT32_C(2)

typedef struct granit_backend_plugin_texture_desc {
  uint32_t struct_size;
  uint32_t reserved;
  uint32_t width;
  uint32_t height;
  granit_backend_plugin_texture_usage usage;
  granit_backend_plugin_texture_format format;
  uint32_t mip_level_count;
  granit_backend_plugin_texture_dimension dimension;
  uint32_t array_layer_count;
} granit_backend_plugin_texture_desc;

typedef struct granit_backend_plugin_texture_view_desc {
  uint32_t struct_size;
  granit_backend_plugin_texture_format format;
  uint32_t base_mip_level;
  uint32_t mip_level_count;
  granit_backend_plugin_texture_dimension dimension;
  uint32_t base_array_layer;
  uint32_t array_layer_count;
} granit_backend_plugin_texture_view_desc;

/** 数据指针从首个有效字节开始；行跨度为零表示按写入宽度紧密排列。 */
typedef struct granit_backend_plugin_texture_write_desc {
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
} granit_backend_plugin_texture_write_desc;

typedef uint32_t granit_backend_plugin_upload_type;
#define GRANIT_BACKEND_PLUGIN_UPLOAD_TYPE_BUFFER UINT32_C(1)
#define GRANIT_BACKEND_PLUGIN_UPLOAD_TYPE_TEXTURE UINT32_C(2)

/** 批量上传调用期间借用 data；未使用的资源句柄和字段必须为零。 */
typedef struct granit_backend_plugin_upload_operation {
  uint32_t struct_size;
  granit_backend_plugin_upload_type type;
  granit_backend_plugin_buffer buffer;
  granit_backend_plugin_texture texture;
  uint64_t destination_offset;
  granit_backend_plugin_texture_write_desc texture_write;
  const void* data;
  uint64_t size;
  uint64_t reserved;
} granit_backend_plugin_upload_operation;

typedef uint32_t granit_backend_plugin_filter;
#define GRANIT_BACKEND_PLUGIN_FILTER_NEAREST UINT32_C(1)
#define GRANIT_BACKEND_PLUGIN_FILTER_LINEAR UINT32_C(2)

typedef uint32_t granit_backend_plugin_address_mode;
#define GRANIT_BACKEND_PLUGIN_ADDRESS_MODE_REPEAT UINT32_C(1)
#define GRANIT_BACKEND_PLUGIN_ADDRESS_MODE_MIRROR_REPEAT UINT32_C(2)
#define GRANIT_BACKEND_PLUGIN_ADDRESS_MODE_CLAMP_TO_EDGE UINT32_C(3)

typedef uint32_t granit_backend_plugin_compare_operation;
#define GRANIT_BACKEND_PLUGIN_COMPARE_OPERATION_DISABLED UINT32_C(0)
#define GRANIT_BACKEND_PLUGIN_COMPARE_OPERATION_NEVER UINT32_C(1)
#define GRANIT_BACKEND_PLUGIN_COMPARE_OPERATION_LESS UINT32_C(2)
#define GRANIT_BACKEND_PLUGIN_COMPARE_OPERATION_EQUAL UINT32_C(3)
#define GRANIT_BACKEND_PLUGIN_COMPARE_OPERATION_LESS_EQUAL UINT32_C(4)
#define GRANIT_BACKEND_PLUGIN_COMPARE_OPERATION_GREATER UINT32_C(5)
#define GRANIT_BACKEND_PLUGIN_COMPARE_OPERATION_NOT_EQUAL UINT32_C(6)
#define GRANIT_BACKEND_PLUGIN_COMPARE_OPERATION_GREATER_EQUAL UINT32_C(7)
#define GRANIT_BACKEND_PLUGIN_COMPARE_OPERATION_ALWAYS UINT32_C(8)

typedef struct granit_backend_plugin_sampler_desc {
  uint32_t struct_size;
  uint32_t reserved;
  granit_backend_plugin_filter min_filter;
  granit_backend_plugin_filter mag_filter;
  granit_backend_plugin_filter mipmap_filter;
  granit_backend_plugin_address_mode address_mode_u;
  granit_backend_plugin_address_mode address_mode_v;
  granit_backend_plugin_address_mode address_mode_w;
  granit_backend_plugin_compare_operation compare_operation;
  uint32_t max_anisotropy;
  float min_lod;
  float max_lod;
  uint32_t reserved_2[2];
} granit_backend_plugin_sampler_desc;

typedef uint32_t granit_backend_plugin_binding_type;
#define GRANIT_BACKEND_PLUGIN_BINDING_TYPE_UNIFORM_BUFFER UINT32_C(1)
#define GRANIT_BACKEND_PLUGIN_BINDING_TYPE_DYNAMIC_UNIFORM_BUFFER UINT32_C(2)
#define GRANIT_BACKEND_PLUGIN_BINDING_TYPE_STORAGE_BUFFER UINT32_C(3)
#define GRANIT_BACKEND_PLUGIN_BINDING_TYPE_SAMPLED_TEXTURE UINT32_C(4)
#define GRANIT_BACKEND_PLUGIN_BINDING_TYPE_SAMPLED_TEXTURE_CUBE UINT32_C(6)
#define GRANIT_BACKEND_PLUGIN_BINDING_TYPE_SAMPLER UINT32_C(5)
#define GRANIT_BACKEND_PLUGIN_BINDING_TYPE_COMPARISON_SAMPLER UINT32_C(7)
#define GRANIT_BACKEND_PLUGIN_BINDING_TYPE_SAMPLED_DEPTH_TEXTURE UINT32_C(8)

typedef struct granit_backend_plugin_bind_group_layout_entry {
  uint32_t binding;
  granit_backend_plugin_binding_type type;
  uint32_t visibility;
  uint32_t array_count;
} granit_backend_plugin_bind_group_layout_entry;

typedef struct granit_backend_plugin_bind_group_layout_desc {
  uint32_t struct_size;
  uint32_t entry_count;
  const granit_backend_plugin_bind_group_layout_entry* entries;
  uint64_t reserved;
} granit_backend_plugin_bind_group_layout_desc;

typedef struct granit_backend_plugin_bind_group_entry {
  uint32_t binding;
  granit_backend_plugin_binding_type type;
  granit_backend_plugin_buffer buffer;
  granit_backend_plugin_texture_view texture_view;
  granit_backend_plugin_sampler sampler;
  uint64_t offset;
  uint64_t size;
} granit_backend_plugin_bind_group_entry;

typedef struct granit_backend_plugin_bind_group_desc {
  uint32_t struct_size;
  uint32_t entry_count;
  granit_backend_plugin_bind_group_layout layout;
  const granit_backend_plugin_bind_group_entry* entries;
  uint64_t reserved;
} granit_backend_plugin_bind_group_desc;

typedef uint32_t granit_backend_plugin_shader_stage;
#define GRANIT_BACKEND_PLUGIN_SHADER_STAGE_VERTEX UINT32_C(1)
#define GRANIT_BACKEND_PLUGIN_SHADER_STAGE_FRAGMENT UINT32_C(2)
#define GRANIT_BACKEND_PLUGIN_SHADER_STAGE_COMPUTE UINT32_C(3)

/** WGSL 字节和入口点仅在调用期间有效，插件必须复制所需内容。 */
typedef struct granit_backend_plugin_shader_desc {
  uint32_t struct_size;
  granit_backend_plugin_shader_stage stage;
  const char* wgsl;
  uint64_t wgsl_length;
  const char* entry_point;
  uint64_t entry_point_length;
} granit_backend_plugin_shader_desc;

typedef uint32_t granit_backend_plugin_vertex_format;
#define GRANIT_BACKEND_PLUGIN_VERTEX_FORMAT_FLOAT32 UINT32_C(1)
#define GRANIT_BACKEND_PLUGIN_VERTEX_FORMAT_FLOAT32X2 UINT32_C(2)
#define GRANIT_BACKEND_PLUGIN_VERTEX_FORMAT_FLOAT32X3 UINT32_C(3)
#define GRANIT_BACKEND_PLUGIN_VERTEX_FORMAT_FLOAT32X4 UINT32_C(4)
#define GRANIT_BACKEND_PLUGIN_VERTEX_FORMAT_UINT32 UINT32_C(5)
#define GRANIT_BACKEND_PLUGIN_VERTEX_FORMAT_UINT32X2 UINT32_C(6)
#define GRANIT_BACKEND_PLUGIN_VERTEX_FORMAT_UINT32X3 UINT32_C(7)
#define GRANIT_BACKEND_PLUGIN_VERTEX_FORMAT_UINT32X4 UINT32_C(8)
#define GRANIT_BACKEND_PLUGIN_VERTEX_FORMAT_SINT32 UINT32_C(9)
#define GRANIT_BACKEND_PLUGIN_VERTEX_FORMAT_SINT32X2 UINT32_C(10)
#define GRANIT_BACKEND_PLUGIN_VERTEX_FORMAT_SINT32X3 UINT32_C(11)
#define GRANIT_BACKEND_PLUGIN_VERTEX_FORMAT_SINT32X4 UINT32_C(12)

typedef uint32_t granit_backend_plugin_vertex_step_mode;
#define GRANIT_BACKEND_PLUGIN_VERTEX_STEP_MODE_VERTEX UINT32_C(1)
#define GRANIT_BACKEND_PLUGIN_VERTEX_STEP_MODE_INSTANCE UINT32_C(2)

typedef struct granit_backend_plugin_vertex_attribute {
  uint32_t location;
  granit_backend_plugin_vertex_format format;
  uint32_t offset;
  uint32_t reserved;
} granit_backend_plugin_vertex_attribute;

typedef struct granit_backend_plugin_vertex_buffer_layout {
  uint32_t stride;
  granit_backend_plugin_vertex_step_mode step_mode;
  uint32_t attribute_count;
  uint32_t reserved;
  const granit_backend_plugin_vertex_attribute* attributes;
} granit_backend_plugin_vertex_buffer_layout;

typedef uint32_t granit_backend_plugin_blend_factor;
#define GRANIT_BACKEND_PLUGIN_BLEND_FACTOR_ZERO UINT32_C(1)
#define GRANIT_BACKEND_PLUGIN_BLEND_FACTOR_ONE UINT32_C(2)
#define GRANIT_BACKEND_PLUGIN_BLEND_FACTOR_SOURCE_COLOR UINT32_C(3)
#define GRANIT_BACKEND_PLUGIN_BLEND_FACTOR_ONE_MINUS_SOURCE_COLOR UINT32_C(4)
#define GRANIT_BACKEND_PLUGIN_BLEND_FACTOR_SOURCE_ALPHA UINT32_C(5)
#define GRANIT_BACKEND_PLUGIN_BLEND_FACTOR_ONE_MINUS_SOURCE_ALPHA UINT32_C(6)
#define GRANIT_BACKEND_PLUGIN_BLEND_FACTOR_DESTINATION_COLOR UINT32_C(7)
#define GRANIT_BACKEND_PLUGIN_BLEND_FACTOR_ONE_MINUS_DESTINATION_COLOR UINT32_C(8)
#define GRANIT_BACKEND_PLUGIN_BLEND_FACTOR_DESTINATION_ALPHA UINT32_C(9)
#define GRANIT_BACKEND_PLUGIN_BLEND_FACTOR_ONE_MINUS_DESTINATION_ALPHA UINT32_C(10)

typedef uint32_t granit_backend_plugin_blend_operation;
#define GRANIT_BACKEND_PLUGIN_BLEND_OPERATION_ADD UINT32_C(1)
#define GRANIT_BACKEND_PLUGIN_BLEND_OPERATION_SUBTRACT UINT32_C(2)
#define GRANIT_BACKEND_PLUGIN_BLEND_OPERATION_REVERSE_SUBTRACT UINT32_C(3)
#define GRANIT_BACKEND_PLUGIN_BLEND_OPERATION_MIN UINT32_C(4)
#define GRANIT_BACKEND_PLUGIN_BLEND_OPERATION_MAX UINT32_C(5)

#define GRANIT_BACKEND_PLUGIN_COLOR_WRITE_RED_BIT (UINT32_C(1) << 0)
#define GRANIT_BACKEND_PLUGIN_COLOR_WRITE_GREEN_BIT (UINT32_C(1) << 1)
#define GRANIT_BACKEND_PLUGIN_COLOR_WRITE_BLUE_BIT (UINT32_C(1) << 2)
#define GRANIT_BACKEND_PLUGIN_COLOR_WRITE_ALPHA_BIT (UINT32_C(1) << 3)
#define GRANIT_BACKEND_PLUGIN_COLOR_WRITE_ALL_BITS                                                 \
  (GRANIT_BACKEND_PLUGIN_COLOR_WRITE_RED_BIT | GRANIT_BACKEND_PLUGIN_COLOR_WRITE_GREEN_BIT |       \
   GRANIT_BACKEND_PLUGIN_COLOR_WRITE_BLUE_BIT | GRANIT_BACKEND_PLUGIN_COLOR_WRITE_ALPHA_BIT)

typedef struct granit_backend_plugin_render_pipeline_desc {
  uint32_t struct_size;
  uint32_t reserved;
  granit_backend_plugin_pipeline_layout layout;
  granit_backend_plugin_shader vertex_shader;
  granit_backend_plugin_shader fragment_shader;
  uint32_t color_format;
  uint32_t vertex_buffer_layout_count;
  const granit_backend_plugin_vertex_buffer_layout* vertex_buffer_layouts;
  granit_backend_plugin_texture_format depth_stencil_format;
  uint32_t depth_test_enabled;
  uint32_t depth_write_enabled;
  granit_backend_plugin_compare_operation depth_compare;
  int32_t depth_bias_constant;
  float depth_bias_slope_scale;
  float depth_bias_clamp;
  uint32_t blend_enabled;
  granit_backend_plugin_blend_factor source_color_factor;
  granit_backend_plugin_blend_factor destination_color_factor;
  granit_backend_plugin_blend_operation color_operation;
  granit_backend_plugin_blend_factor source_alpha_factor;
  granit_backend_plugin_blend_factor destination_alpha_factor;
  granit_backend_plugin_blend_operation alpha_operation;
  uint32_t color_write_mask;
} granit_backend_plugin_render_pipeline_desc;

/** Canvas selector 仅在调用期间有效；插件必须复制后续需要的内容。 */
typedef struct granit_backend_plugin_canvas_surface_desc {
  uint32_t struct_size;
  uint32_t reserved;
  const char* selector;
  uint32_t selector_length;
} granit_backend_plugin_canvas_surface_desc;

typedef struct granit_backend_plugin_win32_surface_desc {
  uint32_t struct_size;
  uint32_t reserved;
  void* instance;
  void* window;
} granit_backend_plugin_win32_surface_desc;

typedef struct granit_backend_plugin_xcb_surface_desc {
  uint32_t struct_size;
  uint32_t reserved;
  void* connection;
  uint32_t window;
  uint32_t reserved_2;
} granit_backend_plugin_xcb_surface_desc;

typedef struct granit_backend_plugin_wayland_surface_desc {
  uint32_t struct_size;
  uint32_t reserved;
  void* display;
  void* surface;
} granit_backend_plugin_wayland_surface_desc;

typedef uint32_t granit_backend_plugin_present_mode;
#define GRANIT_BACKEND_PLUGIN_PRESENT_MODE_FIFO UINT32_C(0)
#define GRANIT_BACKEND_PLUGIN_PRESENT_MODE_MAILBOX UINT32_C(1)
#define GRANIT_BACKEND_PLUGIN_PRESENT_MODE_IMMEDIATE UINT32_C(2)

#define GRANIT_BACKEND_PLUGIN_TEXTURE_FORMAT_RGBA8_UNORM UINT32_C(1)
#define GRANIT_BACKEND_PLUGIN_TEXTURE_FORMAT_BGRA8_UNORM UINT32_C(2)
#define GRANIT_BACKEND_PLUGIN_TEXTURE_FORMAT_R8_UNORM UINT32_C(3)
#define GRANIT_BACKEND_PLUGIN_TEXTURE_FORMAT_RG8_UNORM UINT32_C(4)
#define GRANIT_BACKEND_PLUGIN_TEXTURE_FORMAT_RGBA8_SRGB UINT32_C(5)
#define GRANIT_BACKEND_PLUGIN_TEXTURE_FORMAT_D32_FLOAT UINT32_C(6)
#define GRANIT_BACKEND_PLUGIN_TEXTURE_FORMAT_RGBA16_FLOAT UINT32_C(7)

typedef struct granit_backend_plugin_swapchain_desc {
  uint32_t struct_size;
  uint32_t width;
  uint32_t height;
  uint32_t minimum_image_count;
  granit_backend_plugin_present_mode present_mode;
} granit_backend_plugin_swapchain_desc;

/** Swapchain 实际采用的配置；不支持请求模式时 Provider 必须报告降级后的模式。 */
typedef struct granit_backend_plugin_swapchain_info {
  uint32_t struct_size;
  uint32_t width;
  uint32_t height;
  uint32_t image_count;
  granit_backend_plugin_present_mode present_mode;
  granit_backend_plugin_texture_format format;
} granit_backend_plugin_swapchain_info;

/** Acquire 返回的 Texture/View 由 Swapchain 借出，在帧结束或重建后失效。 */
typedef struct granit_backend_plugin_acquired_frame {
  uint32_t struct_size;
  uint32_t image_index;
  uint32_t needs_recreate;
  uint32_t reserved;
  granit_backend_plugin_texture texture;
  granit_backend_plugin_texture_view view;
} granit_backend_plugin_acquired_frame;

/** 插件实例创建后固定的后端无关能力快照。 */
typedef struct granit_backend_plugin_capabilities {
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
} granit_backend_plugin_capabilities;

typedef void* (*granit_backend_plugin_allocate_fn)(uint64_t size, uint64_t alignment,
                                                   void* user_data);
typedef void (*granit_backend_plugin_deallocate_fn)(void* memory, uint64_t size, uint64_t alignment,
                                                    void* user_data);

/**
 * Host 服务及其 user_data 在所有插件实例销毁前保持有效。
 *
 * 回调可能由插件工作线程并发调用，不得向 ABI 外抛出异常。allocate 和 deallocate 必须成对提供；
 * 插件只能用 deallocate 释放同一 Host allocate 返回的内存。
 */
typedef struct granit_backend_plugin_host_api {
  uint32_t struct_size;
  uint32_t reserved;
  granit_diagnostic_callback diagnostic_callback;
  void* diagnostic_user_data;
  granit_backend_plugin_allocate_fn allocate;
  granit_backend_plugin_deallocate_fn deallocate;
  void* allocator_user_data;
} granit_backend_plugin_host_api;

typedef granit_result (*granit_backend_plugin_create_fn)(
    const granit_backend_plugin_host_api* host, granit_backend_plugin_instance* out_instance);
/** instance 非零且只允许销毁一次；销毁返回前插件不得继续使用 Host 服务。 */
typedef void (*granit_backend_plugin_destroy_fn)(granit_backend_plugin_instance instance);
typedef granit_result (*granit_backend_plugin_get_capabilities_fn)(
    granit_backend_plugin_instance instance, granit_backend_plugin_capabilities* capabilities);
typedef granit_result (*granit_backend_plugin_create_buffer_fn)(
    granit_backend_plugin_instance instance, const granit_backend_plugin_buffer_desc* desc,
    granit_backend_plugin_buffer* buffer);
typedef granit_result (*granit_backend_plugin_destroy_buffer_fn)(
    granit_backend_plugin_instance instance, granit_backend_plugin_buffer buffer);
typedef granit_result (*granit_backend_plugin_write_buffer_fn)(
    granit_backend_plugin_instance instance, granit_backend_plugin_buffer buffer, uint64_t offset,
    const void* data, uint64_t size);
typedef granit_result (*granit_backend_plugin_read_buffer_fn)(
    granit_backend_plugin_instance instance, granit_backend_plugin_buffer buffer, uint64_t offset,
    void* data, uint64_t size);
typedef granit_result (*granit_backend_plugin_create_texture_fn)(
    granit_backend_plugin_instance instance, const granit_backend_plugin_texture_desc* desc,
    granit_backend_plugin_texture* texture);
typedef granit_result (*granit_backend_plugin_destroy_texture_fn)(
    granit_backend_plugin_instance instance, granit_backend_plugin_texture texture);
typedef granit_result (*granit_backend_plugin_write_texture_fn)(
    granit_backend_plugin_instance instance, granit_backend_plugin_texture texture,
    const granit_backend_plugin_texture_write_desc* desc, const void* data, uint64_t size);
typedef granit_result (*granit_backend_plugin_write_upload_batch_fn)(
    granit_backend_plugin_instance instance,
    const granit_backend_plugin_upload_operation* operations, uint32_t operation_count);
typedef granit_result (*granit_backend_plugin_create_texture_view_fn)(
    granit_backend_plugin_instance instance, granit_backend_plugin_texture texture,
    const granit_backend_plugin_texture_view_desc* desc, granit_backend_plugin_texture_view* view);
typedef granit_result (*granit_backend_plugin_destroy_texture_view_fn)(
    granit_backend_plugin_instance instance, granit_backend_plugin_texture_view view);
typedef granit_result (*granit_backend_plugin_create_sampler_fn)(
    granit_backend_plugin_instance instance, const granit_backend_plugin_sampler_desc* desc,
    granit_backend_plugin_sampler* sampler);
typedef granit_result (*granit_backend_plugin_destroy_sampler_fn)(
    granit_backend_plugin_instance instance, granit_backend_plugin_sampler sampler);
typedef granit_result (*granit_backend_plugin_create_bind_group_layout_fn)(
    granit_backend_plugin_instance instance,
    const granit_backend_plugin_bind_group_layout_desc* desc,
    granit_backend_plugin_bind_group_layout* layout);
typedef granit_result (*granit_backend_plugin_destroy_bind_group_layout_fn)(
    granit_backend_plugin_instance instance, granit_backend_plugin_bind_group_layout layout);
typedef granit_result (*granit_backend_plugin_create_bind_group_fn)(
    granit_backend_plugin_instance instance, const granit_backend_plugin_bind_group_desc* desc,
    granit_backend_plugin_bind_group* bind_group);
typedef granit_result (*granit_backend_plugin_destroy_bind_group_fn)(
    granit_backend_plugin_instance instance, granit_backend_plugin_bind_group bind_group);
typedef granit_result (*granit_backend_plugin_create_shader_fn)(
    granit_backend_plugin_instance instance, const granit_backend_plugin_shader_desc* desc,
    granit_backend_plugin_shader* shader);
typedef granit_result (*granit_backend_plugin_destroy_shader_fn)(
    granit_backend_plugin_instance instance, granit_backend_plugin_shader shader);
typedef struct granit_backend_plugin_pipeline_layout_desc {
  uint32_t struct_size;
  uint32_t bind_group_layout_count;
  const granit_backend_plugin_bind_group_layout* bind_group_layouts;
  uint64_t reserved;
} granit_backend_plugin_pipeline_layout_desc;
typedef granit_result (*granit_backend_plugin_create_pipeline_layout_fn)(
    granit_backend_plugin_instance instance, const granit_backend_plugin_pipeline_layout_desc* desc,
    granit_backend_plugin_pipeline_layout* pipeline_layout);
typedef granit_result (*granit_backend_plugin_destroy_pipeline_layout_fn)(
    granit_backend_plugin_instance instance, granit_backend_plugin_pipeline_layout pipeline_layout);
typedef granit_result (*granit_backend_plugin_create_render_pipeline_fn)(
    granit_backend_plugin_instance instance, const granit_backend_plugin_render_pipeline_desc* desc,
    granit_backend_plugin_render_pipeline* render_pipeline);
typedef granit_result (*granit_backend_plugin_destroy_render_pipeline_fn)(
    granit_backend_plugin_instance instance, granit_backend_plugin_render_pipeline render_pipeline);
typedef struct granit_backend_plugin_compute_pipeline_desc {
  uint32_t struct_size;
  uint32_t reserved;
  granit_backend_plugin_pipeline_layout layout;
  granit_backend_plugin_shader shader;
} granit_backend_plugin_compute_pipeline_desc;
typedef granit_result (*granit_backend_plugin_create_compute_pipeline_fn)(
    granit_backend_plugin_instance instance,
    const granit_backend_plugin_compute_pipeline_desc* desc,
    granit_backend_plugin_compute_pipeline* compute_pipeline);
typedef granit_result (*granit_backend_plugin_destroy_compute_pipeline_fn)(
    granit_backend_plugin_instance instance,
    granit_backend_plugin_compute_pipeline compute_pipeline);
typedef granit_result (*granit_backend_plugin_recorder_begin_compute_fn)(
    granit_backend_plugin_instance instance, granit_backend_plugin_command_recorder recorder);
typedef granit_result (*granit_backend_plugin_recorder_bind_compute_pipeline_fn)(
    granit_backend_plugin_instance instance, granit_backend_plugin_command_recorder recorder,
    granit_backend_plugin_compute_pipeline pipeline);
typedef granit_result (*granit_backend_plugin_recorder_bind_compute_groups_fn)(
    granit_backend_plugin_instance instance, granit_backend_plugin_command_recorder recorder,
    granit_backend_plugin_pipeline_layout layout, uint32_t first_group,
    const granit_backend_plugin_bind_group* groups, uint32_t group_count,
    const uint32_t* dynamic_offsets, uint32_t dynamic_offset_count);
typedef granit_result (*granit_backend_plugin_recorder_dispatch_fn)(
    granit_backend_plugin_instance instance, granit_backend_plugin_command_recorder recorder,
    uint32_t group_count_x, uint32_t group_count_y, uint32_t group_count_z);
typedef granit_result (*granit_backend_plugin_recorder_end_compute_fn)(
    granit_backend_plugin_instance instance, granit_backend_plugin_command_recorder recorder);
typedef granit_result (*granit_backend_plugin_recorder_set_viewports_fn)(
    granit_backend_plugin_instance instance, granit_backend_plugin_command_recorder recorder,
    uint32_t first, const granit_backend_plugin_viewport* viewports, uint32_t count);
typedef granit_result (*granit_backend_plugin_recorder_set_scissors_fn)(
    granit_backend_plugin_instance instance, granit_backend_plugin_command_recorder recorder,
    uint32_t first, const granit_backend_plugin_scissor* scissors, uint32_t count);
typedef granit_result (*granit_backend_plugin_create_command_recorder_fn)(
    granit_backend_plugin_instance instance, granit_backend_plugin_command_recorder* recorder);
typedef granit_result (*granit_backend_plugin_destroy_command_recorder_fn)(
    granit_backend_plugin_instance instance, granit_backend_plugin_command_recorder recorder);
typedef granit_result (*granit_backend_plugin_recorder_copy_buffer_to_texture_fn)(
    granit_backend_plugin_instance instance, granit_backend_plugin_command_recorder recorder,
    granit_backend_plugin_buffer buffer, granit_backend_plugin_texture texture, uint32_t width,
    uint32_t height, uint32_t bytes_per_row);
typedef granit_result (*granit_backend_plugin_recorder_begin_rendering_fn)(
    granit_backend_plugin_instance instance, granit_backend_plugin_command_recorder recorder,
    granit_backend_plugin_texture_view target, granit_backend_plugin_load_operation load_operation,
    granit_backend_plugin_store_operation store_operation, float clear_r, float clear_g,
    float clear_b, float clear_a, granit_backend_plugin_texture_view depth_target,
    granit_backend_plugin_load_operation depth_load_operation,
    granit_backend_plugin_store_operation depth_store_operation, float clear_depth);
typedef granit_result (*granit_backend_plugin_recorder_bind_pipeline_fn)(
    granit_backend_plugin_instance instance, granit_backend_plugin_command_recorder recorder,
    granit_backend_plugin_render_pipeline pipeline);
typedef granit_result (*granit_backend_plugin_recorder_bind_graphics_groups_fn)(
    granit_backend_plugin_instance instance, granit_backend_plugin_command_recorder recorder,
    granit_backend_plugin_pipeline_layout layout, uint32_t first_group,
    const granit_backend_plugin_bind_group* groups, uint32_t group_count,
    const uint32_t* dynamic_offsets, uint32_t dynamic_offset_count);
typedef granit_result (*granit_backend_plugin_recorder_bind_vertex_buffers_fn)(
    granit_backend_plugin_instance instance, granit_backend_plugin_command_recorder recorder,
    uint32_t first, const granit_backend_plugin_vertex_buffer_binding* bindings, uint32_t count);
typedef granit_result (*granit_backend_plugin_recorder_bind_index_buffer_fn)(
    granit_backend_plugin_instance instance, granit_backend_plugin_command_recorder recorder,
    granit_backend_plugin_buffer buffer, uint64_t offset,
    granit_backend_plugin_index_format format);
typedef granit_result (*granit_backend_plugin_recorder_draw_vertices_fn)(
    granit_backend_plugin_instance instance, granit_backend_plugin_command_recorder recorder,
    uint32_t vertex_count, uint32_t instance_count, uint32_t first_vertex, uint32_t first_instance);
typedef granit_result (*granit_backend_plugin_recorder_draw_indices_fn)(
    granit_backend_plugin_instance instance, granit_backend_plugin_command_recorder recorder,
    uint32_t index_count, uint32_t instance_count, uint32_t first_index, int32_t vertex_offset,
    uint32_t first_instance);
typedef granit_result (*granit_backend_plugin_recorder_end_rendering_fn)(
    granit_backend_plugin_instance instance, granit_backend_plugin_command_recorder recorder);
typedef granit_result (*granit_backend_plugin_finish_command_recorder_fn)(
    granit_backend_plugin_instance instance, granit_backend_plugin_command_recorder recorder,
    granit_backend_plugin_command_buffer* command_buffer);
typedef granit_result (*granit_backend_plugin_destroy_command_buffer_fn)(
    granit_backend_plugin_instance instance, granit_backend_plugin_command_buffer command_buffer);
typedef granit_result (*granit_backend_plugin_submit_command_buffer_fn)(
    granit_backend_plugin_instance instance, granit_backend_plugin_command_buffer command_buffer);
typedef granit_result (*granit_backend_plugin_recorder_copy_texture_to_buffer_fn)(
    granit_backend_plugin_instance instance, granit_backend_plugin_command_recorder recorder,
    granit_backend_plugin_texture texture, granit_backend_plugin_buffer buffer, uint32_t width,
    uint32_t height, uint32_t bytes_per_row);
typedef granit_result (*granit_backend_plugin_get_instance_status_fn)(
    granit_backend_plugin_instance instance, granit_backend_plugin_instance_status* status);
/** 非阻塞地推进已完成的异步回调；没有待处理事件也返回成功。 */
typedef granit_result (*granit_backend_plugin_process_events_fn)(
    granit_backend_plugin_instance instance);
typedef granit_result (*granit_backend_plugin_create_win32_surface_fn)(
    granit_backend_plugin_instance instance, const granit_backend_plugin_win32_surface_desc* desc,
    granit_backend_plugin_surface* surface);
typedef granit_result (*granit_backend_plugin_create_xcb_surface_fn)(
    granit_backend_plugin_instance instance, const granit_backend_plugin_xcb_surface_desc* desc,
    granit_backend_plugin_surface* surface);
typedef granit_result (*granit_backend_plugin_create_wayland_surface_fn)(
    granit_backend_plugin_instance instance, const granit_backend_plugin_wayland_surface_desc* desc,
    granit_backend_plugin_surface* surface);
typedef granit_result (*granit_backend_plugin_create_canvas_surface_fn)(
    granit_backend_plugin_instance instance, const granit_backend_plugin_canvas_surface_desc* desc,
    granit_backend_plugin_surface* surface);
typedef granit_result (*granit_backend_plugin_destroy_surface_fn)(
    granit_backend_plugin_instance instance, granit_backend_plugin_surface surface);
typedef granit_result (*granit_backend_plugin_create_swapchain_fn)(
    granit_backend_plugin_instance instance, granit_backend_plugin_surface surface,
    const granit_backend_plugin_swapchain_desc* desc, granit_backend_plugin_swapchain* swapchain);
typedef granit_result (*granit_backend_plugin_recreate_swapchain_fn)(
    granit_backend_plugin_instance instance, granit_backend_plugin_swapchain swapchain,
    const granit_backend_plugin_swapchain_desc* desc);
typedef granit_result (*granit_backend_plugin_get_swapchain_info_fn)(
    granit_backend_plugin_instance instance, granit_backend_plugin_swapchain swapchain,
    granit_backend_plugin_swapchain_info* info);
typedef granit_result (*granit_backend_plugin_acquire_swapchain_fn)(
    granit_backend_plugin_instance instance, granit_backend_plugin_swapchain swapchain,
    granit_backend_plugin_acquired_frame* frame);
typedef granit_result (*granit_backend_plugin_present_swapchain_fn)(
    granit_backend_plugin_instance instance, granit_backend_plugin_swapchain swapchain,
    uint32_t* needs_recreate);
typedef granit_result (*granit_backend_plugin_cancel_swapchain_fn)(
    granit_backend_plugin_instance instance, granit_backend_plugin_swapchain swapchain,
    uint32_t* needs_recreate);
typedef granit_result (*granit_backend_plugin_destroy_swapchain_fn)(
    granit_backend_plugin_instance instance, granit_backend_plugin_swapchain swapchain);

/**
 * 实例操作表由插件拥有，在插件卸载前保持有效。
 *
 * 除状态查询、事件推进和销毁操作外，依赖设备的调用在 initializing 状态返回 NOT_READY，在 failed
 * 或 device_lost 状态返回状态快照中的 failure_result。销毁操作必须始终允许清理已有资源。
 */
typedef struct granit_backend_plugin_instance_api {
  uint32_t struct_size;
  uint32_t reserved;
  granit_backend_plugin_get_capabilities_fn get_capabilities;
  granit_backend_plugin_create_buffer_fn create_buffer;
  granit_backend_plugin_destroy_buffer_fn destroy_buffer;
  granit_backend_plugin_write_buffer_fn write_buffer;
  granit_backend_plugin_read_buffer_fn read_buffer;
  granit_backend_plugin_create_texture_fn create_texture;
  granit_backend_plugin_destroy_texture_fn destroy_texture;
  granit_backend_plugin_write_texture_fn write_texture;
  granit_backend_plugin_create_texture_view_fn create_texture_view;
  granit_backend_plugin_destroy_texture_view_fn destroy_texture_view;
  granit_backend_plugin_create_sampler_fn create_sampler;
  granit_backend_plugin_destroy_sampler_fn destroy_sampler;
  granit_backend_plugin_create_bind_group_layout_fn create_bind_group_layout;
  granit_backend_plugin_destroy_bind_group_layout_fn destroy_bind_group_layout;
  granit_backend_plugin_create_bind_group_fn create_bind_group;
  granit_backend_plugin_destroy_bind_group_fn destroy_bind_group;
  granit_backend_plugin_create_shader_fn create_shader;
  granit_backend_plugin_destroy_shader_fn destroy_shader;
  granit_backend_plugin_create_pipeline_layout_fn create_pipeline_layout;
  granit_backend_plugin_destroy_pipeline_layout_fn destroy_pipeline_layout;
  granit_backend_plugin_create_render_pipeline_fn create_render_pipeline;
  granit_backend_plugin_destroy_render_pipeline_fn destroy_render_pipeline;
  granit_backend_plugin_create_command_recorder_fn create_command_recorder;
  granit_backend_plugin_destroy_command_recorder_fn destroy_command_recorder;
  granit_backend_plugin_recorder_copy_buffer_to_texture_fn recorder_copy_buffer_to_texture;
  granit_backend_plugin_finish_command_recorder_fn finish_command_recorder;
  granit_backend_plugin_destroy_command_buffer_fn destroy_command_buffer;
  granit_backend_plugin_submit_command_buffer_fn submit_command_buffer;
  granit_backend_plugin_recorder_copy_texture_to_buffer_fn recorder_copy_texture_to_buffer;
  granit_backend_plugin_get_instance_status_fn get_instance_status;
  granit_backend_plugin_process_events_fn process_events;
  granit_backend_plugin_create_win32_surface_fn create_win32_surface;
  granit_backend_plugin_create_xcb_surface_fn create_xcb_surface;
  granit_backend_plugin_create_wayland_surface_fn create_wayland_surface;
  granit_backend_plugin_create_canvas_surface_fn create_canvas_surface;
  granit_backend_plugin_destroy_surface_fn destroy_surface;
  granit_backend_plugin_create_swapchain_fn create_swapchain;
  granit_backend_plugin_recreate_swapchain_fn recreate_swapchain;
  granit_backend_plugin_get_swapchain_info_fn get_swapchain_info;
  granit_backend_plugin_acquire_swapchain_fn acquire_swapchain;
  granit_backend_plugin_present_swapchain_fn present_swapchain;
  granit_backend_plugin_cancel_swapchain_fn cancel_swapchain;
  granit_backend_plugin_destroy_swapchain_fn destroy_swapchain;
  granit_backend_plugin_recorder_begin_rendering_fn recorder_begin_rendering;
  granit_backend_plugin_recorder_bind_pipeline_fn recorder_bind_pipeline;
  granit_backend_plugin_recorder_bind_graphics_groups_fn recorder_bind_graphics_groups;
  granit_backend_plugin_recorder_bind_vertex_buffers_fn recorder_bind_vertex_buffers;
  granit_backend_plugin_recorder_bind_index_buffer_fn recorder_bind_index_buffer;
  granit_backend_plugin_recorder_draw_vertices_fn recorder_draw_vertices;
  granit_backend_plugin_recorder_draw_indices_fn recorder_draw_indices;
  granit_backend_plugin_recorder_end_rendering_fn recorder_end_rendering;
  granit_backend_plugin_write_upload_batch_fn write_upload_batch;
  granit_backend_plugin_create_compute_pipeline_fn create_compute_pipeline;
  granit_backend_plugin_destroy_compute_pipeline_fn destroy_compute_pipeline;
  granit_backend_plugin_recorder_begin_compute_fn recorder_begin_compute;
  granit_backend_plugin_recorder_bind_compute_pipeline_fn recorder_bind_compute_pipeline;
  granit_backend_plugin_recorder_bind_compute_groups_fn recorder_bind_compute_groups;
  granit_backend_plugin_recorder_dispatch_fn recorder_dispatch;
  granit_backend_plugin_recorder_end_compute_fn recorder_end_compute;
  granit_backend_plugin_recorder_set_viewports_fn recorder_set_viewports;
  granit_backend_plugin_recorder_set_scissors_fn recorder_set_scissors;
} granit_backend_plugin_instance_api;

/** 后端插件入口返回的只读描述；字符串在插件卸载前有效。 */
typedef struct granit_backend_plugin_api {
  uint32_t struct_size;
  uint32_t abi_version;
  granit_backend_plugin_kind kind;
  uint32_t reserved;
  const char* name;
  uint32_t name_length;
  granit_backend_plugin_create_fn create;
  granit_backend_plugin_destroy_fn destroy;
  const granit_backend_plugin_instance_api* instance_api;
} granit_backend_plugin_api;

typedef const granit_backend_plugin_api* (*granit_backend_plugin_query_fn)(uint32_t requested_abi);

#endif
