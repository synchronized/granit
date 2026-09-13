// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_BACKEND_WEBGPU_DEVICE_STATE_H_
#define GRANIT_BACKEND_WEBGPU_DEVICE_STATE_H_

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include <webgpu/webgpu.h>

#include "backend/contracts/callback_lifetime.h"
#include "backend/contracts/lifecycle.h"
#include "backend/webgpu/types.h"

namespace granit::detail {

/** 集中拥有 Dawn 设备对象、异步回调状态及各类原生资源句柄表。 */
struct webgpu_device_state {
  struct buffer_record {
    WGPUBuffer buffer;
    std::uint64_t size;
    webgpu_buffer_usage usage;
  };
  struct texture_record {
    WGPUTexture texture;
    std::uint32_t width;
    std::uint32_t height;
    webgpu_texture_format format;
    std::uint32_t mip_level_count;
    std::uint32_t array_layer_count;
    std::uint32_t sample_count;
    webgpu_texture_usage usage;
    bool borrowed;
  };
  struct texture_view_record {
    WGPUTextureView view;
    webgpu_texture texture;
    bool borrowed;
  };
  struct bind_group_record {
    WGPUBindGroup bind_group;
    webgpu_bind_group_layout layout;
    std::vector<webgpu_buffer> buffers;
    std::vector<webgpu_texture_view> texture_views;
    std::vector<webgpu_sampler> samplers;
    std::vector<webgpu_bind_group_entry> entries;
  };
  struct bind_group_layout_record {
    WGPUBindGroupLayout bind_group_layout;
    std::vector<webgpu_bind_group_layout_entry> entries;
  };
  struct pipeline_layout_record {
    WGPUPipelineLayout pipeline_layout;
    std::vector<webgpu_bind_group_layout> bind_group_layouts;
  };
  struct shader_record {
    WGPUShaderModule shader;
    webgpu_shader_stage stage;
    std::string entry_point;
  };
  struct render_pipeline_record {
    WGPURenderPipeline render_pipeline;
    webgpu_pipeline_layout pipeline_layout;
    webgpu_shader vertex_shader;
    webgpu_shader fragment_shader;
  };
  struct compute_pipeline_record {
    WGPUComputePipeline compute_pipeline;
    webgpu_pipeline_layout pipeline_layout;
    webgpu_shader shader;
  };
  struct command_recorder_record {
    WGPUCommandEncoder encoder;
    WGPURenderPassEncoder pass;
    WGPUComputePassEncoder compute_pass;
    bool finished;
    bool pipeline_bound;
    bool compute_pipeline_bound;
    std::uint64_t index_available;
    std::uint32_t index_element_size;
    std::vector<WGPUBuffer> temporary_buffers;
    std::vector<webgpu_timestamp_query_pool> timestamp_pools;
  };
  struct timestamp_query_record {
    WGPUQuerySet query_set{};
    WGPUBuffer resolve_buffer{};
    WGPUBuffer read_buffer{};
    std::uint32_t count{};
    std::atomic_uint32_t map_state{};
    std::vector<std::uint64_t> values;

    ~timestamp_query_record() {
      if (read_buffer != nullptr)
        wgpuBufferRelease(read_buffer);
      if (resolve_buffer != nullptr)
        wgpuBufferRelease(resolve_buffer);
      if (query_set != nullptr)
        wgpuQuerySetRelease(query_set);
    }
  };
  struct readback_record {
    WGPUBuffer buffer{};
    std::uint64_t offset{};
    std::uint64_t size{};
    std::atomic_uint32_t state{};
    std::vector<std::byte> bytes;

    ~readback_record() {
      if (buffer != nullptr)
        wgpuBufferRelease(buffer);
    }
  };
  struct pipeline_warmup_record {
    std::atomic<granit_result> result{GRANIT_ERROR_NOT_READY};
    WGPUFuture future{};
  };
  struct surface_record {
    void* surface;
    std::string selector;
  };
  struct swapchain_record {
    webgpu_surface surface;
    void* native_surface;
    webgpu_swapchain_info info;
    webgpu_texture acquired_texture;
    webgpu_texture_view acquired_view;
  };

  webgpu_host_api host;
  WGPUInstance instance;
  WGPUAdapter adapter;
  WGPUDevice device;
  WGPUQueue queue;
  webgpu_capabilities capabilities;
  backend_lifecycle lifecycle;
  backend_callback_lifetime callback_lifetime;
  backend_callback_ticket adapter_ticket;
  backend_callback_ticket device_ticket;
  backend_callback_ticket device_lost_ticket;
  bool deferred_initialization_for_test;
  bool fail_initialization_for_test;
  bool force_device_loss_for_test;
  std::unordered_map<webgpu_buffer, buffer_record> buffers;
  std::unordered_map<webgpu_texture, texture_record> textures;
  std::unordered_map<webgpu_texture_view, texture_view_record> texture_views;
  std::unordered_map<webgpu_sampler, WGPUSampler> samplers;
  std::unordered_map<webgpu_bind_group_layout, bind_group_layout_record> bind_group_layouts;
  std::unordered_map<webgpu_bind_group, bind_group_record> bind_groups;
  std::unordered_map<webgpu_shader, shader_record> shaders;
  std::unordered_map<webgpu_pipeline_layout, pipeline_layout_record> pipeline_layouts;
  std::unordered_map<webgpu_render_pipeline, render_pipeline_record> render_pipelines;
  std::unordered_map<webgpu_compute_pipeline, compute_pipeline_record> compute_pipelines;
  std::unordered_map<webgpu_command_recorder, command_recorder_record> command_recorders;
  std::unordered_map<webgpu_command_buffer, WGPUCommandBuffer> command_buffers;
  std::unordered_map<webgpu_timestamp_query_pool, std::shared_ptr<timestamp_query_record>>
      timestamp_queries;
  std::unordered_map<webgpu_readback, std::shared_ptr<readback_record>> readbacks;
  std::unordered_map<webgpu_pipeline_warmup, std::shared_ptr<pipeline_warmup_record>>
      pipeline_warmups;
  std::unordered_map<webgpu_surface, surface_record> surfaces;
  std::unordered_map<webgpu_swapchain, swapchain_record> swapchains;

  webgpu_device_state(const webgpu_host_api& host_api, WGPUInstance native_instance) noexcept
      : host(host_api), instance(native_instance), adapter(nullptr), device(nullptr),
        queue(nullptr), capabilities{}, adapter_ticket(callback_lifetime.ticket()),
        device_ticket(callback_lifetime.ticket()), device_lost_ticket(callback_lifetime.ticket()),
        deferred_initialization_for_test(false), fail_initialization_for_test(false),
        force_device_loss_for_test(false) {}
};

namespace webgpu_native {

inline std::mutex instances_mutex;
inline std::unordered_map<webgpu_instance_handle, webgpu_device_state*> instances;
inline std::atomic_uint64_t next_instance{1};
inline std::atomic_uint64_t next_buffer{1};
inline std::atomic_uint64_t next_texture{1};
inline std::atomic_uint64_t next_texture_view{1};
inline std::atomic_uint64_t next_sampler{1};
inline std::atomic_uint64_t next_bind_group_layout{1};
inline std::atomic_uint64_t next_bind_group{1};
inline std::atomic_uint64_t next_shader{1};
inline std::atomic_uint64_t next_pipeline_layout{1};
inline std::atomic_uint64_t next_render_pipeline{1};
inline std::atomic_uint64_t next_compute_pipeline{1};
inline std::atomic_uint64_t next_command_recorder{1};
inline std::atomic_uint64_t next_command_buffer{1};
inline std::atomic_uint64_t next_timestamp_query_pool{1};
inline std::atomic_uint64_t next_readback{1};
inline std::atomic_uint64_t next_pipeline_warmup{1};
inline std::atomic_uint64_t next_swapchain{1};
inline std::atomic_uint64_t next_surface{1};

[[nodiscard]] inline granit_result require_ready(const webgpu_device_state& state) noexcept {
  return state.lifecycle.gate();
}

template <typename Handle>
[[nodiscard]] Handle next_handle(std::atomic_uint64_t& counter) noexcept {
  auto handle = counter.fetch_add(1, std::memory_order_relaxed);
  while (handle == 0)
    handle = counter.fetch_add(1, std::memory_order_relaxed);
  return static_cast<Handle>(handle);
}

} // namespace webgpu_native

} // namespace granit::detail

#endif
