// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_BACKEND_PLUGIN_API_H_
#define GRANIT_BACKEND_PLUGIN_API_H_

#include <stdint.h>

#include <granit/core/diagnostic.h>
#include <granit/core/result.h>

#define GRANIT_BACKEND_PLUGIN_ABI_VERSION UINT32_C(2)
#define GRANIT_BACKEND_PLUGIN_KIND_WEBGPU UINT32_C(1)
#define GRANIT_BACKEND_PLUGIN_QUERY_SYMBOL "granit_backend_plugin_query"

typedef uint32_t granit_backend_plugin_kind;
typedef uint64_t granit_backend_plugin_instance;
typedef uint64_t granit_backend_plugin_buffer;
typedef uint64_t granit_backend_plugin_texture;
typedef uint64_t granit_backend_plugin_texture_view;
typedef uint64_t granit_backend_plugin_sampler;

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

/** 实例操作表由插件拥有，在插件卸载前保持有效。 */
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
