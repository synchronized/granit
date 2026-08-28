// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_BACKEND_PLUGIN_API_H_
#define GRANIT_BACKEND_PLUGIN_API_H_

#include <stdint.h>

#include <granit/core/diagnostic.h>
#include <granit/core/result.h>

#define GRANIT_BACKEND_PLUGIN_ABI_VERSION UINT32_C(6)
#define GRANIT_BACKEND_PLUGIN_KIND_WEBGPU UINT32_C(1)
#define GRANIT_BACKEND_PLUGIN_QUERY_SYMBOL "granit_backend_plugin_query"

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
typedef uint64_t granit_backend_plugin_command_recorder;
typedef uint64_t granit_backend_plugin_command_buffer;
typedef uint64_t granit_backend_plugin_surface;
typedef uint64_t granit_backend_plugin_swapchain;

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

/** S-10B 首轮只支持二维 RGBA8 UNORM、单层、单 mip、单采样纹理。 */
typedef struct granit_backend_plugin_texture_desc {
  uint32_t struct_size;
  uint32_t reserved;
  uint32_t width;
  uint32_t height;
  granit_backend_plugin_texture_usage usage;
  uint32_t reserved_flags;
} granit_backend_plugin_texture_desc;

typedef uint32_t granit_backend_plugin_filter;
#define GRANIT_BACKEND_PLUGIN_FILTER_NEAREST UINT32_C(1)
#define GRANIT_BACKEND_PLUGIN_FILTER_LINEAR UINT32_C(2)

typedef struct granit_backend_plugin_sampler_desc {
  uint32_t struct_size;
  uint32_t reserved;
  granit_backend_plugin_filter min_filter;
  granit_backend_plugin_filter mag_filter;
} granit_backend_plugin_sampler_desc;

/** 固定绑定 0 为二维浮点 Texture View，绑定 1 为过滤 Sampler。 */
typedef struct granit_backend_plugin_bind_group_desc {
  uint32_t struct_size;
  uint32_t reserved;
  granit_backend_plugin_bind_group_layout layout;
  granit_backend_plugin_texture_view texture_view;
  granit_backend_plugin_sampler sampler;
} granit_backend_plugin_bind_group_desc;

typedef uint32_t granit_backend_plugin_shader_stage;
#define GRANIT_BACKEND_PLUGIN_SHADER_STAGE_VERTEX UINT32_C(1)
#define GRANIT_BACKEND_PLUGIN_SHADER_STAGE_FRAGMENT UINT32_C(2)

/** WGSL 字节和入口点仅在调用期间有效，插件必须复制所需内容。 */
typedef struct granit_backend_plugin_shader_desc {
  uint32_t struct_size;
  granit_backend_plugin_shader_stage stage;
  const char* wgsl;
  uint64_t wgsl_length;
  const char* entry_point;
  uint64_t entry_point_length;
} granit_backend_plugin_shader_desc;

typedef struct granit_backend_plugin_render_pipeline_desc {
  uint32_t struct_size;
  uint32_t reserved;
  granit_backend_plugin_pipeline_layout layout;
  granit_backend_plugin_shader vertex_shader;
  granit_backend_plugin_shader fragment_shader;
} granit_backend_plugin_render_pipeline_desc;

/** Canvas selector 仅在调用期间有效；插件必须复制后续需要的内容。 */
typedef struct granit_backend_plugin_canvas_surface_desc {
  uint32_t struct_size;
  uint32_t reserved;
  const char* selector;
  uint32_t selector_length;
} granit_backend_plugin_canvas_surface_desc;

typedef uint32_t granit_backend_plugin_present_mode;
#define GRANIT_BACKEND_PLUGIN_PRESENT_MODE_FIFO UINT32_C(0)
#define GRANIT_BACKEND_PLUGIN_PRESENT_MODE_MAILBOX UINT32_C(1)
#define GRANIT_BACKEND_PLUGIN_PRESENT_MODE_IMMEDIATE UINT32_C(2)

typedef uint32_t granit_backend_plugin_texture_format;
#define GRANIT_BACKEND_PLUGIN_TEXTURE_FORMAT_RGBA8_UNORM UINT32_C(1)
#define GRANIT_BACKEND_PLUGIN_TEXTURE_FORMAT_BGRA8_UNORM UINT32_C(2)

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
typedef granit_result (*granit_backend_plugin_create_texture_view_fn)(
    granit_backend_plugin_instance instance, granit_backend_plugin_texture texture,
    granit_backend_plugin_texture_view* view);
typedef granit_result (*granit_backend_plugin_destroy_texture_view_fn)(
    granit_backend_plugin_instance instance, granit_backend_plugin_texture_view view);
typedef granit_result (*granit_backend_plugin_create_sampler_fn)(
    granit_backend_plugin_instance instance, const granit_backend_plugin_sampler_desc* desc,
    granit_backend_plugin_sampler* sampler);
typedef granit_result (*granit_backend_plugin_destroy_sampler_fn)(
    granit_backend_plugin_instance instance, granit_backend_plugin_sampler sampler);
typedef granit_result (*granit_backend_plugin_create_bind_group_layout_fn)(
    granit_backend_plugin_instance instance, granit_backend_plugin_bind_group_layout* layout);
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
typedef granit_result (*granit_backend_plugin_create_pipeline_layout_fn)(
    granit_backend_plugin_instance instance,
    granit_backend_plugin_bind_group_layout bind_group_layout,
    granit_backend_plugin_pipeline_layout* pipeline_layout);
typedef granit_result (*granit_backend_plugin_destroy_pipeline_layout_fn)(
    granit_backend_plugin_instance instance, granit_backend_plugin_pipeline_layout pipeline_layout);
typedef granit_result (*granit_backend_plugin_create_render_pipeline_fn)(
    granit_backend_plugin_instance instance, const granit_backend_plugin_render_pipeline_desc* desc,
    granit_backend_plugin_render_pipeline* render_pipeline);
typedef granit_result (*granit_backend_plugin_destroy_render_pipeline_fn)(
    granit_backend_plugin_instance instance, granit_backend_plugin_render_pipeline render_pipeline);
typedef granit_result (*granit_backend_plugin_create_command_recorder_fn)(
    granit_backend_plugin_instance instance, granit_backend_plugin_command_recorder* recorder);
typedef granit_result (*granit_backend_plugin_destroy_command_recorder_fn)(
    granit_backend_plugin_instance instance, granit_backend_plugin_command_recorder recorder);
typedef granit_result (*granit_backend_plugin_recorder_copy_buffer_to_texture_fn)(
    granit_backend_plugin_instance instance, granit_backend_plugin_command_recorder recorder,
    granit_backend_plugin_buffer buffer, granit_backend_plugin_texture texture, uint32_t width,
    uint32_t height, uint32_t bytes_per_row);
typedef granit_result (*granit_backend_plugin_recorder_draw_fn)(
    granit_backend_plugin_instance instance, granit_backend_plugin_command_recorder recorder,
    granit_backend_plugin_texture_view target, granit_backend_plugin_render_pipeline pipeline,
    granit_backend_plugin_bind_group bind_group);
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
  granit_backend_plugin_recorder_draw_fn recorder_draw;
  granit_backend_plugin_finish_command_recorder_fn finish_command_recorder;
  granit_backend_plugin_destroy_command_buffer_fn destroy_command_buffer;
  granit_backend_plugin_submit_command_buffer_fn submit_command_buffer;
  granit_backend_plugin_recorder_copy_texture_to_buffer_fn recorder_copy_texture_to_buffer;
  granit_backend_plugin_get_instance_status_fn get_instance_status;
  granit_backend_plugin_process_events_fn process_events;
  granit_backend_plugin_create_canvas_surface_fn create_canvas_surface;
  granit_backend_plugin_destroy_surface_fn destroy_surface;
  granit_backend_plugin_create_swapchain_fn create_swapchain;
  granit_backend_plugin_recreate_swapchain_fn recreate_swapchain;
  granit_backend_plugin_get_swapchain_info_fn get_swapchain_info;
  granit_backend_plugin_acquire_swapchain_fn acquire_swapchain;
  granit_backend_plugin_present_swapchain_fn present_swapchain;
  granit_backend_plugin_cancel_swapchain_fn cancel_swapchain;
  granit_backend_plugin_destroy_swapchain_fn destroy_swapchain;
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
