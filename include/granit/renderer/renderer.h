// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_RENDERER_H_
#define GRANIT_RENDERER_H_

#include <stddef.h>
#include <stdint.h>

#include <granit/core/diagnostic.h>
#include <granit/core/export.h>
#include <granit/core/result.h>
#include <granit/core/shader_features.h>
#include <granit/core/types.h>

/** Renderer 对象句柄。零值无效。 */
typedef granit_handle granit_renderer;

typedef uint32_t granit_renderer_backend;
#define GRANIT_RENDERER_BACKEND_AUTO UINT32_C(0)
#define GRANIT_RENDERER_BACKEND_VULKAN UINT32_C(1)
#define GRANIT_RENDERER_BACKEND_WEBGPU UINT32_C(2)

typedef uint32_t granit_renderer_state;
#define GRANIT_RENDERER_STATE_INITIALIZING UINT32_C(1)
#define GRANIT_RENDERER_STATE_READY UINT32_C(2)
#define GRANIT_RENDERER_STATE_FAILED UINT32_C(3)
#define GRANIT_RENDERER_STATE_DEVICE_LOST UINT32_C(4)

/** Renderer 当前生命周期快照；failure_result 仅在失败或设备丢失状态下非成功。 */
typedef struct granit_renderer_status {
  uint32_t struct_size;
  granit_renderer_state state;
  granit_result failure_result;
  uint32_t reserved;
} granit_renderer_status;

#define GRANIT_RENDERER_STATUS_VERSION_1_SIZE                                                      \
  ((uint32_t)(offsetof(granit_renderer_status, reserved) + sizeof(uint32_t)))
#define GRANIT_RENDERER_STATUS_INIT                                                                \
  {(uint32_t)sizeof(granit_renderer_status), GRANIT_RENDERER_STATE_INITIALIZING, GRANIT_SUCCESS,   \
   UINT32_C(0)}

#define GRANIT_RENDERER_API_VERSION_1 UINT32_C(1)
#define GRANIT_RENDERER_API_VERSION_CURRENT GRANIT_RENDERER_API_VERSION_1

#define GRANIT_RENDERER_ENABLE_VALIDATION_BIT (UINT32_C(1) << 0)

#define GRANIT_SURFACE_TYPE_WIN32_BIT (UINT32_C(1) << 0)
#define GRANIT_SURFACE_TYPE_XCB_BIT (UINT32_C(1) << 1)
#define GRANIT_SURFACE_TYPE_WAYLAND_BIT (UINT32_C(1) << 2)
#define GRANIT_SURFACE_TYPE_CANVAS_BIT (UINT32_C(1) << 3)

#define GRANIT_DEFAULT_FRAMES_IN_FLIGHT UINT32_C(2)
#define GRANIT_MAX_FRAMES_IN_FLIGHT UINT32_C(4)

/** Renderer 对应设备的公开限制快照。 */
typedef struct granit_renderer_limits {
  uint32_t struct_size;
  uint32_t reserved;
  uint64_t uniform_buffer_offset_alignment;
  uint64_t max_uniform_buffer_binding_size;
  /** 通用颜色与深度附件共同支持的样本数位集合。 */
  uint32_t framebuffer_sample_counts;
  /** Sampler 支持的最大各向异性；1 表示不支持各向异性。 */
  float max_sampler_anisotropy;
} granit_renderer_limits;

#define GRANIT_RENDERER_LIMITS_VERSION_1_SIZE                                                      \
  ((uint32_t)(offsetof(granit_renderer_limits, max_sampler_anisotropy) + sizeof(float)))

#define GRANIT_RENDERER_LIMITS_INIT                                                                \
  {(uint32_t)sizeof(granit_renderer_limits),                                                       \
   UINT32_C(0),                                                                                    \
   UINT64_C(0),                                                                                    \
   UINT64_C(0),                                                                                    \
   UINT32_C(0),                                                                                    \
   1.0F}

/** Renderer 对应设备可用于 Shader 变体选择的后端无关能力快照。 */
typedef struct granit_renderer_shader_capabilities {
  uint32_t struct_size;
  granit_renderer_backend backend;
  uint32_t profile;
  uint32_t reserved;
  granit_shader_feature_flags supported_features;
} granit_renderer_shader_capabilities;

#define GRANIT_RENDERER_SHADER_CAPABILITIES_SIZE                                                   \
  ((uint32_t)sizeof(granit_renderer_shader_capabilities))
#define GRANIT_RENDERER_SHADER_CAPABILITIES_INIT                                                   \
  {(uint32_t)sizeof(granit_renderer_shader_capabilities), GRANIT_RENDERER_BACKEND_AUTO,            \
   GRANIT_SHADER_PROFILE_PORTABLE, UINT32_C(0), UINT64_C(0)}

/** Renderer 当前存活的公开子资源及后端待回收资源快照。 */
typedef struct granit_renderer_resource_stats {
  uint32_t struct_size;
  uint32_t reserved;
  uint64_t total_live_count;
  uint64_t buffer_count;
  uint64_t texture_count;
  uint64_t texture_view_count;
  uint64_t sampler_count;
  uint64_t shader_count;
  uint64_t bind_group_layout_count;
  uint64_t bind_group_count;
  uint64_t pipeline_layout_count;
  uint64_t graphics_pipeline_count;
  uint64_t compute_pipeline_count;
  uint64_t surface_count;
  uint64_t swapchain_count;
  uint64_t command_recorder_count;
  uint64_t frame_context_count;
  uint64_t frame_count;
  uint64_t timestamp_query_pool_count;
  uint64_t upload_batch_count;
  uint64_t pending_retirement_count;
} granit_renderer_resource_stats;

#define GRANIT_RENDERER_RESOURCE_STATS_VERSION_1_SIZE                                              \
  ((uint32_t)(offsetof(granit_renderer_resource_stats, pending_retirement_count) +                 \
              sizeof(uint64_t)))
#define GRANIT_RENDERER_RESOURCE_STATS_INIT                                                        \
  {(uint32_t)sizeof(granit_renderer_resource_stats),                                               \
   UINT32_C(0),                                                                                    \
   UINT64_C(0),                                                                                    \
   UINT64_C(0),                                                                                    \
   UINT64_C(0),                                                                                    \
   UINT64_C(0),                                                                                    \
   UINT64_C(0),                                                                                    \
   UINT64_C(0),                                                                                    \
   UINT64_C(0),                                                                                    \
   UINT64_C(0),                                                                                    \
   UINT64_C(0),                                                                                    \
   UINT64_C(0),                                                                                    \
   UINT64_C(0),                                                                                    \
   UINT64_C(0),                                                                                    \
   UINT64_C(0),                                                                                    \
   UINT64_C(0),                                                                                    \
   UINT64_C(0),                                                                                    \
   UINT64_C(0),                                                                                    \
   UINT64_C(0),                                                                                    \
   UINT64_C(0),                                                                                    \
   UINT64_C(0)}

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
  granit_renderer_backend backend;
} granit_renderer_desc;

#define GRANIT_RENDERER_DESC_SIZE                                                                  \
  ((uint32_t)(offsetof(granit_renderer_desc, backend) + sizeof(granit_renderer_backend)))

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
   0,                                                                                              \
   GRANIT_RENDERER_BACKEND_AUTO}

/** Renderer 实际后端与只用于诊断的 Adapter 元数据。 */
typedef struct granit_renderer_info {
  uint32_t struct_size;
  granit_renderer_backend backend;
  char* adapter_name;
  uint32_t adapter_name_capacity;
  uint32_t adapter_name_length;
  uint32_t vendor_id;
  uint32_t device_id;
  uint32_t reserved[2];
} granit_renderer_info;

#define GRANIT_RENDERER_INFO_VERSION_1_SIZE                                                        \
  ((uint32_t)(offsetof(granit_renderer_info, reserved) + sizeof(uint32_t) * 2))
#define GRANIT_RENDERER_INFO_INIT                                                                  \
  {                                                                                                \
    (uint32_t)sizeof(granit_renderer_info), GRANIT_RENDERER_BACKEND_AUTO, 0, UINT32_C(0),          \
        UINT32_C(0), UINT32_C(0), UINT32_C(0), {                                                   \
      UINT32_C(0), UINT32_C(0)                                                                     \
    }                                                                                              \
  }

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

/** 查询用于 Shader 变体选择的后端、能力档位和可选特性位。 */
GRANIT_API granit_result granit_renderer_get_shader_capabilities(
    granit_renderer renderer, granit_renderer_shader_capabilities* capabilities);

/** 查询实际后端和 Adapter 元数据；名称容量包含结尾零字符。 */
GRANIT_API granit_result granit_renderer_get_info(granit_renderer renderer,
                                                  granit_renderer_info* info);

/** 查询公开子资源与延迟回收队列；调用者须先设置 stats->struct_size。 */
GRANIT_API granit_result granit_renderer_get_resource_stats(granit_renderer renderer,
                                                            granit_renderer_resource_stats* stats);

/** 查询 Renderer 生命周期；该调用不等待，也不执行用户回调。 */
GRANIT_API granit_result granit_renderer_get_status(granit_renderer renderer,
                                                    granit_renderer_status* status);

/** 非阻塞地推进 Renderer 后端已完成的异步事件。 */
GRANIT_API granit_result granit_renderer_process_events(granit_renderer renderer);

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
