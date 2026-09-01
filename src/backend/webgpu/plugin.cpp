// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "backend/callback_lifetime.h"
#include "backend/lifecycle.h"
#include "backend/plugin_api.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <mutex>
#include <new>
#include <string>
#include <unordered_map>
#include <vector>

#include <webgpu/webgpu.h>

#if defined(_WIN32)
#define GRANIT_BACKEND_PLUGIN_EXPORT __declspec(dllexport)
#else
#define GRANIT_BACKEND_PLUGIN_EXPORT __attribute__((visibility("default")))
#endif

namespace {

constexpr std::uint32_t provider_surface_types =
#if defined(GRANIT_WEBGPU_NATIVE_SURFACE_TEST)
    GRANIT_BACKEND_PLUGIN_SURFACE_TYPE_WIN32_BIT | GRANIT_BACKEND_PLUGIN_SURFACE_TYPE_XCB_BIT |
    GRANIT_BACKEND_PLUGIN_SURFACE_TYPE_WAYLAND_BIT |
#elif defined(_WIN32) && !defined(__EMSCRIPTEN__)
    GRANIT_BACKEND_PLUGIN_SURFACE_TYPE_WIN32_BIT |
#elif defined(__linux__) && !defined(__EMSCRIPTEN__)
    GRANIT_BACKEND_PLUGIN_SURFACE_TYPE_XCB_BIT | GRANIT_BACKEND_PLUGIN_SURFACE_TYPE_WAYLAND_BIT |
#endif
#if defined(__EMSCRIPTEN__) || defined(GRANIT_WEBGPU_CANVAS_SURFACE_TEST)
    GRANIT_BACKEND_PLUGIN_SURFACE_TYPE_CANVAS_BIT;
#else
    UINT32_C(0);
#endif

struct webgpu_instance {
  struct buffer_record {
    WGPUBuffer buffer;
    std::uint64_t size;
    granit_backend_plugin_buffer_usage usage;
  };
  struct texture_record {
    WGPUTexture texture;
    std::uint32_t width;
    std::uint32_t height;
    granit_backend_plugin_texture_format format;
    std::uint32_t mip_level_count;
    std::uint32_t array_layer_count;
    granit_backend_plugin_texture_usage usage;
    bool borrowed;
  };
  struct texture_view_record {
    WGPUTextureView view;
    granit_backend_plugin_texture texture;
    bool borrowed;
  };
  struct bind_group_record {
    WGPUBindGroup bind_group;
    granit_backend_plugin_bind_group_layout layout;
    std::vector<granit_backend_plugin_buffer> buffers;
    std::vector<granit_backend_plugin_texture_view> texture_views;
    std::vector<granit_backend_plugin_sampler> samplers;
    std::vector<granit_backend_plugin_bind_group_entry> entries;
  };
  struct bind_group_layout_record {
    WGPUBindGroupLayout bind_group_layout;
    std::vector<granit_backend_plugin_bind_group_layout_entry> entries;
  };
  struct pipeline_layout_record {
    WGPUPipelineLayout pipeline_layout;
    std::vector<granit_backend_plugin_bind_group_layout> bind_group_layouts;
  };
  struct shader_record {
    WGPUShaderModule shader;
    granit_backend_plugin_shader_stage stage;
    std::string entry_point;
  };
  struct render_pipeline_record {
    WGPURenderPipeline render_pipeline;
    granit_backend_plugin_pipeline_layout pipeline_layout;
    granit_backend_plugin_shader vertex_shader;
    granit_backend_plugin_shader fragment_shader;
  };
  struct compute_pipeline_record {
    WGPUComputePipeline compute_pipeline;
    granit_backend_plugin_pipeline_layout pipeline_layout;
    granit_backend_plugin_shader shader;
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
  };
  struct surface_record {
    void* surface;
    std::string selector;
  };
  struct swapchain_record {
    granit_backend_plugin_surface surface;
    void* native_surface;
    granit_backend_plugin_swapchain_info info;
    granit_backend_plugin_texture acquired_texture;
    granit_backend_plugin_texture_view acquired_view;
  };

  granit_backend_plugin_host_api host;
  WGPUInstance instance;
  WGPUAdapter adapter;
  WGPUDevice device;
  WGPUQueue queue;
  granit_backend_plugin_capabilities capabilities;
  granit::detail::backend_lifecycle lifecycle;
  granit::detail::backend_callback_lifetime callback_lifetime;
  granit::detail::backend_callback_ticket adapter_ticket;
  granit::detail::backend_callback_ticket device_ticket;
  granit::detail::backend_callback_ticket device_lost_ticket;
  bool deferred_initialization_for_test;
  bool fail_initialization_for_test;
  bool force_device_loss_for_test;
  std::unordered_map<granit_backend_plugin_buffer, buffer_record> buffers;
  std::unordered_map<granit_backend_plugin_texture, texture_record> textures;
  std::unordered_map<granit_backend_plugin_texture_view, texture_view_record> texture_views;
  std::unordered_map<granit_backend_plugin_sampler, WGPUSampler> samplers;
  std::unordered_map<granit_backend_plugin_bind_group_layout, bind_group_layout_record>
      bind_group_layouts;
  std::unordered_map<granit_backend_plugin_bind_group, bind_group_record> bind_groups;
  std::unordered_map<granit_backend_plugin_shader, shader_record> shaders;
  std::unordered_map<granit_backend_plugin_pipeline_layout, pipeline_layout_record>
      pipeline_layouts;
  std::unordered_map<granit_backend_plugin_render_pipeline, render_pipeline_record>
      render_pipelines;
  std::unordered_map<granit_backend_plugin_compute_pipeline, compute_pipeline_record>
      compute_pipelines;
  std::unordered_map<granit_backend_plugin_command_recorder, command_recorder_record>
      command_recorders;
  std::unordered_map<granit_backend_plugin_command_buffer, WGPUCommandBuffer> command_buffers;
  std::unordered_map<granit_backend_plugin_surface, surface_record> surfaces;
  std::unordered_map<granit_backend_plugin_swapchain, swapchain_record> swapchains;

  webgpu_instance(const granit_backend_plugin_host_api& host_api,
                  WGPUInstance native_instance) noexcept
      : host(host_api), instance(native_instance), adapter(nullptr), device(nullptr),
        queue(nullptr), capabilities{}, adapter_ticket(callback_lifetime.ticket()),
        device_ticket(callback_lifetime.ticket()), device_lost_ticket(callback_lifetime.ticket()),
        deferred_initialization_for_test(false), fail_initialization_for_test(false),
        force_device_loss_for_test(false) {}
};

constexpr std::uint64_t request_timeout_ns = UINT64_C(10000000000);

struct adapter_request {
  WGPURequestAdapterStatus status{};
  WGPUAdapter adapter{};
  char message[256]{};
  std::uint32_t message_length{};
};

struct device_request {
  const granit_backend_plugin_host_api* host{};
  WGPURequestDeviceStatus status{};
  WGPUDevice device{};
};

struct map_request {
  const granit_backend_plugin_host_api* host{};
  WGPUMapAsyncStatus status{};
};

std::mutex instances_mutex;
std::unordered_map<granit_backend_plugin_instance, webgpu_instance*> instances;
std::atomic_uint64_t next_instance{1};
std::atomic_uint64_t next_buffer{1};
std::atomic_uint64_t next_texture{1};
std::atomic_uint64_t next_texture_view{1};
std::atomic_uint64_t next_sampler{1};
std::atomic_uint64_t next_bind_group_layout{1};
std::atomic_uint64_t next_bind_group{1};
std::atomic_uint64_t next_shader{1};
std::atomic_uint64_t next_pipeline_layout{1};
std::atomic_uint64_t next_render_pipeline{1};
std::atomic_uint64_t next_compute_pipeline{1};
std::atomic_uint64_t next_command_recorder{1};
std::atomic_uint64_t next_command_buffer{1};
std::atomic_uint64_t next_swapchain{1};
std::atomic_uint64_t next_surface{1};
#if defined(GRANIT_WEBGPU_DEFER_INITIALIZATION_TEST)
#endif

granit_result require_ready(const webgpu_instance& state) noexcept {
  return state.lifecycle.gate();
}

void deallocate(const granit_backend_plugin_host_api& host, void* memory) noexcept {
  try {
    host.deallocate(memory, sizeof(webgpu_instance), alignof(webgpu_instance),
                    host.allocator_user_data);
  } catch (...) {
  }
}

WGPUStatus present_surface(WGPUSurface surface) noexcept {
#if defined(__EMSCRIPTEN__)
  // 浏览器在 requestAnimationFrame 边界隐式呈现，Emscripten 禁止显式调用 Present。
  static_cast<void>(surface);
  return WGPUStatus_Success;
#else
  return wgpuSurfacePresent(surface);
#endif
}

void release_resources(webgpu_instance& state) noexcept {
  state.callback_lifetime.invalidate();
  for (const auto& [handle, swapchain] : state.swapchains) {
    static_cast<void>(handle);
    const auto native_surface = static_cast<WGPUSurface>(swapchain.native_surface);
    if (swapchain.acquired_view != 0) {
      const auto view = state.texture_views.find(swapchain.acquired_view);
      if (view != state.texture_views.end()) {
        wgpuTextureViewRelease(view->second.view);
        state.texture_views.erase(view);
      }
    }
    if (swapchain.acquired_texture != 0) {
      const auto texture = state.textures.find(swapchain.acquired_texture);
      if (texture != state.textures.end()) {
        static_cast<void>(present_surface(native_surface));
        wgpuTextureRelease(texture->second.texture);
        state.textures.erase(texture);
      }
    }
    wgpuSurfaceUnconfigure(native_surface);
  }
  state.swapchains.clear();
  for (const auto& [handle, surface] : state.surfaces) {
    static_cast<void>(handle);
#if defined(__EMSCRIPTEN__) || defined(GRANIT_WEBGPU_CANVAS_SURFACE_TEST)
    wgpuSurfaceRelease(static_cast<WGPUSurface>(surface.surface));
#else
    static_cast<void>(surface);
#endif
  }
  state.surfaces.clear();
  for (const auto& [handle, command_buffer] : state.command_buffers) {
    static_cast<void>(handle);
    wgpuCommandBufferRelease(command_buffer);
  }
  state.command_buffers.clear();
  for (const auto& [handle, recorder] : state.command_recorders) {
    static_cast<void>(handle);
    if (recorder.pass != nullptr)
      wgpuRenderPassEncoderRelease(recorder.pass);
    if (recorder.compute_pass != nullptr)
      wgpuComputePassEncoderRelease(recorder.compute_pass);
    wgpuCommandEncoderRelease(recorder.encoder);
  }
  state.command_recorders.clear();
  for (const auto& [handle, pipeline] : state.render_pipelines) {
    static_cast<void>(handle);
    wgpuRenderPipelineRelease(pipeline.render_pipeline);
  }
  state.render_pipelines.clear();
  for (const auto& [handle, pipeline] : state.compute_pipelines) {
    static_cast<void>(handle);
    wgpuComputePipelineRelease(pipeline.compute_pipeline);
  }
  state.compute_pipelines.clear();
  for (const auto& [handle, shader] : state.shaders) {
    static_cast<void>(handle);
    wgpuShaderModuleRelease(shader.shader);
  }
  state.shaders.clear();
  for (const auto& [handle, layout] : state.pipeline_layouts) {
    static_cast<void>(handle);
    wgpuPipelineLayoutRelease(layout.pipeline_layout);
  }
  state.pipeline_layouts.clear();
  for (const auto& [handle, bind_group] : state.bind_groups) {
    static_cast<void>(handle);
    wgpuBindGroupRelease(bind_group.bind_group);
  }
  state.bind_groups.clear();
  for (const auto& [handle, layout] : state.bind_group_layouts) {
    static_cast<void>(handle);
    wgpuBindGroupLayoutRelease(layout.bind_group_layout);
  }
  state.bind_group_layouts.clear();
  for (const auto& [handle, view] : state.texture_views) {
    static_cast<void>(handle);
    wgpuTextureViewRelease(view.view);
  }
  state.texture_views.clear();
  for (const auto& [handle, sampler] : state.samplers) {
    static_cast<void>(handle);
    wgpuSamplerRelease(sampler);
  }
  state.samplers.clear();
  for (const auto& [handle, texture] : state.textures) {
    static_cast<void>(handle);
    wgpuTextureRelease(texture.texture);
  }
  state.textures.clear();
  for (const auto& [handle, buffer] : state.buffers) {
    static_cast<void>(handle);
    wgpuBufferRelease(buffer.buffer);
  }
  state.buffers.clear();
  if (state.queue != nullptr) {
    wgpuQueueRelease(state.queue);
  }
  if (state.device != nullptr) {
    wgpuDeviceRelease(state.device);
  }
  if (state.adapter != nullptr) {
    wgpuAdapterRelease(state.adapter);
  }
  if (state.instance != nullptr) {
    wgpuInstanceRelease(state.instance);
  }
}

void emit(const granit_backend_plugin_host_api& host, granit_diagnostic_severity severity,
          const char* message, std::uint32_t message_length) noexcept {
  if (host.diagnostic_callback == nullptr) {
    return;
  }
  try {
    host.diagnostic_callback(severity, GRANIT_DIAGNOSTIC_CATEGORY_DEVICE, message, message_length,
                             host.diagnostic_user_data);
  } catch (...) {
  }
}

void emit_dawn_message(const granit_backend_plugin_host_api* host,
                       WGPUStringView message) noexcept {
  if (host == nullptr || message.data == nullptr) {
    return;
  }
  const auto length = message.length == WGPU_STRLEN ? std::strlen(message.data) : message.length;
  const auto bounded_length = static_cast<std::uint32_t>(
      (std::min)(length, static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max())));
  emit(*host, GRANIT_DIAGNOSTIC_SEVERITY_ERROR, message.data, bounded_length);
}

void receive_device_lost(const WGPUDevice*, WGPUDeviceLostReason reason, WGPUStringView message,
                         void* data, void*) noexcept {
  auto& state = *static_cast<webgpu_instance*>(data);
  static_cast<void>(state.device_lost_ticket.invoke([&state, reason, message] {
    if (reason == WGPUDeviceLostReason_Destroyed ||
        reason == WGPUDeviceLostReason_CallbackCancelled)
      return;
    state.lifecycle.mark_device_lost();
    emit_dawn_message(&state.host, message);
    constexpr char diagnostic[] = "Dawn WebGPU device lost";
    emit(state.host, GRANIT_DIAGNOSTIC_SEVERITY_ERROR, diagnostic, sizeof(diagnostic) - 1);
  }));
}

#if !defined(__EMSCRIPTEN__)
void receive_adapter(WGPURequestAdapterStatus status, WGPUAdapter adapter, WGPUStringView message,
                     void* data, void*) noexcept {
  auto& request = *static_cast<adapter_request*>(data);
  request.status = status;
  request.adapter = adapter;
  if (message.data != nullptr) {
    const auto length = message.length == WGPU_STRLEN ? std::strlen(message.data) : message.length;
    const auto copy_length = (std::min)(length, sizeof(request.message) - 1);
    std::memcpy(request.message, message.data, copy_length);
    request.message[copy_length] = '\0';
    request.message_length = static_cast<std::uint32_t>(copy_length);
  }
}

void receive_device(WGPURequestDeviceStatus status, WGPUDevice device, WGPUStringView message,
                    void* data, void*) noexcept {
  auto& request = *static_cast<device_request*>(data);
  request.status = status;
  request.device = device;
  if (status != WGPURequestDeviceStatus_Success) {
    emit_dawn_message(request.host, message);
  }
}
#endif

#if defined(__EMSCRIPTEN__)
void receive_device_async(WGPURequestDeviceStatus status, WGPUDevice device, WGPUStringView message,
                          void* data, void*) noexcept {
  auto& state = *static_cast<webgpu_instance*>(data);
  static_cast<void>(state.device_ticket.invoke([&state, status, device, message] {
    if (status != WGPURequestDeviceStatus_Success || device == nullptr) {
      emit_dawn_message(&state.host, message);
      state.lifecycle.mark_failed(GRANIT_ERROR_INITIALIZATION_FAILED);
      return;
    }
    state.device = device;
    state.queue = wgpuDeviceGetQueue(device);
    WGPULimits limits = WGPU_LIMITS_INIT;
    if (state.queue == nullptr || wgpuDeviceGetLimits(device, &limits) != WGPUStatus_Success) {
      state.lifecycle.mark_failed(GRANIT_ERROR_INITIALIZATION_FAILED);
      return;
    }
    state.capabilities = {
        sizeof(granit_backend_plugin_capabilities),
        0,
        limits.minUniformBufferOffsetAlignment,
        limits.minStorageBufferOffsetAlignment,
        limits.maxUniformBufferBindingSize,
        limits.maxStorageBufferBindingSize,
        limits.maxBufferSize,
        limits.maxTextureDimension2D,
        limits.maxBindGroups,
        limits.maxColorAttachments,
        provider_surface_types,
        0,
    };
    state.lifecycle.mark_ready();
    constexpr char diagnostic[] = "Emscripten WebGPU adapter and device are ready";
    emit(state.host, GRANIT_DIAGNOSTIC_SEVERITY_INFO, diagnostic, sizeof(diagnostic) - 1);
  }));
}

void receive_adapter_async(WGPURequestAdapterStatus status, WGPUAdapter adapter,
                           WGPUStringView message, void* data, void*) noexcept {
  auto& state = *static_cast<webgpu_instance*>(data);
  static_cast<void>(state.adapter_ticket.invoke([&state, status, adapter, message] {
    if (status != WGPURequestAdapterStatus_Success || adapter == nullptr) {
      emit_dawn_message(&state.host, message);
      state.lifecycle.mark_failed(GRANIT_ERROR_NO_SUITABLE_DEVICE);
      return;
    }
    state.adapter = adapter;
    WGPUDeviceDescriptor descriptor = WGPU_DEVICE_DESCRIPTOR_INIT;
    descriptor.deviceLostCallbackInfo.mode = WGPUCallbackMode_AllowSpontaneous;
    descriptor.deviceLostCallbackInfo.callback = receive_device_lost;
    descriptor.deviceLostCallbackInfo.userdata1 = &state;
    WGPURequestDeviceCallbackInfo callback = WGPU_REQUEST_DEVICE_CALLBACK_INFO_INIT;
    callback.mode = WGPUCallbackMode_AllowSpontaneous;
    callback.callback = receive_device_async;
    callback.userdata1 = &state;
    static_cast<void>(wgpuAdapterRequestDevice(adapter, &descriptor, callback));
  }));
}
#endif

void receive_map(WGPUMapAsyncStatus status, WGPUStringView message, void* data, void*) noexcept {
  auto& request = *static_cast<map_request*>(data);
  request.status = status;
  if (status != WGPUMapAsyncStatus_Success) {
    emit_dawn_message(request.host, message);
  }
}

#if !defined(__EMSCRIPTEN__)
template <typename Request>
bool wait_for(WGPUInstance instance, WGPUFuture future, Request& request) noexcept {
  WGPUFutureWaitInfo wait_info{future, WGPU_FALSE};
  return wgpuInstanceWaitAny(instance, 1, &wait_info, request_timeout_ns) ==
             WGPUWaitStatus_Success &&
         wait_info.completed && request.status != 0;
}

bool request_adapter(WGPUInstance instance, WGPURequestAdapterOptions& options,
                     adapter_request& request) noexcept {
  request = {};
  const WGPURequestAdapterCallbackInfo callback{nullptr, WGPUCallbackMode_WaitAnyOnly,
                                                receive_adapter, &request, nullptr};
  const auto future = wgpuInstanceRequestAdapter(instance, &options, callback);
  return wait_for(instance, future, request) &&
         request.status == WGPURequestAdapterStatus_Success && request.adapter != nullptr;
}
#endif

granit_result register_instance(webgpu_instance* state,
                                granit_backend_plugin_instance* out_instance) noexcept {
  granit_backend_plugin_instance handle = next_instance.fetch_add(1, std::memory_order_relaxed);
  if (handle == 0) {
    handle = next_instance.fetch_add(1, std::memory_order_relaxed);
  }
  try {
    const std::scoped_lock lock{instances_mutex};
    const auto [iterator, inserted] = instances.emplace(handle, state);
    static_cast<void>(iterator);
    if (!inserted) {
      return GRANIT_ERROR_INTERNAL;
    }
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
  *out_instance = handle;
  return GRANIT_SUCCESS;
}

granit_result create_backend(const granit_backend_plugin_host_api* host,
                             granit_backend_plugin_instance* out_instance) noexcept {
  constexpr std::size_t minimum_host_size =
      offsetof(granit_backend_plugin_host_api, allocator_user_data) + sizeof(void*);
  if (host == nullptr || host->struct_size < minimum_host_size || host->reserved != 0 ||
      out_instance == nullptr || host->allocate == nullptr || host->deallocate == nullptr) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  *out_instance = 0;

  void* memory = nullptr;
  try {
    memory = host->allocate(sizeof(webgpu_instance), alignof(webgpu_instance),
                            host->allocator_user_data);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
  if (memory == nullptr) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  }
  WGPUInstanceDescriptor descriptor{};
#if !defined(__EMSCRIPTEN__)
  constexpr WGPUInstanceFeatureName features[]{WGPUInstanceFeatureName_TimedWaitAny};
  const WGPUInstanceLimits instance_limits{nullptr, 1};
  descriptor.requiredFeatureCount = 1;
  descriptor.requiredFeatures = features;
  descriptor.requiredLimits = &instance_limits;
#endif
  auto* state = new (memory) webgpu_instance{*host, wgpuCreateInstance(&descriptor)};
  if (state->instance == nullptr) {
    state->~webgpu_instance();
    deallocate(*host, memory);
    return GRANIT_ERROR_INITIALIZATION_FAILED;
  }

#if defined(__EMSCRIPTEN__)
  const auto register_result = register_instance(state, out_instance);
  if (register_result != GRANIT_SUCCESS) {
    release_resources(*state);
    state->~webgpu_instance();
    deallocate(*host, memory);
    return register_result;
  }
  WGPURequestAdapterOptions options = WGPU_REQUEST_ADAPTER_OPTIONS_INIT;
  WGPURequestAdapterCallbackInfo callback = WGPU_REQUEST_ADAPTER_CALLBACK_INFO_INIT;
  callback.mode = WGPUCallbackMode_AllowSpontaneous;
  callback.callback = receive_adapter_async;
  callback.userdata1 = state;
  static_cast<void>(wgpuInstanceRequestAdapter(state->instance, &options, callback));
  constexpr char initializing_message[] = "Emscripten WebGPU initialization started";
  emit(*host, GRANIT_DIAGNOSTIC_SEVERITY_INFO, initializing_message,
       sizeof(initializing_message) - 1);
  return GRANIT_SUCCESS;
#else

  adapter_request adapter{};
  WGPURequestAdapterOptions adapter_options{};
#if defined(_WIN32)
  adapter_options.backendType = WGPUBackendType_D3D12;
#else
  adapter_options.backendType = WGPUBackendType_Vulkan;
#endif
#if defined(GRANIT_WEBGPU_FORCE_FALLBACK_ADAPTER)
  adapter_options.forceFallbackAdapter = WGPU_TRUE;
#endif
#if defined(GRANIT_WEBGPU_FORCE_FALLBACK_ADAPTER)
  if (!request_adapter(state->instance, adapter_options, adapter)) {
    constexpr char retry_message[] = "WebGPU fallback adapter 请求失败，正在重试普通 adapter";
    emit(*host, GRANIT_DIAGNOSTIC_SEVERITY_WARNING, retry_message, sizeof(retry_message) - 1);
    if (adapter.adapter != nullptr) {
      wgpuAdapterRelease(adapter.adapter);
      adapter.adapter = nullptr;
    }
    adapter_options.forceFallbackAdapter = WGPU_FALSE;
  }
#endif
  if (adapter.adapter == nullptr && !request_adapter(state->instance, adapter_options, adapter)) {
    if (adapter.message_length != 0) {
      emit(*host, GRANIT_DIAGNOSTIC_SEVERITY_ERROR, adapter.message, adapter.message_length);
    }
    constexpr char message[] = "Dawn WebGPU adapter 请求失败或超时";
    emit(*host, GRANIT_DIAGNOSTIC_SEVERITY_ERROR, message, sizeof(message) - 1);
    if (adapter.adapter != nullptr) {
      wgpuAdapterRelease(adapter.adapter);
    }
    wgpuInstanceRelease(state->instance);
    state->~webgpu_instance();
    deallocate(*host, memory);
    return GRANIT_ERROR_NO_SUITABLE_DEVICE;
  }
  state->adapter = adapter.adapter;

  device_request device{host};
  WGPUDeviceDescriptor device_descriptor = WGPU_DEVICE_DESCRIPTOR_INIT;
  device_descriptor.deviceLostCallbackInfo.mode = WGPUCallbackMode_AllowSpontaneous;
  device_descriptor.deviceLostCallbackInfo.callback = receive_device_lost;
  device_descriptor.deviceLostCallbackInfo.userdata1 = state;
  const WGPURequestDeviceCallbackInfo device_callback{nullptr, WGPUCallbackMode_WaitAnyOnly,
                                                      receive_device, &device, nullptr};
  const auto device_future =
      wgpuAdapterRequestDevice(state->adapter, &device_descriptor, device_callback);
  if (!wait_for(state->instance, device_future, device) ||
      device.status != WGPURequestDeviceStatus_Success || device.device == nullptr) {
    constexpr char message[] = "Dawn WebGPU device request failed or timed out";
    emit(*host, GRANIT_DIAGNOSTIC_SEVERITY_ERROR, message, sizeof(message) - 1);
    if (device.device != nullptr) {
      wgpuDeviceRelease(device.device);
    }
    wgpuAdapterRelease(state->adapter);
    wgpuInstanceRelease(state->instance);
    state->~webgpu_instance();
    deallocate(*host, memory);
    return GRANIT_ERROR_INITIALIZATION_FAILED;
  }
  state->device = device.device;
  state->queue = wgpuDeviceGetQueue(state->device);
  if (state->queue == nullptr) {
    constexpr char message[] = "Dawn WebGPU queue request failed";
    emit(*host, GRANIT_DIAGNOSTIC_SEVERITY_ERROR, message, sizeof(message) - 1);
    release_resources(*state);
    state->~webgpu_instance();
    deallocate(*host, memory);
    return GRANIT_ERROR_INITIALIZATION_FAILED;
  }

  WGPULimits device_limits = WGPU_LIMITS_INIT;
  if (wgpuDeviceGetLimits(state->device, &device_limits) != WGPUStatus_Success) {
    constexpr char message[] = "Dawn WebGPU device limits query failed";
    emit(*host, GRANIT_DIAGNOSTIC_SEVERITY_ERROR, message, sizeof(message) - 1);
    release_resources(*state);
    state->~webgpu_instance();
    deallocate(*host, memory);
    return GRANIT_ERROR_INITIALIZATION_FAILED;
  }
  state->capabilities = {
      sizeof(granit_backend_plugin_capabilities),
      0,
      device_limits.minUniformBufferOffsetAlignment,
      device_limits.minStorageBufferOffsetAlignment,
      device_limits.maxUniformBufferBindingSize,
      device_limits.maxStorageBufferBindingSize,
      device_limits.maxBufferSize,
      device_limits.maxTextureDimension2D,
      device_limits.maxBindGroups,
      device_limits.maxColorAttachments,
      provider_surface_types,
      0,
  };
#if defined(GRANIT_WEBGPU_DEFER_INITIALIZATION_TEST)
  const auto extended_host = host->struct_size > sizeof(granit_backend_plugin_host_api);
  state->fail_initialization_for_test = extended_host;
  state->deferred_initialization_for_test = true;
#else
  state->lifecycle.mark_ready();
#endif

  const auto register_result = register_instance(state, out_instance);
  if (register_result != GRANIT_SUCCESS) {
    release_resources(*state);
    state->~webgpu_instance();
    deallocate(*host, memory);
    return register_result;
  }

  constexpr char message[] = "Dawn WebGPU instance, adapter and device created";
  emit(*host, GRANIT_DIAGNOSTIC_SEVERITY_INFO, message, sizeof(message) - 1);
  return GRANIT_SUCCESS;
#endif
}

void destroy_backend(granit_backend_plugin_instance instance) noexcept {
  webgpu_instance* state = nullptr;
  {
    const std::scoped_lock lock{instances_mutex};
    const auto found = instances.find(instance);
    if (found == instances.end()) {
      return;
    }
    state = found->second;
    instances.erase(found);
  }

  const auto host = state->host;
  release_resources(*state);
  state->~webgpu_instance();
  deallocate(host, state);
  constexpr char message[] = "Dawn WebGPU device, adapter and instance destroyed";
  emit(host, GRANIT_DIAGNOSTIC_SEVERITY_INFO, message, sizeof(message) - 1);
}

granit_result get_capabilities(granit_backend_plugin_instance instance,
                               granit_backend_plugin_capabilities* capabilities) noexcept {
  if (instance == 0 || capabilities == nullptr ||
      capabilities->struct_size < sizeof(granit_backend_plugin_capabilities) ||
      capabilities->reserved != 0) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  const std::scoped_lock lock{instances_mutex};
  const auto found = instances.find(instance);
  if (found == instances.end()) {
    return GRANIT_ERROR_INVALID_HANDLE;
  }
  if (const auto ready = require_ready(*found->second); ready != GRANIT_SUCCESS)
    return ready;
  const auto caller_size = capabilities->struct_size;
  *capabilities = found->second->capabilities;
  capabilities->struct_size = caller_size;
  return GRANIT_SUCCESS;
}

granit_result get_instance_status(granit_backend_plugin_instance instance,
                                  granit_backend_plugin_instance_status* status) noexcept {
  if (instance == 0 || status == nullptr ||
      status->struct_size < sizeof(granit_backend_plugin_instance_status) ||
      status->reserved != 0) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  const std::scoped_lock lock{instances_mutex};
  const auto found = instances.find(instance);
  if (found == instances.end()) {
    return GRANIT_ERROR_INVALID_HANDLE;
  }
  const auto caller_size = status->struct_size;
  const auto lifecycle = found->second->lifecycle.status();
  granit_backend_plugin_instance_state plugin_state{};
  switch (lifecycle.state) {
  case granit::detail::backend_lifecycle_state::initializing:
    plugin_state = GRANIT_BACKEND_PLUGIN_INSTANCE_STATE_INITIALIZING;
    break;
  case granit::detail::backend_lifecycle_state::ready:
    plugin_state = GRANIT_BACKEND_PLUGIN_INSTANCE_STATE_READY;
    break;
  case granit::detail::backend_lifecycle_state::failed:
    plugin_state = GRANIT_BACKEND_PLUGIN_INSTANCE_STATE_FAILED;
    break;
  case granit::detail::backend_lifecycle_state::device_lost:
    plugin_state = GRANIT_BACKEND_PLUGIN_INSTANCE_STATE_DEVICE_LOST;
    break;
  }
  *status = {caller_size, plugin_state, lifecycle.failure_result, 0};
  return GRANIT_SUCCESS;
}

granit_result process_events(granit_backend_plugin_instance instance) noexcept {
  if (instance == 0) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  const std::scoped_lock lock{instances_mutex};
  const auto found = instances.find(instance);
  if (found == instances.end()) {
    return GRANIT_ERROR_INVALID_HANDLE;
  }
  wgpuInstanceProcessEvents(found->second->instance);
#if defined(GRANIT_WEBGPU_DEFER_INITIALIZATION_TEST)
  if (found->second->deferred_initialization_for_test) {
    found->second->deferred_initialization_for_test = false;
    if (found->second->fail_initialization_for_test) {
      found->second->fail_initialization_for_test = false;
      found->second->lifecycle.mark_failed(GRANIT_ERROR_INITIALIZATION_FAILED);
      constexpr char message[] = "mock forced WebGPU initialization failure";
      emit(found->second->host, GRANIT_DIAGNOSTIC_SEVERITY_ERROR, message, sizeof(message) - 1);
    } else {
      found->second->force_device_loss_for_test = true;
      found->second->lifecycle.mark_ready();
    }
  } else if (found->second->force_device_loss_for_test) {
    found->second->force_device_loss_for_test = false;
    constexpr char message[] = "mock forced device loss";
    wgpuDeviceForceLoss(found->second->device, WGPUDeviceLostReason_Unknown,
                        {message, sizeof(message) - 1});
  }
#endif
  return found->second->lifecycle.gate();
}

granit_result create_buffer(granit_backend_plugin_instance instance,
                            const granit_backend_plugin_buffer_desc* desc,
                            granit_backend_plugin_buffer* out_buffer) noexcept {
  constexpr std::size_t minimum_size =
      offsetof(granit_backend_plugin_buffer_desc, reserved_flags) + sizeof(std::uint32_t);
  constexpr auto known_usage = GRANIT_BACKEND_PLUGIN_BUFFER_USAGE_MAP_READ_BIT |
                               GRANIT_BACKEND_PLUGIN_BUFFER_USAGE_COPY_SRC_BIT |
                               GRANIT_BACKEND_PLUGIN_BUFFER_USAGE_COPY_DST_BIT |
                               GRANIT_BACKEND_PLUGIN_BUFFER_USAGE_VERTEX_BIT |
                               GRANIT_BACKEND_PLUGIN_BUFFER_USAGE_INDEX_BIT |
                               GRANIT_BACKEND_PLUGIN_BUFFER_USAGE_UNIFORM_BIT |
                               GRANIT_BACKEND_PLUGIN_BUFFER_USAGE_STORAGE_BIT;
  if (out_buffer != nullptr) {
    *out_buffer = 0;
  }
  if (instance == 0 || desc == nullptr || out_buffer == nullptr ||
      desc->struct_size < minimum_size || desc->reserved != 0 || desc->reserved_flags != 0 ||
      desc->size == 0 || desc->usage == 0 || (desc->usage & ~known_usage) != 0 ||
      ((desc->usage & GRANIT_BACKEND_PLUGIN_BUFFER_USAGE_MAP_READ_BIT) != 0 &&
       (desc->usage & ~GRANIT_BACKEND_PLUGIN_BUFFER_USAGE_MAP_READ_BIT &
        ~GRANIT_BACKEND_PLUGIN_BUFFER_USAGE_COPY_DST_BIT) != 0)) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  const std::scoped_lock lock{instances_mutex};
  const auto found = instances.find(instance);
  if (found == instances.end()) {
    return GRANIT_ERROR_INVALID_HANDLE;
  }
  if (const auto ready = require_ready(*found->second); ready != GRANIT_SUCCESS)
    return ready;
  auto& state = *found->second;
  if (desc->size > state.capabilities.max_buffer_size) {
    return GRANIT_ERROR_UNSUPPORTED;
  }

  WGPUBufferUsage usage = WGPUBufferUsage_None;
  if ((desc->usage & GRANIT_BACKEND_PLUGIN_BUFFER_USAGE_MAP_READ_BIT) != 0)
    usage |= WGPUBufferUsage_MapRead;
  if ((desc->usage & GRANIT_BACKEND_PLUGIN_BUFFER_USAGE_COPY_SRC_BIT) != 0)
    usage |= WGPUBufferUsage_CopySrc;
  if ((desc->usage & GRANIT_BACKEND_PLUGIN_BUFFER_USAGE_COPY_DST_BIT) != 0)
    usage |= WGPUBufferUsage_CopyDst;
  if ((desc->usage & GRANIT_BACKEND_PLUGIN_BUFFER_USAGE_VERTEX_BIT) != 0)
    usage |= WGPUBufferUsage_Vertex;
  if ((desc->usage & GRANIT_BACKEND_PLUGIN_BUFFER_USAGE_INDEX_BIT) != 0)
    usage |= WGPUBufferUsage_Index;
  if ((desc->usage & GRANIT_BACKEND_PLUGIN_BUFFER_USAGE_UNIFORM_BIT) != 0)
    usage |= WGPUBufferUsage_Uniform;
  if ((desc->usage & GRANIT_BACKEND_PLUGIN_BUFFER_USAGE_STORAGE_BIT) != 0)
    usage |= WGPUBufferUsage_Storage;
  WGPUBufferDescriptor descriptor = WGPU_BUFFER_DESCRIPTOR_INIT;
  descriptor.usage = usage;
  descriptor.size = desc->size;
  const auto native = wgpuDeviceCreateBuffer(state.device, &descriptor);
  if (native == nullptr) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  }
  auto handle = next_buffer.fetch_add(1, std::memory_order_relaxed);
  if (handle == 0) {
    handle = next_buffer.fetch_add(1, std::memory_order_relaxed);
  }
  try {
    const auto [iterator, inserted] = state.buffers.emplace(
        handle, webgpu_instance::buffer_record{native, desc->size, desc->usage});
    static_cast<void>(iterator);
    if (!inserted) {
      wgpuBufferRelease(native);
      return GRANIT_ERROR_INTERNAL;
    }
  } catch (const std::bad_alloc&) {
    wgpuBufferRelease(native);
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    wgpuBufferRelease(native);
    return GRANIT_ERROR_INTERNAL;
  }
  *out_buffer = handle;
  return GRANIT_SUCCESS;
}

granit_result destroy_buffer(granit_backend_plugin_instance instance,
                             granit_backend_plugin_buffer buffer) noexcept {
  if (instance == 0 || buffer == 0) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  const std::scoped_lock lock{instances_mutex};
  const auto found = instances.find(instance);
  if (found == instances.end()) {
    return GRANIT_ERROR_INVALID_HANDLE;
  }
  const auto buffer_found = found->second->buffers.find(buffer);
  if (buffer_found == found->second->buffers.end()) {
    return GRANIT_ERROR_INVALID_HANDLE;
  }
  if (std::any_of(found->second->bind_groups.begin(), found->second->bind_groups.end(),
                  [buffer](const auto& entry) {
                    return std::find(entry.second.buffers.begin(), entry.second.buffers.end(),
                                     buffer) != entry.second.buffers.end();
                  }))
    return GRANIT_ERROR_INVALID_ARGUMENT;
  wgpuBufferRelease(buffer_found->second.buffer);
  found->second->buffers.erase(buffer_found);
  return GRANIT_SUCCESS;
}

granit_result write_buffer(granit_backend_plugin_instance instance,
                           granit_backend_plugin_buffer buffer, std::uint64_t offset,
                           const void* data, std::uint64_t size) noexcept {
  if (instance == 0 || buffer == 0 || data == nullptr || size == 0 || offset % 4 != 0 ||
      size % 4 != 0 || size > static_cast<std::uint64_t>(SIZE_MAX)) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  const std::scoped_lock lock{instances_mutex};
  const auto found = instances.find(instance);
  if (found == instances.end()) {
    return GRANIT_ERROR_INVALID_HANDLE;
  }
  if (const auto ready = require_ready(*found->second); ready != GRANIT_SUCCESS)
    return ready;
  const auto buffer_found = found->second->buffers.find(buffer);
  if (buffer_found == found->second->buffers.end()) {
    return GRANIT_ERROR_INVALID_HANDLE;
  }
  const auto& record = buffer_found->second;
  if ((record.usage & GRANIT_BACKEND_PLUGIN_BUFFER_USAGE_COPY_DST_BIT) == 0 ||
      offset > record.size || size > record.size - offset) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  wgpuQueueWriteBuffer(found->second->queue, record.buffer, offset, data,
                       static_cast<std::size_t>(size));
  return GRANIT_SUCCESS;
}

granit_result read_buffer(granit_backend_plugin_instance instance,
                          granit_backend_plugin_buffer buffer, std::uint64_t offset, void* data,
                          std::uint64_t size) noexcept {
  if (instance == 0 || buffer == 0 || data == nullptr || size == 0 || offset % 8 != 0 ||
      size % 4 != 0 || size > static_cast<std::uint64_t>(SIZE_MAX)) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  const std::scoped_lock lock{instances_mutex};
  const auto found = instances.find(instance);
  if (found == instances.end()) {
    return GRANIT_ERROR_INVALID_HANDLE;
  }
  if (const auto ready = require_ready(*found->second); ready != GRANIT_SUCCESS)
    return ready;
  const auto buffer_found = found->second->buffers.find(buffer);
  if (buffer_found == found->second->buffers.end()) {
    return GRANIT_ERROR_INVALID_HANDLE;
  }
  const auto& record = buffer_found->second;
  if ((record.usage & GRANIT_BACKEND_PLUGIN_BUFFER_USAGE_MAP_READ_BIT) == 0 ||
      offset > record.size || size > record.size - offset) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }

  map_request request{&found->second->host};
  const WGPUBufferMapCallbackInfo callback{nullptr, WGPUCallbackMode_WaitAnyOnly, receive_map,
                                           &request, nullptr};
  const auto future =
      wgpuBufferMapAsync(record.buffer, WGPUMapMode_Read, static_cast<std::size_t>(offset),
                         static_cast<std::size_t>(size), callback);
  WGPUFutureWaitInfo wait_info{future, WGPU_FALSE};
  const auto wait_status =
      wgpuInstanceWaitAny(found->second->instance, 1, &wait_info, request_timeout_ns);
  if (wait_status != WGPUWaitStatus_Success || !wait_info.completed) {
    wgpuBufferUnmap(record.buffer);
    return GRANIT_ERROR_NOT_READY;
  }
  if (request.status != WGPUMapAsyncStatus_Success) {
    wgpuBufferUnmap(record.buffer);
    return GRANIT_ERROR_INTERNAL;
  }
  const auto* mapped = wgpuBufferGetConstMappedRange(
      record.buffer, static_cast<std::size_t>(offset), static_cast<std::size_t>(size));
  if (mapped == nullptr) {
    wgpuBufferUnmap(record.buffer);
    return GRANIT_ERROR_INTERNAL;
  }
  std::memcpy(data, mapped, static_cast<std::size_t>(size));
  wgpuBufferUnmap(record.buffer);
  return GRANIT_SUCCESS;
}

template <typename Handle> Handle next_handle(std::atomic_uint64_t& counter) noexcept {
  auto handle = counter.fetch_add(1, std::memory_order_relaxed);
  if (handle == 0) {
    handle = counter.fetch_add(1, std::memory_order_relaxed);
  }
  return static_cast<Handle>(handle);
}

WGPUTextureFormat to_native_texture_format(granit_backend_plugin_texture_format format) noexcept {
  switch (format) {
  case GRANIT_BACKEND_PLUGIN_TEXTURE_FORMAT_R8_UNORM:
    return WGPUTextureFormat_R8Unorm;
  case GRANIT_BACKEND_PLUGIN_TEXTURE_FORMAT_RG8_UNORM:
    return WGPUTextureFormat_RG8Unorm;
  case GRANIT_BACKEND_PLUGIN_TEXTURE_FORMAT_RGBA8_UNORM:
    return WGPUTextureFormat_RGBA8Unorm;
  case GRANIT_BACKEND_PLUGIN_TEXTURE_FORMAT_RGBA8_SRGB:
    return WGPUTextureFormat_RGBA8UnormSrgb;
  case GRANIT_BACKEND_PLUGIN_TEXTURE_FORMAT_D32_FLOAT:
    return WGPUTextureFormat_Depth32Float;
  case GRANIT_BACKEND_PLUGIN_TEXTURE_FORMAT_RGBA16_FLOAT:
    return WGPUTextureFormat_RGBA16Float;
  default:
    return WGPUTextureFormat_Undefined;
  }
}

WGPUCompareFunction
to_native_compare_operation(granit_backend_plugin_compare_operation operation) noexcept {
  switch (operation) {
  case GRANIT_BACKEND_PLUGIN_COMPARE_OPERATION_NEVER:
    return WGPUCompareFunction_Never;
  case GRANIT_BACKEND_PLUGIN_COMPARE_OPERATION_LESS:
    return WGPUCompareFunction_Less;
  case GRANIT_BACKEND_PLUGIN_COMPARE_OPERATION_EQUAL:
    return WGPUCompareFunction_Equal;
  case GRANIT_BACKEND_PLUGIN_COMPARE_OPERATION_LESS_EQUAL:
    return WGPUCompareFunction_LessEqual;
  case GRANIT_BACKEND_PLUGIN_COMPARE_OPERATION_GREATER:
    return WGPUCompareFunction_Greater;
  case GRANIT_BACKEND_PLUGIN_COMPARE_OPERATION_NOT_EQUAL:
    return WGPUCompareFunction_NotEqual;
  case GRANIT_BACKEND_PLUGIN_COMPARE_OPERATION_GREATER_EQUAL:
    return WGPUCompareFunction_GreaterEqual;
  case GRANIT_BACKEND_PLUGIN_COMPARE_OPERATION_ALWAYS:
    return WGPUCompareFunction_Always;
  default:
    return WGPUCompareFunction_Undefined;
  }
}

std::uint32_t texture_bytes_per_pixel(granit_backend_plugin_texture_format format) noexcept {
  switch (format) {
  case GRANIT_BACKEND_PLUGIN_TEXTURE_FORMAT_R8_UNORM:
    return 1;
  case GRANIT_BACKEND_PLUGIN_TEXTURE_FORMAT_RG8_UNORM:
    return 2;
  case GRANIT_BACKEND_PLUGIN_TEXTURE_FORMAT_RGBA8_UNORM:
  case GRANIT_BACKEND_PLUGIN_TEXTURE_FORMAT_RGBA8_SRGB:
  case GRANIT_BACKEND_PLUGIN_TEXTURE_FORMAT_D32_FLOAT:
    return 4;
  case GRANIT_BACKEND_PLUGIN_TEXTURE_FORMAT_RGBA16_FLOAT:
    return 8;
  default:
    return 0;
  }
}

granit_result create_texture(granit_backend_plugin_instance instance,
                             const granit_backend_plugin_texture_desc* desc,
                             granit_backend_plugin_texture* out_texture) noexcept {
  constexpr auto known_usage = GRANIT_BACKEND_PLUGIN_TEXTURE_USAGE_COPY_SRC_BIT |
                               GRANIT_BACKEND_PLUGIN_TEXTURE_USAGE_COPY_DST_BIT |
                               GRANIT_BACKEND_PLUGIN_TEXTURE_USAGE_SAMPLED_BIT |
                               GRANIT_BACKEND_PLUGIN_TEXTURE_USAGE_RENDER_ATTACHMENT_BIT;
  if (out_texture != nullptr) {
    *out_texture = 0;
  }
  const auto dimension = desc != nullptr && desc->dimension != 0
                             ? desc->dimension
                             : GRANIT_BACKEND_PLUGIN_TEXTURE_DIMENSION_2D;
  const auto array_layer_count =
      desc != nullptr && desc->array_layer_count != 0 ? desc->array_layer_count : 1;
  if (instance == 0 || desc == nullptr || out_texture == nullptr ||
      desc->struct_size < sizeof(granit_backend_plugin_texture_desc) || desc->reserved != 0 ||
      desc->width == 0 || desc->height == 0 || desc->usage == 0 || desc->mip_level_count == 0 ||
      (dimension != GRANIT_BACKEND_PLUGIN_TEXTURE_DIMENSION_2D &&
       dimension != GRANIT_BACKEND_PLUGIN_TEXTURE_DIMENSION_CUBE) ||
      (dimension == GRANIT_BACKEND_PLUGIN_TEXTURE_DIMENSION_CUBE &&
       (desc->width != desc->height || array_layer_count != 6)) ||
      to_native_texture_format(desc->format) == WGPUTextureFormat_Undefined ||
      (desc->usage & ~known_usage) != 0) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  const std::scoped_lock lock{instances_mutex};
  const auto found = instances.find(instance);
  if (found == instances.end()) {
    return GRANIT_ERROR_INVALID_HANDLE;
  }
  if (const auto ready = require_ready(*found->second); ready != GRANIT_SUCCESS)
    return ready;
  auto& state = *found->second;
  if (desc->width > state.capabilities.max_texture_dimension_2d ||
      desc->height > state.capabilities.max_texture_dimension_2d) {
    return GRANIT_ERROR_UNSUPPORTED;
  }
  WGPUTextureUsage usage = WGPUTextureUsage_None;
  if ((desc->usage & GRANIT_BACKEND_PLUGIN_TEXTURE_USAGE_COPY_SRC_BIT) != 0)
    usage |= WGPUTextureUsage_CopySrc;
  if ((desc->usage & GRANIT_BACKEND_PLUGIN_TEXTURE_USAGE_COPY_DST_BIT) != 0)
    usage |= WGPUTextureUsage_CopyDst;
  if ((desc->usage & GRANIT_BACKEND_PLUGIN_TEXTURE_USAGE_SAMPLED_BIT) != 0)
    usage |= WGPUTextureUsage_TextureBinding;
  if ((desc->usage & GRANIT_BACKEND_PLUGIN_TEXTURE_USAGE_RENDER_ATTACHMENT_BIT) != 0)
    usage |= WGPUTextureUsage_RenderAttachment;
  WGPUTextureDescriptor descriptor = WGPU_TEXTURE_DESCRIPTOR_INIT;
  descriptor.usage = usage;
  descriptor.dimension = WGPUTextureDimension_2D;
  descriptor.size = {desc->width, desc->height, array_layer_count};
  descriptor.format = to_native_texture_format(desc->format);
  descriptor.mipLevelCount = desc->mip_level_count;
  descriptor.sampleCount = 1;
  const auto native = wgpuDeviceCreateTexture(state.device, &descriptor);
  if (native == nullptr) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  }
  const auto handle = next_handle<granit_backend_plugin_texture>(next_texture);
  try {
    if (!state.textures
             .emplace(handle, webgpu_instance::texture_record{native, desc->width, desc->height,
                                                              desc->format, desc->mip_level_count,
                                                              array_layer_count, desc->usage,
                                                              false})
             .second) {
      wgpuTextureRelease(native);
      return GRANIT_ERROR_INTERNAL;
    }
  } catch (const std::bad_alloc&) {
    wgpuTextureRelease(native);
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    wgpuTextureRelease(native);
    return GRANIT_ERROR_INTERNAL;
  }
  *out_texture = handle;
  return GRANIT_SUCCESS;
}

granit_result destroy_texture(granit_backend_plugin_instance instance,
                              granit_backend_plugin_texture texture) noexcept {
  if (instance == 0 || texture == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const std::scoped_lock lock{instances_mutex};
  const auto found = instances.find(instance);
  if (found == instances.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  auto& state = *found->second;
  const auto texture_found = state.textures.find(texture);
  if (texture_found == state.textures.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  if (texture_found->second.borrowed)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  if (std::any_of(state.texture_views.begin(), state.texture_views.end(),
                  [texture](const auto& entry) { return entry.second.texture == texture; })) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  wgpuTextureRelease(texture_found->second.texture);
  state.textures.erase(texture_found);
  return GRANIT_SUCCESS;
}

granit_result write_texture(granit_backend_plugin_instance instance,
                            granit_backend_plugin_texture texture,
                            const granit_backend_plugin_texture_write_desc* desc, const void* data,
                            std::uint64_t size) noexcept {
  const auto array_layer_count =
      desc != nullptr && desc->array_layer_count != 0 ? desc->array_layer_count : 1;
  if (instance == 0 || texture == 0 || desc == nullptr || data == nullptr || size == 0 ||
      desc->struct_size < sizeof(granit_backend_plugin_texture_write_desc) ||
      desc->width == 0 || desc->height == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const std::scoped_lock lock{instances_mutex};
  const auto found = instances.find(instance);
  if (found == instances.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  if (const auto ready = require_ready(*found->second); ready != GRANIT_SUCCESS)
    return ready;
  const auto texture_found = found->second->textures.find(texture);
  if (texture_found == found->second->textures.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  const auto& record = texture_found->second;
  if (record.borrowed || (record.usage & GRANIT_BACKEND_PLUGIN_TEXTURE_USAGE_COPY_DST_BIT) == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  if (desc->mip_level >= record.mip_level_count ||
      desc->base_array_layer >= record.array_layer_count ||
      array_layer_count > record.array_layer_count - desc->base_array_layer)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const auto mip_width = std::max(UINT32_C(1), record.width >> desc->mip_level);
  const auto mip_height = std::max(UINT32_C(1), record.height >> desc->mip_level);
  if (desc->x >= mip_width || desc->width > mip_width - desc->x || desc->y >= mip_height ||
      desc->height > mip_height - desc->y)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const std::uint64_t tight_row =
      std::uint64_t{desc->width} * texture_bytes_per_pixel(record.format);
  const std::uint64_t row_pitch = desc->bytes_per_row == 0 ? tight_row : desc->bytes_per_row;
  const std::uint64_t rows = desc->rows_per_image == 0 ? desc->height : desc->rows_per_image;
  if (row_pitch < tight_row || rows < desc->height ||
      row_pitch > std::numeric_limits<std::uint32_t>::max())
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const std::uint64_t image_pitch = rows * row_pitch;
  const std::uint64_t required =
      (std::uint64_t{array_layer_count} - 1) * image_pitch +
      (std::uint64_t{desc->height} - 1) * row_pitch + tight_row;
  if (required > size || size > std::numeric_limits<std::size_t>::max())
    return GRANIT_ERROR_INVALID_ARGUMENT;
  WGPUTexelCopyTextureInfo destination = WGPU_TEXEL_COPY_TEXTURE_INFO_INIT;
  destination.texture = record.texture;
  destination.mipLevel = desc->mip_level;
  destination.origin = {desc->x, desc->y, desc->base_array_layer};
  destination.aspect = WGPUTextureAspect_All;
  WGPUTexelCopyBufferLayout layout{};
  layout.offset = 0;
  layout.bytesPerRow = static_cast<std::uint32_t>(row_pitch);
  layout.rowsPerImage = static_cast<std::uint32_t>(rows);
  const WGPUExtent3D extent{desc->width, desc->height, array_layer_count};
  wgpuQueueWriteTexture(found->second->queue, &destination, data, static_cast<std::size_t>(size),
                        &layout, &extent);
  return GRANIT_SUCCESS;
}

granit_result write_upload_batch(granit_backend_plugin_instance instance,
                                 const granit_backend_plugin_upload_operation* operations,
                                 std::uint32_t operation_count) noexcept {
  if (instance == 0 || operations == nullptr || operation_count == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const std::scoped_lock lock{instances_mutex};
  const auto found = instances.find(instance);
  if (found == instances.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  if (const auto ready = require_ready(*found->second); ready != GRANIT_SUCCESS)
    return ready;
  auto& state = *found->second;

  for (std::uint32_t index = 0; index < operation_count; ++index) {
    const auto& operation = operations[index];
    if (operation.struct_size < sizeof(granit_backend_plugin_upload_operation) ||
        operation.data == nullptr || operation.size == 0 || operation.reserved != 0 ||
        operation.size > static_cast<std::uint64_t>(SIZE_MAX))
      return GRANIT_ERROR_INVALID_ARGUMENT;
    if (operation.type == GRANIT_BACKEND_PLUGIN_UPLOAD_TYPE_BUFFER) {
      if (operation.buffer == 0 || operation.texture != 0 ||
          operation.texture_write.struct_size != 0 || operation.destination_offset % 4 != 0 ||
          operation.size % 4 != 0)
        return GRANIT_ERROR_INVALID_ARGUMENT;
      const auto buffer = state.buffers.find(operation.buffer);
      if (buffer == state.buffers.end())
        return GRANIT_ERROR_INVALID_HANDLE;
      if ((buffer->second.usage & GRANIT_BACKEND_PLUGIN_BUFFER_USAGE_COPY_DST_BIT) == 0 ||
          operation.destination_offset > buffer->second.size ||
          operation.size > buffer->second.size - operation.destination_offset)
        return GRANIT_ERROR_INVALID_ARGUMENT;
      continue;
    }
    if (operation.type != GRANIT_BACKEND_PLUGIN_UPLOAD_TYPE_TEXTURE || operation.buffer != 0 ||
        operation.texture == 0 || operation.destination_offset != 0)
      return GRANIT_ERROR_INVALID_ARGUMENT;
    const auto& desc = operation.texture_write;
    if (desc.struct_size < sizeof(granit_backend_plugin_texture_write_desc) ||
        desc.width == 0 || desc.height == 0)
      return GRANIT_ERROR_INVALID_ARGUMENT;
    const auto texture = state.textures.find(operation.texture);
    if (texture == state.textures.end())
      return GRANIT_ERROR_INVALID_HANDLE;
    const auto& record = texture->second;
    const auto array_layer_count = desc.array_layer_count == 0 ? 1 : desc.array_layer_count;
    if (record.borrowed || (record.usage & GRANIT_BACKEND_PLUGIN_TEXTURE_USAGE_COPY_DST_BIT) == 0 ||
        desc.mip_level >= record.mip_level_count ||
        desc.base_array_layer >= record.array_layer_count ||
        array_layer_count > record.array_layer_count - desc.base_array_layer)
      return GRANIT_ERROR_INVALID_ARGUMENT;
    const auto mip_width = std::max(UINT32_C(1), record.width >> desc.mip_level);
    const auto mip_height = std::max(UINT32_C(1), record.height >> desc.mip_level);
    if (desc.x >= mip_width || desc.width > mip_width - desc.x || desc.y >= mip_height ||
        desc.height > mip_height - desc.y)
      return GRANIT_ERROR_INVALID_ARGUMENT;
    const std::uint64_t tight_row =
        std::uint64_t{desc.width} * texture_bytes_per_pixel(record.format);
    const std::uint64_t row_pitch = desc.bytes_per_row == 0 ? tight_row : desc.bytes_per_row;
    const std::uint64_t rows = desc.rows_per_image == 0 ? desc.height : desc.rows_per_image;
    if (row_pitch < tight_row || rows < desc.height ||
        row_pitch > std::numeric_limits<std::uint32_t>::max())
      return GRANIT_ERROR_INVALID_ARGUMENT;
    const std::uint64_t image_pitch = rows * row_pitch;
    const std::uint64_t required =
        (std::uint64_t{array_layer_count} - 1) * image_pitch +
        (std::uint64_t{desc.height} - 1) * row_pitch + tight_row;
    if (required > operation.size)
      return GRANIT_ERROR_INVALID_ARGUMENT;
  }

  for (std::uint32_t index = 0; index < operation_count; ++index) {
    const auto& operation = operations[index];
    if (operation.type == GRANIT_BACKEND_PLUGIN_UPLOAD_TYPE_BUFFER) {
      const auto& buffer = state.buffers.find(operation.buffer)->second;
      wgpuQueueWriteBuffer(state.queue, buffer.buffer, operation.destination_offset, operation.data,
                           static_cast<std::size_t>(operation.size));
      continue;
    }
    const auto& desc = operation.texture_write;
    const auto& texture = state.textures.find(operation.texture)->second;
    const std::uint64_t tight_row =
        std::uint64_t{desc.width} * texture_bytes_per_pixel(texture.format);
    const auto row_pitch =
        static_cast<std::uint32_t>(desc.bytes_per_row == 0 ? tight_row : desc.bytes_per_row);
    const auto rows = desc.rows_per_image == 0 ? desc.height : desc.rows_per_image;
    const auto array_layer_count = desc.array_layer_count == 0 ? 1 : desc.array_layer_count;
    WGPUTexelCopyTextureInfo destination = WGPU_TEXEL_COPY_TEXTURE_INFO_INIT;
    destination.texture = texture.texture;
    destination.mipLevel = desc.mip_level;
    destination.origin = {desc.x, desc.y, desc.base_array_layer};
    destination.aspect = WGPUTextureAspect_All;
    WGPUTexelCopyBufferLayout layout{};
    layout.bytesPerRow = row_pitch;
    layout.rowsPerImage = rows;
    const WGPUExtent3D extent{desc.width, desc.height, array_layer_count};
    wgpuQueueWriteTexture(state.queue, &destination, operation.data,
                          static_cast<std::size_t>(operation.size), &layout, &extent);
  }
  return GRANIT_SUCCESS;
}

granit_result create_texture_view(granit_backend_plugin_instance instance,
                                  granit_backend_plugin_texture texture,
                                  const granit_backend_plugin_texture_view_desc* desc,
                                  granit_backend_plugin_texture_view* out_view) noexcept {
  if (out_view != nullptr)
    *out_view = 0;
  const auto dimension = desc != nullptr && desc->dimension != 0
                             ? desc->dimension
                             : GRANIT_BACKEND_PLUGIN_TEXTURE_DIMENSION_2D;
  const auto array_layer_count =
      desc != nullptr && desc->array_layer_count != 0 ? desc->array_layer_count : 1;
  if (instance == 0 || texture == 0 || desc == nullptr || out_view == nullptr ||
      desc->struct_size < sizeof(granit_backend_plugin_texture_view_desc) ||
      desc->mip_level_count == 0 ||
      (dimension != GRANIT_BACKEND_PLUGIN_TEXTURE_DIMENSION_2D &&
       dimension != GRANIT_BACKEND_PLUGIN_TEXTURE_DIMENSION_CUBE))
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const std::scoped_lock lock{instances_mutex};
  const auto found = instances.find(instance);
  if (found == instances.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  if (const auto ready = require_ready(*found->second); ready != GRANIT_SUCCESS)
    return ready;
  auto& state = *found->second;
  const auto texture_found = state.textures.find(texture);
  if (texture_found == state.textures.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  if (desc->format != texture_found->second.format ||
      desc->base_mip_level >= texture_found->second.mip_level_count ||
      desc->mip_level_count > texture_found->second.mip_level_count - desc->base_mip_level ||
      desc->base_array_layer >= texture_found->second.array_layer_count ||
      array_layer_count >
          texture_found->second.array_layer_count - desc->base_array_layer ||
      (dimension == GRANIT_BACKEND_PLUGIN_TEXTURE_DIMENSION_CUBE && array_layer_count != 6))
    return GRANIT_ERROR_INVALID_ARGUMENT;
  WGPUTextureViewDescriptor descriptor = WGPU_TEXTURE_VIEW_DESCRIPTOR_INIT;
  descriptor.format = to_native_texture_format(desc->format);
  descriptor.dimension = dimension == GRANIT_BACKEND_PLUGIN_TEXTURE_DIMENSION_CUBE
                             ? WGPUTextureViewDimension_Cube
                             : WGPUTextureViewDimension_2D;
  descriptor.baseMipLevel = desc->base_mip_level;
  descriptor.mipLevelCount = desc->mip_level_count;
  descriptor.baseArrayLayer = desc->base_array_layer;
  descriptor.arrayLayerCount = array_layer_count;
  descriptor.aspect = WGPUTextureAspect_All;
  const auto native = wgpuTextureCreateView(texture_found->second.texture, &descriptor);
  if (native == nullptr)
    return GRANIT_ERROR_OUT_OF_MEMORY;
  const auto handle = next_handle<granit_backend_plugin_texture_view>(next_texture_view);
  try {
    if (!state.texture_views
             .emplace(handle, webgpu_instance::texture_view_record{native, texture, false})
             .second) {
      wgpuTextureViewRelease(native);
      return GRANIT_ERROR_INTERNAL;
    }
  } catch (const std::bad_alloc&) {
    wgpuTextureViewRelease(native);
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    wgpuTextureViewRelease(native);
    return GRANIT_ERROR_INTERNAL;
  }
  *out_view = handle;
  return GRANIT_SUCCESS;
}

granit_result destroy_texture_view(granit_backend_plugin_instance instance,
                                   granit_backend_plugin_texture_view view) noexcept {
  if (instance == 0 || view == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const std::scoped_lock lock{instances_mutex};
  const auto found = instances.find(instance);
  if (found == instances.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  const auto view_found = found->second->texture_views.find(view);
  if (view_found == found->second->texture_views.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  if (view_found->second.borrowed)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  if (std::any_of(found->second->bind_groups.begin(), found->second->bind_groups.end(),
                  [view](const auto& entry) {
                    return std::find(entry.second.texture_views.begin(),
                                     entry.second.texture_views.end(),
                                     view) != entry.second.texture_views.end();
                  }))
    return GRANIT_ERROR_INVALID_ARGUMENT;
  wgpuTextureViewRelease(view_found->second.view);
  found->second->texture_views.erase(view_found);
  return GRANIT_SUCCESS;
}

granit_result create_sampler(granit_backend_plugin_instance instance,
                             const granit_backend_plugin_sampler_desc* desc,
                             granit_backend_plugin_sampler* out_sampler) noexcept {
  if (out_sampler != nullptr)
    *out_sampler = 0;
  const auto valid_filter = [](granit_backend_plugin_filter filter) {
    return filter == GRANIT_BACKEND_PLUGIN_FILTER_NEAREST ||
           filter == GRANIT_BACKEND_PLUGIN_FILTER_LINEAR;
  };
  const auto valid_address_mode = [](granit_backend_plugin_address_mode mode) {
    return mode >= GRANIT_BACKEND_PLUGIN_ADDRESS_MODE_REPEAT &&
           mode <= GRANIT_BACKEND_PLUGIN_ADDRESS_MODE_CLAMP_TO_EDGE;
  };
  if (instance == 0 || desc == nullptr || out_sampler == nullptr ||
      desc->struct_size < sizeof(granit_backend_plugin_sampler_desc) || desc->reserved != 0 ||
      desc->reserved_2[0] != 0 || desc->reserved_2[1] != 0 || !valid_filter(desc->min_filter) ||
      !valid_filter(desc->mag_filter) || !valid_filter(desc->mipmap_filter) ||
      !valid_address_mode(desc->address_mode_u) || !valid_address_mode(desc->address_mode_v) ||
      !valid_address_mode(desc->address_mode_w) ||
      desc->compare_operation > GRANIT_BACKEND_PLUGIN_COMPARE_OPERATION_ALWAYS ||
      desc->max_anisotropy == 0 || desc->max_anisotropy > UINT16_MAX ||
      (desc->max_anisotropy > 1 && (desc->min_filter != GRANIT_BACKEND_PLUGIN_FILTER_LINEAR ||
                                    desc->mag_filter != GRANIT_BACKEND_PLUGIN_FILTER_LINEAR ||
                                    desc->mipmap_filter != GRANIT_BACKEND_PLUGIN_FILTER_LINEAR)) ||
      !std::isfinite(desc->min_lod) || !std::isfinite(desc->max_lod) || desc->min_lod < 0.0F ||
      desc->max_lod < desc->min_lod) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  const std::scoped_lock lock{instances_mutex};
  const auto found = instances.find(instance);
  if (found == instances.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  if (const auto ready = require_ready(*found->second); ready != GRANIT_SUCCESS)
    return ready;
  const auto to_address_mode = [](granit_backend_plugin_address_mode mode) {
    switch (mode) {
    case GRANIT_BACKEND_PLUGIN_ADDRESS_MODE_REPEAT:
      return WGPUAddressMode_Repeat;
    case GRANIT_BACKEND_PLUGIN_ADDRESS_MODE_MIRROR_REPEAT:
      return WGPUAddressMode_MirrorRepeat;
    default:
      return WGPUAddressMode_ClampToEdge;
    }
  };
  const auto to_compare = [](granit_backend_plugin_compare_operation operation) {
    switch (operation) {
    case GRANIT_BACKEND_PLUGIN_COMPARE_OPERATION_NEVER:
      return WGPUCompareFunction_Never;
    case GRANIT_BACKEND_PLUGIN_COMPARE_OPERATION_LESS:
      return WGPUCompareFunction_Less;
    case GRANIT_BACKEND_PLUGIN_COMPARE_OPERATION_EQUAL:
      return WGPUCompareFunction_Equal;
    case GRANIT_BACKEND_PLUGIN_COMPARE_OPERATION_LESS_EQUAL:
      return WGPUCompareFunction_LessEqual;
    case GRANIT_BACKEND_PLUGIN_COMPARE_OPERATION_GREATER:
      return WGPUCompareFunction_Greater;
    case GRANIT_BACKEND_PLUGIN_COMPARE_OPERATION_NOT_EQUAL:
      return WGPUCompareFunction_NotEqual;
    case GRANIT_BACKEND_PLUGIN_COMPARE_OPERATION_GREATER_EQUAL:
      return WGPUCompareFunction_GreaterEqual;
    case GRANIT_BACKEND_PLUGIN_COMPARE_OPERATION_ALWAYS:
      return WGPUCompareFunction_Always;
    default:
      return WGPUCompareFunction_Undefined;
    }
  };
  WGPUSamplerDescriptor descriptor = WGPU_SAMPLER_DESCRIPTOR_INIT;
  descriptor.addressModeU = to_address_mode(desc->address_mode_u);
  descriptor.addressModeV = to_address_mode(desc->address_mode_v);
  descriptor.addressModeW = to_address_mode(desc->address_mode_w);
  descriptor.minFilter = desc->min_filter == GRANIT_BACKEND_PLUGIN_FILTER_LINEAR
                             ? WGPUFilterMode_Linear
                             : WGPUFilterMode_Nearest;
  descriptor.magFilter = desc->mag_filter == GRANIT_BACKEND_PLUGIN_FILTER_LINEAR
                             ? WGPUFilterMode_Linear
                             : WGPUFilterMode_Nearest;
  descriptor.mipmapFilter = desc->mipmap_filter == GRANIT_BACKEND_PLUGIN_FILTER_LINEAR
                                ? WGPUMipmapFilterMode_Linear
                                : WGPUMipmapFilterMode_Nearest;
  descriptor.lodMinClamp = desc->min_lod;
  descriptor.lodMaxClamp = desc->max_lod;
  descriptor.compare = to_compare(desc->compare_operation);
  descriptor.maxAnisotropy = static_cast<std::uint16_t>(desc->max_anisotropy);
  const auto native = wgpuDeviceCreateSampler(found->second->device, &descriptor);
  if (native == nullptr)
    return GRANIT_ERROR_OUT_OF_MEMORY;
  const auto handle = next_handle<granit_backend_plugin_sampler>(next_sampler);
  try {
    if (!found->second->samplers.emplace(handle, native).second) {
      wgpuSamplerRelease(native);
      return GRANIT_ERROR_INTERNAL;
    }
  } catch (const std::bad_alloc&) {
    wgpuSamplerRelease(native);
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    wgpuSamplerRelease(native);
    return GRANIT_ERROR_INTERNAL;
  }
  *out_sampler = handle;
  return GRANIT_SUCCESS;
}

granit_result destroy_sampler(granit_backend_plugin_instance instance,
                              granit_backend_plugin_sampler sampler) noexcept {
  if (instance == 0 || sampler == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const std::scoped_lock lock{instances_mutex};
  const auto found = instances.find(instance);
  if (found == instances.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  const auto sampler_found = found->second->samplers.find(sampler);
  if (sampler_found == found->second->samplers.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  if (std::any_of(found->second->bind_groups.begin(), found->second->bind_groups.end(),
                  [sampler](const auto& entry) {
                    return std::find(entry.second.samplers.begin(), entry.second.samplers.end(),
                                     sampler) != entry.second.samplers.end();
                  }))
    return GRANIT_ERROR_INVALID_ARGUMENT;
  wgpuSamplerRelease(sampler_found->second);
  found->second->samplers.erase(sampler_found);
  return GRANIT_SUCCESS;
}

granit_result
create_bind_group_layout(granit_backend_plugin_instance instance,
                         const granit_backend_plugin_bind_group_layout_desc* desc,
                         granit_backend_plugin_bind_group_layout* out_layout) noexcept {
  if (out_layout != nullptr)
    *out_layout = 0;
  if (instance == 0 || desc == nullptr || out_layout == nullptr ||
      desc->struct_size < sizeof(granit_backend_plugin_bind_group_layout_desc) ||
      desc->reserved != 0 || (desc->entry_count != 0 && desc->entries == nullptr))
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const std::scoped_lock lock{instances_mutex};
  const auto found = instances.find(instance);
  if (found == instances.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  if (const auto ready = require_ready(*found->second); ready != GRANIT_SUCCESS)
    return ready;
  try {
    std::vector<WGPUBindGroupLayoutEntry> entries(desc->entry_count);
    std::vector<granit_backend_plugin_bind_group_layout_entry> declarations;
    if (desc->entry_count != 0)
      declarations.assign(desc->entries, desc->entries + desc->entry_count);
    for (std::uint32_t index = 0; index < desc->entry_count; ++index) {
      const auto& source = desc->entries[index];
      if (source.array_count != 1 || source.visibility == 0 ||
          (source.visibility & ~UINT32_C(7)) != 0 ||
          std::any_of(desc->entries, desc->entries + index,
                      [&](const auto& previous) { return previous.binding == source.binding; }))
        return GRANIT_ERROR_INVALID_ARGUMENT;
      auto& entry = entries[index];
      entry = WGPU_BIND_GROUP_LAYOUT_ENTRY_INIT;
      entry.binding = source.binding;
      entry.visibility = source.visibility;
      switch (source.type) {
      case GRANIT_BACKEND_PLUGIN_BINDING_TYPE_UNIFORM_BUFFER:
        entry.buffer.type = WGPUBufferBindingType_Uniform;
        break;
      case GRANIT_BACKEND_PLUGIN_BINDING_TYPE_DYNAMIC_UNIFORM_BUFFER:
        entry.buffer.type = WGPUBufferBindingType_Uniform;
        entry.buffer.hasDynamicOffset = WGPU_TRUE;
        break;
      case GRANIT_BACKEND_PLUGIN_BINDING_TYPE_STORAGE_BUFFER:
        entry.buffer.type = WGPUBufferBindingType_Storage;
        break;
      case GRANIT_BACKEND_PLUGIN_BINDING_TYPE_SAMPLED_TEXTURE:
        entry.texture.sampleType = WGPUTextureSampleType_Float;
        entry.texture.viewDimension = WGPUTextureViewDimension_2D;
        break;
      case GRANIT_BACKEND_PLUGIN_BINDING_TYPE_SAMPLED_TEXTURE_CUBE:
        entry.texture.sampleType = WGPUTextureSampleType_Float;
        entry.texture.viewDimension = WGPUTextureViewDimension_Cube;
        break;
      case GRANIT_BACKEND_PLUGIN_BINDING_TYPE_SAMPLER:
        entry.sampler.type = WGPUSamplerBindingType_Filtering;
        break;
      default:
        return GRANIT_ERROR_INVALID_ARGUMENT;
      }
    }
    std::sort(declarations.begin(), declarations.end(),
              [](const auto& left, const auto& right) { return left.binding < right.binding; });
    WGPUBindGroupLayoutDescriptor descriptor = WGPU_BIND_GROUP_LAYOUT_DESCRIPTOR_INIT;
    descriptor.entryCount = entries.size();
    descriptor.entries = entries.data();
    const auto native = wgpuDeviceCreateBindGroupLayout(found->second->device, &descriptor);
    if (native == nullptr)
      return GRANIT_ERROR_OUT_OF_MEMORY;
    const auto handle =
        next_handle<granit_backend_plugin_bind_group_layout>(next_bind_group_layout);
    try {
      webgpu_instance::bind_group_layout_record record{native, std::move(declarations)};
      if (!found->second->bind_group_layouts.emplace(handle, std::move(record)).second) {
        wgpuBindGroupLayoutRelease(native);
        return GRANIT_ERROR_INTERNAL;
      }
    } catch (const std::bad_alloc&) {
      wgpuBindGroupLayoutRelease(native);
      return GRANIT_ERROR_OUT_OF_MEMORY;
    } catch (...) {
      wgpuBindGroupLayoutRelease(native);
      return GRANIT_ERROR_INTERNAL;
    }
    *out_layout = handle;
    return GRANIT_SUCCESS;
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result destroy_bind_group_layout(granit_backend_plugin_instance instance,
                                        granit_backend_plugin_bind_group_layout layout) noexcept {
  if (instance == 0 || layout == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const std::scoped_lock lock{instances_mutex};
  const auto found = instances.find(instance);
  if (found == instances.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  auto& state = *found->second;
  const auto layout_found = state.bind_group_layouts.find(layout);
  if (layout_found == state.bind_group_layouts.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  const auto used_by_group =
      std::any_of(state.bind_groups.begin(), state.bind_groups.end(),
                  [layout](const auto& entry) { return entry.second.layout == layout; });
  const auto used_by_pipeline = std::any_of(
      state.pipeline_layouts.begin(), state.pipeline_layouts.end(), [layout](const auto& entry) {
        return std::find(entry.second.bind_group_layouts.begin(),
                         entry.second.bind_group_layouts.end(),
                         layout) != entry.second.bind_group_layouts.end();
      });
  if (used_by_group || used_by_pipeline)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  wgpuBindGroupLayoutRelease(layout_found->second.bind_group_layout);
  state.bind_group_layouts.erase(layout_found);
  return GRANIT_SUCCESS;
}

granit_result create_bind_group(granit_backend_plugin_instance instance,
                                const granit_backend_plugin_bind_group_desc* desc,
                                granit_backend_plugin_bind_group* out_bind_group) noexcept {
  if (out_bind_group != nullptr)
    *out_bind_group = 0;
  if (instance == 0 || desc == nullptr || out_bind_group == nullptr ||
      desc->struct_size < sizeof(granit_backend_plugin_bind_group_desc) || desc->reserved != 0 ||
      desc->layout == 0 || (desc->entry_count != 0 && desc->entries == nullptr)) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  const std::scoped_lock lock{instances_mutex};
  const auto found = instances.find(instance);
  if (found == instances.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  if (const auto ready = require_ready(*found->second); ready != GRANIT_SUCCESS)
    return ready;
  auto& state = *found->second;
  const auto layout = state.bind_group_layouts.find(desc->layout);
  if (layout == state.bind_group_layouts.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  if (desc->entry_count != layout->second.entries.size())
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    std::vector<WGPUBindGroupEntry> entries(desc->entry_count);
    webgpu_instance::bind_group_record record{nullptr, desc->layout, {}, {}, {}, {}};
    record.entries.assign(desc->entries, desc->entries + desc->entry_count);
    for (std::uint32_t index = 0; index < desc->entry_count; ++index) {
      const auto& source = desc->entries[index];
      if (std::any_of(desc->entries, desc->entries + index,
                      [&](const auto& previous) { return previous.binding == source.binding; }))
        return GRANIT_ERROR_INVALID_ARGUMENT;
      const auto declaration =
          std::find_if(layout->second.entries.begin(), layout->second.entries.end(),
                       [&](const auto& candidate) { return candidate.binding == source.binding; });
      if (declaration == layout->second.entries.end() || declaration->type != source.type)
        return GRANIT_ERROR_INVALID_ARGUMENT;
      auto& entry = entries[index];
      entry = WGPU_BIND_GROUP_ENTRY_INIT;
      entry.binding = source.binding;
      if (source.type == GRANIT_BACKEND_PLUGIN_BINDING_TYPE_UNIFORM_BUFFER ||
          source.type == GRANIT_BACKEND_PLUGIN_BINDING_TYPE_DYNAMIC_UNIFORM_BUFFER ||
          source.type == GRANIT_BACKEND_PLUGIN_BINDING_TYPE_STORAGE_BUFFER) {
        const auto buffer = state.buffers.find(source.buffer);
        if (buffer == state.buffers.end())
          return GRANIT_ERROR_INVALID_HANDLE;
        const auto required_usage = source.type == GRANIT_BACKEND_PLUGIN_BINDING_TYPE_STORAGE_BUFFER
                                        ? GRANIT_BACKEND_PLUGIN_BUFFER_USAGE_STORAGE_BIT
                                        : GRANIT_BACKEND_PLUGIN_BUFFER_USAGE_UNIFORM_BIT;
        if (source.offset >= buffer->second.size || source.size == 0 ||
            source.size > buffer->second.size - source.offset ||
            (buffer->second.usage & required_usage) == 0)
          return GRANIT_ERROR_INVALID_ARGUMENT;
        entry.buffer = buffer->second.buffer;
        entry.offset = source.offset;
        entry.size = source.size;
        record.buffers.push_back(source.buffer);
      } else if (source.type == GRANIT_BACKEND_PLUGIN_BINDING_TYPE_SAMPLED_TEXTURE ||
                 source.type == GRANIT_BACKEND_PLUGIN_BINDING_TYPE_SAMPLED_TEXTURE_CUBE) {
        const auto view = state.texture_views.find(source.texture_view);
        if (view == state.texture_views.end())
          return GRANIT_ERROR_INVALID_HANDLE;
        const auto texture = state.textures.find(view->second.texture);
        if (texture == state.textures.end() ||
            (texture->second.usage & GRANIT_BACKEND_PLUGIN_TEXTURE_USAGE_SAMPLED_BIT) == 0)
          return GRANIT_ERROR_INVALID_ARGUMENT;
        entry.textureView = view->second.view;
        record.texture_views.push_back(source.texture_view);
      } else if (source.type == GRANIT_BACKEND_PLUGIN_BINDING_TYPE_SAMPLER) {
        const auto sampler = state.samplers.find(source.sampler);
        if (sampler == state.samplers.end())
          return GRANIT_ERROR_INVALID_HANDLE;
        entry.sampler = sampler->second;
        record.samplers.push_back(source.sampler);
      } else {
        return GRANIT_ERROR_INVALID_ARGUMENT;
      }
    }
    if (record.buffers.size() + record.texture_views.size() + record.samplers.size() !=
        desc->entry_count) {
      return GRANIT_ERROR_INVALID_HANDLE;
    }
    WGPUBindGroupDescriptor descriptor = WGPU_BIND_GROUP_DESCRIPTOR_INIT;
    descriptor.layout = layout->second.bind_group_layout;
    descriptor.entryCount = entries.size();
    descriptor.entries = entries.data();
    const auto native = wgpuDeviceCreateBindGroup(state.device, &descriptor);
    if (native == nullptr)
      return GRANIT_ERROR_OUT_OF_MEMORY;
    const auto handle = next_handle<granit_backend_plugin_bind_group>(next_bind_group);
    try {
      record.bind_group = native;
      if (!state.bind_groups.emplace(handle, std::move(record)).second) {
        wgpuBindGroupRelease(native);
        return GRANIT_ERROR_INTERNAL;
      }
    } catch (const std::bad_alloc&) {
      wgpuBindGroupRelease(native);
      return GRANIT_ERROR_OUT_OF_MEMORY;
    } catch (...) {
      wgpuBindGroupRelease(native);
      return GRANIT_ERROR_INTERNAL;
    }
    *out_bind_group = handle;
    return GRANIT_SUCCESS;
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result destroy_bind_group(granit_backend_plugin_instance instance,
                                 granit_backend_plugin_bind_group bind_group) noexcept {
  if (instance == 0 || bind_group == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const std::scoped_lock lock{instances_mutex};
  const auto found = instances.find(instance);
  if (found == instances.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  const auto group_found = found->second->bind_groups.find(bind_group);
  if (group_found == found->second->bind_groups.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  wgpuBindGroupRelease(group_found->second.bind_group);
  found->second->bind_groups.erase(group_found);
  return GRANIT_SUCCESS;
}

granit_result create_shader(granit_backend_plugin_instance instance,
                            const granit_backend_plugin_shader_desc* desc,
                            granit_backend_plugin_shader* out_shader) noexcept {
  if (out_shader != nullptr)
    *out_shader = 0;
  if (instance == 0 || desc == nullptr || out_shader == nullptr ||
      desc->struct_size < sizeof(*desc) || desc->wgsl == nullptr || desc->wgsl_length == 0 ||
      desc->entry_point == nullptr || desc->entry_point_length == 0 ||
      (desc->stage != GRANIT_BACKEND_PLUGIN_SHADER_STAGE_VERTEX &&
       desc->stage != GRANIT_BACKEND_PLUGIN_SHADER_STAGE_FRAGMENT &&
       desc->stage != GRANIT_BACKEND_PLUGIN_SHADER_STAGE_COMPUTE))
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const std::scoped_lock lock{instances_mutex};
  const auto found = instances.find(instance);
  if (found == instances.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  if (const auto ready = require_ready(*found->second); ready != GRANIT_SUCCESS)
    return ready;
  auto& state = *found->second;
  WGPUShaderSourceWGSL source = WGPU_SHADER_SOURCE_WGSL_INIT;
  source.code = {desc->wgsl, static_cast<std::size_t>(desc->wgsl_length)};
  WGPUShaderModuleDescriptor descriptor = WGPU_SHADER_MODULE_DESCRIPTOR_INIT;
  descriptor.nextInChain = &source.chain;
  const auto native = wgpuDeviceCreateShaderModule(state.device, &descriptor);
  if (native == nullptr)
    return GRANIT_ERROR_INITIALIZATION_FAILED;
  const auto handle = next_handle<granit_backend_plugin_shader>(next_shader);
  try {
    webgpu_instance::shader_record record{
        native, desc->stage,
        std::string{desc->entry_point, static_cast<std::size_t>(desc->entry_point_length)}};
    if (!state.shaders.emplace(handle, std::move(record)).second) {
      wgpuShaderModuleRelease(native);
      return GRANIT_ERROR_INTERNAL;
    }
  } catch (const std::bad_alloc&) {
    wgpuShaderModuleRelease(native);
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    wgpuShaderModuleRelease(native);
    return GRANIT_ERROR_INTERNAL;
  }
  *out_shader = handle;
  return GRANIT_SUCCESS;
}

granit_result destroy_shader(granit_backend_plugin_instance instance,
                             granit_backend_plugin_shader shader) noexcept {
  if (instance == 0 || shader == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const std::scoped_lock lock{instances_mutex};
  const auto found = instances.find(instance);
  if (found == instances.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  auto& state = *found->second;
  const auto shader_found = state.shaders.find(shader);
  if (shader_found == state.shaders.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  if (std::any_of(state.render_pipelines.begin(), state.render_pipelines.end(),
                  [shader](const auto& entry) {
                    return entry.second.vertex_shader == shader ||
                           entry.second.fragment_shader == shader;
                  }) ||
      std::any_of(state.compute_pipelines.begin(), state.compute_pipelines.end(),
                  [shader](const auto& entry) { return entry.second.shader == shader; }))
    return GRANIT_ERROR_INVALID_ARGUMENT;
  wgpuShaderModuleRelease(shader_found->second.shader);
  state.shaders.erase(shader_found);
  return GRANIT_SUCCESS;
}

granit_result
create_pipeline_layout(granit_backend_plugin_instance instance,
                       const granit_backend_plugin_pipeline_layout_desc* desc,
                       granit_backend_plugin_pipeline_layout* out_pipeline_layout) noexcept {
  if (out_pipeline_layout != nullptr)
    *out_pipeline_layout = 0;
  if (instance == 0 || desc == nullptr || out_pipeline_layout == nullptr ||
      desc->struct_size < sizeof(granit_backend_plugin_pipeline_layout_desc) ||
      desc->reserved != 0 || desc->bind_group_layout_count > 8 ||
      (desc->bind_group_layout_count != 0 && desc->bind_group_layouts == nullptr))
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const std::scoped_lock lock{instances_mutex};
  const auto found = instances.find(instance);
  if (found == instances.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  if (const auto ready = require_ready(*found->second); ready != GRANIT_SUCCESS)
    return ready;
  auto& state = *found->second;
  try {
    std::vector<WGPUBindGroupLayout> native_layouts;
    std::vector<granit_backend_plugin_bind_group_layout> dependencies;
    native_layouts.reserve(desc->bind_group_layout_count);
    dependencies.reserve(desc->bind_group_layout_count);
    for (std::uint32_t index = 0; index < desc->bind_group_layout_count; ++index) {
      const auto handle = desc->bind_group_layouts[index];
      if (handle == 0)
        return GRANIT_ERROR_INVALID_ARGUMENT;
      const auto layout = state.bind_group_layouts.find(handle);
      if (layout == state.bind_group_layouts.end())
        return GRANIT_ERROR_INVALID_HANDLE;
      native_layouts.push_back(layout->second.bind_group_layout);
      dependencies.push_back(handle);
    }
    WGPUPipelineLayoutDescriptor descriptor = WGPU_PIPELINE_LAYOUT_DESCRIPTOR_INIT;
    descriptor.bindGroupLayoutCount = native_layouts.size();
    descriptor.bindGroupLayouts = native_layouts.empty() ? nullptr : native_layouts.data();
    const auto native = wgpuDeviceCreatePipelineLayout(state.device, &descriptor);
    if (native == nullptr)
      return GRANIT_ERROR_OUT_OF_MEMORY;
    const auto handle = next_handle<granit_backend_plugin_pipeline_layout>(next_pipeline_layout);
    try {
      webgpu_instance::pipeline_layout_record record{native, std::move(dependencies)};
      if (!state.pipeline_layouts.emplace(handle, std::move(record)).second) {
        wgpuPipelineLayoutRelease(native);
        return GRANIT_ERROR_INTERNAL;
      }
    } catch (const std::bad_alloc&) {
      wgpuPipelineLayoutRelease(native);
      return GRANIT_ERROR_OUT_OF_MEMORY;
    } catch (...) {
      wgpuPipelineLayoutRelease(native);
      return GRANIT_ERROR_INTERNAL;
    }
    *out_pipeline_layout = handle;
    return GRANIT_SUCCESS;
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result destroy_pipeline_layout(granit_backend_plugin_instance instance,
                                      granit_backend_plugin_pipeline_layout layout) noexcept {
  if (instance == 0 || layout == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const std::scoped_lock lock{instances_mutex};
  const auto found = instances.find(instance);
  if (found == instances.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  auto& state = *found->second;
  const auto layout_found = state.pipeline_layouts.find(layout);
  if (layout_found == state.pipeline_layouts.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  if (std::any_of(state.render_pipelines.begin(), state.render_pipelines.end(),
                  [layout](const auto& entry) { return entry.second.pipeline_layout == layout; }) ||
      std::any_of(state.compute_pipelines.begin(), state.compute_pipelines.end(),
                  [layout](const auto& entry) { return entry.second.pipeline_layout == layout; }))
    return GRANIT_ERROR_INVALID_ARGUMENT;
  wgpuPipelineLayoutRelease(layout_found->second.pipeline_layout);
  state.pipeline_layouts.erase(layout_found);
  return GRANIT_SUCCESS;
}

WGPUVertexFormat to_vertex_format(granit_backend_plugin_vertex_format format) noexcept {
  switch (format) {
  case GRANIT_BACKEND_PLUGIN_VERTEX_FORMAT_FLOAT32:
    return WGPUVertexFormat_Float32;
  case GRANIT_BACKEND_PLUGIN_VERTEX_FORMAT_FLOAT32X2:
    return WGPUVertexFormat_Float32x2;
  case GRANIT_BACKEND_PLUGIN_VERTEX_FORMAT_FLOAT32X3:
    return WGPUVertexFormat_Float32x3;
  case GRANIT_BACKEND_PLUGIN_VERTEX_FORMAT_FLOAT32X4:
    return WGPUVertexFormat_Float32x4;
  case GRANIT_BACKEND_PLUGIN_VERTEX_FORMAT_UINT32:
    return WGPUVertexFormat_Uint32;
  case GRANIT_BACKEND_PLUGIN_VERTEX_FORMAT_UINT32X2:
    return WGPUVertexFormat_Uint32x2;
  case GRANIT_BACKEND_PLUGIN_VERTEX_FORMAT_UINT32X3:
    return WGPUVertexFormat_Uint32x3;
  case GRANIT_BACKEND_PLUGIN_VERTEX_FORMAT_UINT32X4:
    return WGPUVertexFormat_Uint32x4;
  case GRANIT_BACKEND_PLUGIN_VERTEX_FORMAT_SINT32:
    return WGPUVertexFormat_Sint32;
  case GRANIT_BACKEND_PLUGIN_VERTEX_FORMAT_SINT32X2:
    return WGPUVertexFormat_Sint32x2;
  case GRANIT_BACKEND_PLUGIN_VERTEX_FORMAT_SINT32X3:
    return WGPUVertexFormat_Sint32x3;
  case GRANIT_BACKEND_PLUGIN_VERTEX_FORMAT_SINT32X4:
    return WGPUVertexFormat_Sint32x4;
  default:
    return static_cast<WGPUVertexFormat>(0);
  }
}

std::uint32_t vertex_format_size(granit_backend_plugin_vertex_format format) noexcept {
  if (format == GRANIT_BACKEND_PLUGIN_VERTEX_FORMAT_FLOAT32 ||
      format == GRANIT_BACKEND_PLUGIN_VERTEX_FORMAT_UINT32 ||
      format == GRANIT_BACKEND_PLUGIN_VERTEX_FORMAT_SINT32)
    return 4;
  if (format == GRANIT_BACKEND_PLUGIN_VERTEX_FORMAT_FLOAT32X2 ||
      format == GRANIT_BACKEND_PLUGIN_VERTEX_FORMAT_UINT32X2 ||
      format == GRANIT_BACKEND_PLUGIN_VERTEX_FORMAT_SINT32X2)
    return 8;
  if (format == GRANIT_BACKEND_PLUGIN_VERTEX_FORMAT_FLOAT32X3 ||
      format == GRANIT_BACKEND_PLUGIN_VERTEX_FORMAT_UINT32X3 ||
      format == GRANIT_BACKEND_PLUGIN_VERTEX_FORMAT_SINT32X3)
    return 12;
  if (format == GRANIT_BACKEND_PLUGIN_VERTEX_FORMAT_FLOAT32X4 ||
      format == GRANIT_BACKEND_PLUGIN_VERTEX_FORMAT_UINT32X4 ||
      format == GRANIT_BACKEND_PLUGIN_VERTEX_FORMAT_SINT32X4)
    return 16;
  return 0;
}

granit_result
create_render_pipeline(granit_backend_plugin_instance instance,
                       const granit_backend_plugin_render_pipeline_desc* desc,
                       granit_backend_plugin_render_pipeline* out_render_pipeline) noexcept {
  if (out_render_pipeline != nullptr)
    *out_render_pipeline = 0;
  if (instance == 0 || desc == nullptr || out_render_pipeline == nullptr ||
      desc->struct_size < sizeof(*desc) || desc->reserved != 0 || desc->layout == 0 ||
      desc->vertex_shader == 0 || desc->fragment_shader == 0 ||
      (desc->vertex_buffer_layout_count != 0 && desc->vertex_buffer_layouts == nullptr) ||
      (desc->color_format != GRANIT_BACKEND_PLUGIN_TEXTURE_FORMAT_RGBA8_UNORM &&
       desc->color_format != GRANIT_BACKEND_PLUGIN_TEXTURE_FORMAT_BGRA8_UNORM &&
       desc->color_format != GRANIT_BACKEND_PLUGIN_TEXTURE_FORMAT_RGBA16_FLOAT) ||
      (desc->depth_stencil_format != 0 &&
       desc->depth_stencil_format != GRANIT_BACKEND_PLUGIN_TEXTURE_FORMAT_D32_FLOAT) ||
      desc->depth_test_enabled > 1 || desc->depth_write_enabled > 1 ||
      (desc->depth_stencil_format == 0 &&
       (desc->depth_test_enabled != 0 || desc->depth_write_enabled != 0)) ||
      (desc->depth_test_enabled != 0 &&
       to_native_compare_operation(desc->depth_compare) == WGPUCompareFunction_Undefined))
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const std::scoped_lock lock{instances_mutex};
  const auto found = instances.find(instance);
  if (found == instances.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  if (const auto ready = require_ready(*found->second); ready != GRANIT_SUCCESS)
    return ready;
  auto& state = *found->second;
  const auto layout = state.pipeline_layouts.find(desc->layout);
  const auto vertex = state.shaders.find(desc->vertex_shader);
  const auto fragment_shader = state.shaders.find(desc->fragment_shader);
  if (layout == state.pipeline_layouts.end() || vertex == state.shaders.end() ||
      fragment_shader == state.shaders.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  if (vertex->second.stage != GRANIT_BACKEND_PLUGIN_SHADER_STAGE_VERTEX ||
      fragment_shader->second.stage != GRANIT_BACKEND_PLUGIN_SHADER_STAGE_FRAGMENT)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  std::vector<WGPUVertexBufferLayout> vertex_buffers;
  std::vector<WGPUVertexAttribute> vertex_attributes;
  try {
    vertex_buffers.reserve(desc->vertex_buffer_layout_count);
    std::size_t attribute_count = 0;
    for (std::uint32_t binding = 0; binding < desc->vertex_buffer_layout_count; ++binding) {
      const auto count = desc->vertex_buffer_layouts[binding].attribute_count;
      if (count > std::numeric_limits<std::size_t>::max() - attribute_count)
        return GRANIT_ERROR_INVALID_ARGUMENT;
      attribute_count += count;
    }
    // WGPUVertexBufferLayout 保存属性数组指针，后续不得再触发 vector 重新分配。
    vertex_attributes.reserve(attribute_count);
    for (std::uint32_t binding = 0; binding < desc->vertex_buffer_layout_count; ++binding) {
      const auto& source = desc->vertex_buffer_layouts[binding];
      if (source.stride == 0 || source.reserved != 0 || source.attribute_count == 0 ||
          source.attributes == nullptr ||
          (source.step_mode != GRANIT_BACKEND_PLUGIN_VERTEX_STEP_MODE_VERTEX &&
           source.step_mode != GRANIT_BACKEND_PLUGIN_VERTEX_STEP_MODE_INSTANCE))
        return GRANIT_ERROR_INVALID_ARGUMENT;
      const auto first = vertex_attributes.size();
      for (std::uint32_t index = 0; index < source.attribute_count; ++index) {
        const auto& attribute = source.attributes[index];
        const auto format = to_vertex_format(attribute.format);
        const auto size = vertex_format_size(attribute.format);
        if (attribute.reserved != 0 || size == 0 || attribute.offset > source.stride ||
            size > source.stride - attribute.offset)
          return GRANIT_ERROR_INVALID_ARGUMENT;
        if (std::any_of(vertex_attributes.begin(), vertex_attributes.end(),
                        [&attribute](const auto& existing) {
                          return existing.shaderLocation == attribute.location;
                        }))
          return GRANIT_ERROR_INVALID_ARGUMENT;
        WGPUVertexAttribute native_attribute{};
        native_attribute.format = format;
        native_attribute.offset = attribute.offset;
        native_attribute.shaderLocation = attribute.location;
        vertex_attributes.push_back(native_attribute);
      }
      WGPUVertexBufferLayout native_layout{};
      native_layout.arrayStride = source.stride;
      native_layout.stepMode = source.step_mode == GRANIT_BACKEND_PLUGIN_VERTEX_STEP_MODE_VERTEX
                                   ? WGPUVertexStepMode_Vertex
                                   : WGPUVertexStepMode_Instance;
      native_layout.attributeCount = source.attribute_count;
      native_layout.attributes = vertex_attributes.data() + first;
      vertex_buffers.push_back(native_layout);
    }
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
  WGPUColorTargetState target = WGPU_COLOR_TARGET_STATE_INIT;
  target.format = desc->color_format == GRANIT_BACKEND_PLUGIN_TEXTURE_FORMAT_RGBA8_UNORM
                      ? WGPUTextureFormat_RGBA8Unorm
                      : WGPUTextureFormat_BGRA8Unorm;
  target.writeMask = WGPUColorWriteMask_All;
  WGPUFragmentState fragment = WGPU_FRAGMENT_STATE_INIT;
  fragment.module = fragment_shader->second.shader;
  fragment.entryPoint = {fragment_shader->second.entry_point.data(),
                         fragment_shader->second.entry_point.size()};
  fragment.targetCount = 1;
  fragment.targets = &target;
  WGPURenderPipelineDescriptor descriptor = WGPU_RENDER_PIPELINE_DESCRIPTOR_INIT;
  descriptor.layout = layout->second.pipeline_layout;
  descriptor.vertex.module = vertex->second.shader;
  descriptor.vertex.entryPoint = {vertex->second.entry_point.data(),
                                  vertex->second.entry_point.size()};
  descriptor.vertex.bufferCount = vertex_buffers.size();
  descriptor.vertex.buffers = vertex_buffers.data();
  descriptor.primitive.topology = WGPUPrimitiveTopology_TriangleList;
  descriptor.multisample.count = 1;
  descriptor.multisample.mask = UINT32_MAX;
  descriptor.fragment = &fragment;
  WGPUDepthStencilState depth = WGPU_DEPTH_STENCIL_STATE_INIT;
  if (desc->depth_stencil_format != 0) {
    depth.format = WGPUTextureFormat_Depth32Float;
    depth.depthWriteEnabled =
        desc->depth_write_enabled != 0 ? WGPUOptionalBool_True : WGPUOptionalBool_False;
    depth.depthCompare = desc->depth_test_enabled != 0
                             ? to_native_compare_operation(desc->depth_compare)
                             : WGPUCompareFunction_Always;
    depth.depthBias = desc->depth_bias_constant;
    depth.depthBiasSlopeScale = desc->depth_bias_slope_scale;
    depth.depthBiasClamp = desc->depth_bias_clamp;
    descriptor.depthStencil = &depth;
  }
  const auto native = wgpuDeviceCreateRenderPipeline(state.device, &descriptor);
  if (native == nullptr)
    return GRANIT_ERROR_INITIALIZATION_FAILED;
  const auto handle = next_handle<granit_backend_plugin_render_pipeline>(next_render_pipeline);
  try {
    const auto record = webgpu_instance::render_pipeline_record{
        native, desc->layout, desc->vertex_shader, desc->fragment_shader};
    if (!state.render_pipelines.emplace(handle, record).second) {
      wgpuRenderPipelineRelease(native);
      return GRANIT_ERROR_INTERNAL;
    }
  } catch (const std::bad_alloc&) {
    wgpuRenderPipelineRelease(native);
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    wgpuRenderPipelineRelease(native);
    return GRANIT_ERROR_INTERNAL;
  }
  *out_render_pipeline = handle;
  return GRANIT_SUCCESS;
}

granit_result destroy_render_pipeline(granit_backend_plugin_instance instance,
                                      granit_backend_plugin_render_pipeline pipeline) noexcept {
  if (instance == 0 || pipeline == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const std::scoped_lock lock{instances_mutex};
  const auto found = instances.find(instance);
  if (found == instances.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  const auto pipeline_found = found->second->render_pipelines.find(pipeline);
  if (pipeline_found == found->second->render_pipelines.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  wgpuRenderPipelineRelease(pipeline_found->second.render_pipeline);
  found->second->render_pipelines.erase(pipeline_found);
  return GRANIT_SUCCESS;
}

granit_result
create_compute_pipeline(granit_backend_plugin_instance instance,
                        const granit_backend_plugin_compute_pipeline_desc* desc,
                        granit_backend_plugin_compute_pipeline* out_pipeline) noexcept {
  if (out_pipeline != nullptr)
    *out_pipeline = 0;
  if (instance == 0 || desc == nullptr || out_pipeline == nullptr ||
      desc->struct_size < sizeof(granit_backend_plugin_compute_pipeline_desc) ||
      desc->reserved != 0 || desc->layout == 0 || desc->shader == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const std::scoped_lock lock{instances_mutex};
  const auto found = instances.find(instance);
  if (found == instances.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  if (const auto ready = require_ready(*found->second); ready != GRANIT_SUCCESS)
    return ready;
  auto& state = *found->second;
  const auto layout = state.pipeline_layouts.find(desc->layout);
  const auto shader = state.shaders.find(desc->shader);
  if (layout == state.pipeline_layouts.end() || shader == state.shaders.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  if (shader->second.stage != GRANIT_BACKEND_PLUGIN_SHADER_STAGE_COMPUTE)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  WGPUComputePipelineDescriptor descriptor = WGPU_COMPUTE_PIPELINE_DESCRIPTOR_INIT;
  descriptor.layout = layout->second.pipeline_layout;
  descriptor.compute.module = shader->second.shader;
  descriptor.compute.entryPoint = {shader->second.entry_point.data(),
                                   shader->second.entry_point.size()};
  const auto native = wgpuDeviceCreateComputePipeline(state.device, &descriptor);
  if (native == nullptr)
    return GRANIT_ERROR_OUT_OF_MEMORY;
  const auto handle = next_handle<granit_backend_plugin_compute_pipeline>(next_compute_pipeline);
  try {
    const webgpu_instance::compute_pipeline_record record{native, desc->layout, desc->shader};
    if (!state.compute_pipelines.emplace(handle, record).second) {
      wgpuComputePipelineRelease(native);
      return GRANIT_ERROR_INTERNAL;
    }
  } catch (...) {
    wgpuComputePipelineRelease(native);
    return GRANIT_ERROR_OUT_OF_MEMORY;
  }
  *out_pipeline = handle;
  return GRANIT_SUCCESS;
}

granit_result destroy_compute_pipeline(granit_backend_plugin_instance instance,
                                       granit_backend_plugin_compute_pipeline pipeline) noexcept {
  if (instance == 0 || pipeline == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const std::scoped_lock lock{instances_mutex};
  const auto found = instances.find(instance);
  if (found == instances.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  const auto pipeline_found = found->second->compute_pipelines.find(pipeline);
  if (pipeline_found == found->second->compute_pipelines.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  wgpuComputePipelineRelease(pipeline_found->second.compute_pipeline);
  found->second->compute_pipelines.erase(pipeline_found);
  return GRANIT_SUCCESS;
}

granit_result
create_command_recorder(granit_backend_plugin_instance instance,
                        granit_backend_plugin_command_recorder* out_recorder) noexcept {
  if (out_recorder != nullptr)
    *out_recorder = 0;
  if (instance == 0 || out_recorder == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const std::scoped_lock lock{instances_mutex};
  const auto found = instances.find(instance);
  if (found == instances.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  if (const auto ready = require_ready(*found->second); ready != GRANIT_SUCCESS)
    return ready;
  WGPUCommandEncoderDescriptor descriptor = WGPU_COMMAND_ENCODER_DESCRIPTOR_INIT;
  const auto native = wgpuDeviceCreateCommandEncoder(found->second->device, &descriptor);
  if (native == nullptr)
    return GRANIT_ERROR_OUT_OF_MEMORY;
  const auto handle = next_handle<granit_backend_plugin_command_recorder>(next_command_recorder);
  try {
    const auto record = webgpu_instance::command_recorder_record{native, nullptr, nullptr, false,
                                                                 false,  false,   0,       0};
    if (!found->second->command_recorders.emplace(handle, record).second) {
      wgpuCommandEncoderRelease(native);
      return GRANIT_ERROR_INTERNAL;
    }
  } catch (const std::bad_alloc&) {
    wgpuCommandEncoderRelease(native);
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    wgpuCommandEncoderRelease(native);
    return GRANIT_ERROR_INTERNAL;
  }
  *out_recorder = handle;
  return GRANIT_SUCCESS;
}

granit_result destroy_command_recorder(granit_backend_plugin_instance instance,
                                       granit_backend_plugin_command_recorder recorder) noexcept {
  if (instance == 0 || recorder == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const std::scoped_lock lock{instances_mutex};
  const auto found = instances.find(instance);
  if (found == instances.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  const auto recorder_found = found->second->command_recorders.find(recorder);
  if (recorder_found == found->second->command_recorders.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  if (recorder_found->second.pass != nullptr)
    wgpuRenderPassEncoderRelease(recorder_found->second.pass);
  if (recorder_found->second.compute_pass != nullptr)
    wgpuComputePassEncoderRelease(recorder_found->second.compute_pass);
  wgpuCommandEncoderRelease(recorder_found->second.encoder);
  found->second->command_recorders.erase(recorder_found);
  return GRANIT_SUCCESS;
}

granit_result recorder_copy_buffer_to_texture(granit_backend_plugin_instance instance,
                                              granit_backend_plugin_command_recorder recorder,
                                              granit_backend_plugin_buffer buffer,
                                              granit_backend_plugin_texture texture,
                                              std::uint32_t width, std::uint32_t height,
                                              std::uint32_t bytes_per_row) noexcept {
  if (instance == 0 || recorder == 0 || buffer == 0 || texture == 0 || width == 0 || height == 0 ||
      bytes_per_row < static_cast<std::uint64_t>(width) * 4 || bytes_per_row % 256 != 0) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  const std::scoped_lock lock{instances_mutex};
  const auto found = instances.find(instance);
  if (found == instances.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  if (const auto ready = require_ready(*found->second); ready != GRANIT_SUCCESS)
    return ready;
  auto& state = *found->second;
  const auto recorder_found = state.command_recorders.find(recorder);
  const auto buffer_found = state.buffers.find(buffer);
  const auto texture_found = state.textures.find(texture);
  if (recorder_found == state.command_recorders.end() || buffer_found == state.buffers.end() ||
      texture_found == state.textures.end()) {
    return GRANIT_ERROR_INVALID_HANDLE;
  }
  if (recorder_found->second.finished || recorder_found->second.pass != nullptr ||
      recorder_found->second.compute_pass != nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const auto required_size = static_cast<std::uint64_t>(bytes_per_row) * (height - 1) +
                             static_cast<std::uint64_t>(width) * 4;
  if ((buffer_found->second.usage & GRANIT_BACKEND_PLUGIN_BUFFER_USAGE_COPY_SRC_BIT) == 0 ||
      (texture_found->second.usage & GRANIT_BACKEND_PLUGIN_TEXTURE_USAGE_COPY_DST_BIT) == 0 ||
      width > texture_found->second.width || height > texture_found->second.height ||
      required_size > buffer_found->second.size) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  WGPUTexelCopyBufferInfo source = WGPU_TEXEL_COPY_BUFFER_INFO_INIT;
  source.buffer = buffer_found->second.buffer;
  source.layout.bytesPerRow = bytes_per_row;
  source.layout.rowsPerImage = height;
  WGPUTexelCopyTextureInfo destination = WGPU_TEXEL_COPY_TEXTURE_INFO_INIT;
  destination.texture = texture_found->second.texture;
  destination.aspect = WGPUTextureAspect_All;
  const WGPUExtent3D extent{width, height, 1};
  wgpuCommandEncoderCopyBufferToTexture(recorder_found->second.encoder, &source, &destination,
                                        &extent);
  return GRANIT_SUCCESS;
}

granit_result recorder_begin_rendering(
    granit_backend_plugin_instance instance, granit_backend_plugin_command_recorder recorder,
    granit_backend_plugin_texture_view target, granit_backend_plugin_load_operation load_operation,
    granit_backend_plugin_store_operation store_operation, float clear_r, float clear_g,
    float clear_b, float clear_a, granit_backend_plugin_texture_view depth_target,
    granit_backend_plugin_load_operation depth_load_operation,
    granit_backend_plugin_store_operation depth_store_operation, float clear_depth) noexcept {
  if (instance == 0 || recorder == 0 || target == 0 ||
      (load_operation != GRANIT_BACKEND_PLUGIN_LOAD_OPERATION_LOAD &&
       load_operation != GRANIT_BACKEND_PLUGIN_LOAD_OPERATION_CLEAR) ||
      (store_operation != GRANIT_BACKEND_PLUGIN_STORE_OPERATION_STORE &&
       store_operation != GRANIT_BACKEND_PLUGIN_STORE_OPERATION_DISCARD) ||
      (depth_target != 0 &&
       ((depth_load_operation != GRANIT_BACKEND_PLUGIN_LOAD_OPERATION_LOAD &&
         depth_load_operation != GRANIT_BACKEND_PLUGIN_LOAD_OPERATION_CLEAR) ||
        (depth_store_operation != GRANIT_BACKEND_PLUGIN_STORE_OPERATION_STORE &&
         depth_store_operation != GRANIT_BACKEND_PLUGIN_STORE_OPERATION_DISCARD) ||
        !std::isfinite(clear_depth) || clear_depth < 0.0F || clear_depth > 1.0F)))
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const std::scoped_lock lock{instances_mutex};
  const auto found = instances.find(instance);
  if (found == instances.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  if (const auto ready = require_ready(*found->second); ready != GRANIT_SUCCESS)
    return ready;
  auto& state = *found->second;
  const auto command = state.command_recorders.find(recorder);
  const auto view = state.texture_views.find(target);
  const auto depth_view = state.texture_views.find(depth_target);
  if (command == state.command_recorders.end() || view == state.texture_views.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  const auto texture = state.textures.find(view->second.texture);
  if (command->second.finished || command->second.pass != nullptr ||
      command->second.compute_pass != nullptr || texture == state.textures.end() ||
      (texture->second.usage & GRANIT_BACKEND_PLUGIN_TEXTURE_USAGE_RENDER_ATTACHMENT_BIT) == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  WGPURenderPassDepthStencilAttachment depth_attachment =
      WGPU_RENDER_PASS_DEPTH_STENCIL_ATTACHMENT_INIT;
  if (depth_target != 0) {
    if (depth_view == state.texture_views.end())
      return GRANIT_ERROR_INVALID_HANDLE;
    const auto depth_texture = state.textures.find(depth_view->second.texture);
    if (depth_texture == state.textures.end() ||
        depth_texture->second.format != GRANIT_BACKEND_PLUGIN_TEXTURE_FORMAT_D32_FLOAT ||
        (depth_texture->second.usage & GRANIT_BACKEND_PLUGIN_TEXTURE_USAGE_RENDER_ATTACHMENT_BIT) ==
            0) {
      return GRANIT_ERROR_INVALID_ARGUMENT;
    }
    depth_attachment.view = depth_view->second.view;
    depth_attachment.depthLoadOp = depth_load_operation == GRANIT_BACKEND_PLUGIN_LOAD_OPERATION_LOAD
                                       ? WGPULoadOp_Load
                                       : WGPULoadOp_Clear;
    depth_attachment.depthStoreOp =
        depth_store_operation == GRANIT_BACKEND_PLUGIN_STORE_OPERATION_STORE ? WGPUStoreOp_Store
                                                                             : WGPUStoreOp_Discard;
    depth_attachment.depthClearValue = clear_depth;
    depth_attachment.depthReadOnly = false;
    depth_attachment.stencilLoadOp = WGPULoadOp_Undefined;
    depth_attachment.stencilStoreOp = WGPUStoreOp_Undefined;
    depth_attachment.stencilReadOnly = true;
  }
  WGPURenderPassColorAttachment color = WGPU_RENDER_PASS_COLOR_ATTACHMENT_INIT;
  color.view = view->second.view;
  color.loadOp = load_operation == GRANIT_BACKEND_PLUGIN_LOAD_OPERATION_LOAD ? WGPULoadOp_Load
                                                                             : WGPULoadOp_Clear;
  color.storeOp = store_operation == GRANIT_BACKEND_PLUGIN_STORE_OPERATION_STORE
                      ? WGPUStoreOp_Store
                      : WGPUStoreOp_Discard;
  color.clearValue = {clear_r, clear_g, clear_b, clear_a};
  WGPURenderPassDescriptor descriptor = WGPU_RENDER_PASS_DESCRIPTOR_INIT;
  descriptor.colorAttachmentCount = 1;
  descriptor.colorAttachments = &color;
  descriptor.depthStencilAttachment = depth_target == 0 ? nullptr : &depth_attachment;
  command->second.pass = wgpuCommandEncoderBeginRenderPass(command->second.encoder, &descriptor);
  if (command->second.pass == nullptr)
    return GRANIT_ERROR_INITIALIZATION_FAILED;
  command->second.pipeline_bound = false;
  command->second.index_available = 0;
  command->second.index_element_size = 0;
  return GRANIT_SUCCESS;
}

granit_result recorder_bind_pipeline(granit_backend_plugin_instance instance,
                                     granit_backend_plugin_command_recorder recorder,
                                     granit_backend_plugin_render_pipeline pipeline) noexcept {
  if (instance == 0 || recorder == 0 || pipeline == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const std::scoped_lock lock{instances_mutex};
  const auto found = instances.find(instance);
  if (found == instances.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  auto& state = *found->second;
  const auto command = state.command_recorders.find(recorder);
  const auto native = state.render_pipelines.find(pipeline);
  if (command == state.command_recorders.end() || native == state.render_pipelines.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  if (command->second.pass == nullptr || command->second.finished)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  wgpuRenderPassEncoderSetPipeline(command->second.pass, native->second.render_pipeline);
  command->second.pipeline_bound = true;
  return GRANIT_SUCCESS;
}

granit_result recorder_bind_graphics_groups(
    granit_backend_plugin_instance instance, granit_backend_plugin_command_recorder recorder,
    granit_backend_plugin_pipeline_layout pipeline_layout, std::uint32_t first_group,
    const granit_backend_plugin_bind_group* groups, std::uint32_t group_count,
    const std::uint32_t* dynamic_offsets, std::uint32_t dynamic_offset_count) noexcept {
  if (instance == 0 || recorder == 0 || pipeline_layout == 0 || group_count == 0 ||
      groups == nullptr || first_group > UINT32_MAX - group_count ||
      (dynamic_offset_count != 0 && dynamic_offsets == nullptr))
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const std::scoped_lock lock{instances_mutex};
  const auto found = instances.find(instance);
  if (found == instances.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  auto& state = *found->second;
  const auto command = state.command_recorders.find(recorder);
  const auto layout = state.pipeline_layouts.find(pipeline_layout);
  if (command == state.command_recorders.end() || layout == state.pipeline_layouts.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  if (command->second.pass == nullptr || command->second.finished ||
      first_group > layout->second.bind_group_layouts.size() ||
      group_count > layout->second.bind_group_layouts.size() - first_group)
    return GRANIT_ERROR_INVALID_ARGUMENT;

  std::uint32_t offset_index = 0;
  for (std::uint32_t group_index = 0; group_index < group_count; ++group_index) {
    const auto group = state.bind_groups.find(groups[group_index]);
    if (group == state.bind_groups.end())
      return GRANIT_ERROR_INVALID_HANDLE;
    if (group->second.layout != layout->second.bind_group_layouts[first_group + group_index])
      return GRANIT_ERROR_INVALID_ARGUMENT;
    const auto declarations = state.bind_group_layouts.find(group->second.layout);
    if (declarations == state.bind_group_layouts.end())
      return GRANIT_ERROR_INVALID_HANDLE;
    for (const auto& declaration : declarations->second.entries) {
      if (declaration.type != GRANIT_BACKEND_PLUGIN_BINDING_TYPE_DYNAMIC_UNIFORM_BUFFER)
        continue;
      if (offset_index >= dynamic_offset_count)
        return GRANIT_ERROR_INVALID_ARGUMENT;
      const auto write =
          std::find_if(group->second.entries.begin(), group->second.entries.end(),
                       [&](const auto& entry) { return entry.binding == declaration.binding; });
      if (write == group->second.entries.end())
        return GRANIT_ERROR_INVALID_ARGUMENT;
      const auto buffer = state.buffers.find(write->buffer);
      const auto dynamic_offset = static_cast<std::uint64_t>(dynamic_offsets[offset_index++]);
      if (buffer == state.buffers.end())
        return GRANIT_ERROR_INVALID_HANDLE;
      if ((state.capabilities.uniform_buffer_offset_alignment != 0 &&
           dynamic_offset % state.capabilities.uniform_buffer_offset_alignment != 0) ||
          write->offset > buffer->second.size ||
          dynamic_offset > buffer->second.size - write->offset ||
          write->size > buffer->second.size - write->offset - dynamic_offset)
        return GRANIT_ERROR_INVALID_ARGUMENT;
    }
  }
  if (offset_index != dynamic_offset_count)
    return GRANIT_ERROR_INVALID_ARGUMENT;

  offset_index = 0;
  for (std::uint32_t group_index = 0; group_index < group_count; ++group_index) {
    const auto& group = state.bind_groups.find(groups[group_index])->second;
    const auto& declarations = state.bind_group_layouts.find(group.layout)->second.entries;
    const auto count = static_cast<std::uint32_t>(
        std::count_if(declarations.begin(), declarations.end(), [](const auto& declaration) {
          return declaration.type == GRANIT_BACKEND_PLUGIN_BINDING_TYPE_DYNAMIC_UNIFORM_BUFFER;
        }));
    wgpuRenderPassEncoderSetBindGroup(command->second.pass, first_group + group_index,
                                      group.bind_group, count,
                                      count == 0 ? nullptr : dynamic_offsets + offset_index);
    offset_index += count;
  }
  return GRANIT_SUCCESS;
}

granit_result
recorder_bind_vertex_buffers(granit_backend_plugin_instance instance,
                             granit_backend_plugin_command_recorder recorder, std::uint32_t first,
                             const granit_backend_plugin_vertex_buffer_binding* bindings,
                             std::uint32_t count) noexcept {
  if (instance == 0 || recorder == 0 || count == 0 || bindings == nullptr ||
      first > UINT32_MAX - count)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const std::scoped_lock lock{instances_mutex};
  const auto found = instances.find(instance);
  if (found == instances.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  auto& state = *found->second;
  const auto command = state.command_recorders.find(recorder);
  if (command == state.command_recorders.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  if (command->second.pass == nullptr || command->second.finished)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  for (std::uint32_t index = 0; index < count; ++index) {
    const auto buffer = state.buffers.find(bindings[index].buffer);
    if (buffer == state.buffers.end())
      return GRANIT_ERROR_INVALID_HANDLE;
    if ((buffer->second.usage & GRANIT_BACKEND_PLUGIN_BUFFER_USAGE_VERTEX_BIT) == 0 ||
        bindings[index].offset >= buffer->second.size)
      return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  for (std::uint32_t index = 0; index < count; ++index) {
    const auto& binding = bindings[index];
    const auto& buffer = state.buffers.find(binding.buffer)->second;
    wgpuRenderPassEncoderSetVertexBuffer(command->second.pass, first + index, buffer.buffer,
                                         binding.offset, buffer.size - binding.offset);
  }
  return GRANIT_SUCCESS;
}

granit_result recorder_bind_index_buffer(granit_backend_plugin_instance instance,
                                         granit_backend_plugin_command_recorder recorder,
                                         granit_backend_plugin_buffer buffer, std::uint64_t offset,
                                         granit_backend_plugin_index_format format) noexcept {
  if (instance == 0 || recorder == 0 || buffer == 0 ||
      (format != GRANIT_BACKEND_PLUGIN_INDEX_FORMAT_UINT16 &&
       format != GRANIT_BACKEND_PLUGIN_INDEX_FORMAT_UINT32))
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const std::scoped_lock lock{instances_mutex};
  const auto found = instances.find(instance);
  if (found == instances.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  auto& state = *found->second;
  const auto command = state.command_recorders.find(recorder);
  const auto native = state.buffers.find(buffer);
  if (command == state.command_recorders.end() || native == state.buffers.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  const auto element_size = format == GRANIT_BACKEND_PLUGIN_INDEX_FORMAT_UINT16 ? 2U : 4U;
  if (command->second.pass == nullptr || command->second.finished ||
      (native->second.usage & GRANIT_BACKEND_PLUGIN_BUFFER_USAGE_INDEX_BIT) == 0 ||
      offset >= native->second.size || offset % element_size != 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  wgpuRenderPassEncoderSetIndexBuffer(command->second.pass, native->second.buffer,
                                      format == GRANIT_BACKEND_PLUGIN_INDEX_FORMAT_UINT16
                                          ? WGPUIndexFormat_Uint16
                                          : WGPUIndexFormat_Uint32,
                                      offset, native->second.size - offset);
  command->second.index_available = native->second.size - offset;
  command->second.index_element_size = element_size;
  return GRANIT_SUCCESS;
}

granit_result recorder_set_viewports(granit_backend_plugin_instance instance,
                                     granit_backend_plugin_command_recorder recorder,
                                     std::uint32_t first,
                                     const granit_backend_plugin_viewport* viewports,
                                     std::uint32_t count) noexcept {
  if (instance == 0 || recorder == 0 || first != 0 || viewports == nullptr || count != 1)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const auto& viewport = viewports[0];
  if (viewport.width <= 0.0F || viewport.height <= 0.0F || viewport.min_depth < 0.0F ||
      viewport.max_depth > 1.0F || viewport.min_depth > viewport.max_depth)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const std::scoped_lock lock{instances_mutex};
  const auto found = instances.find(instance);
  if (found == instances.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  const auto command = found->second->command_recorders.find(recorder);
  if (command == found->second->command_recorders.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  if (command->second.pass == nullptr || command->second.finished)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  wgpuRenderPassEncoderSetViewport(command->second.pass, viewport.x, viewport.y, viewport.width,
                                   viewport.height, viewport.min_depth, viewport.max_depth);
  return GRANIT_SUCCESS;
}

granit_result recorder_set_scissors(granit_backend_plugin_instance instance,
                                    granit_backend_plugin_command_recorder recorder,
                                    std::uint32_t first,
                                    const granit_backend_plugin_scissor* scissors,
                                    std::uint32_t count) noexcept {
  if (instance == 0 || recorder == 0 || first != 0 || scissors == nullptr || count != 1 ||
      scissors[0].width == 0 || scissors[0].height == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const std::scoped_lock lock{instances_mutex};
  const auto found = instances.find(instance);
  if (found == instances.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  const auto command = found->second->command_recorders.find(recorder);
  if (command == found->second->command_recorders.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  if (command->second.pass == nullptr || command->second.finished)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const auto& scissor = scissors[0];
  wgpuRenderPassEncoderSetScissorRect(command->second.pass, scissor.x, scissor.y, scissor.width,
                                      scissor.height);
  return GRANIT_SUCCESS;
}

granit_result recorder_draw_vertices(granit_backend_plugin_instance instance,
                                     granit_backend_plugin_command_recorder recorder,
                                     std::uint32_t vertex_count, std::uint32_t instance_count,
                                     std::uint32_t first_vertex,
                                     std::uint32_t first_instance) noexcept {
  if (instance == 0 || recorder == 0 || vertex_count == 0 || instance_count == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const std::scoped_lock lock{instances_mutex};
  const auto found = instances.find(instance);
  if (found == instances.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  const auto command = found->second->command_recorders.find(recorder);
  if (command == found->second->command_recorders.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  if (command->second.pass == nullptr || !command->second.pipeline_bound ||
      command->second.finished)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  wgpuRenderPassEncoderDraw(command->second.pass, vertex_count, instance_count, first_vertex,
                            first_instance);
  return GRANIT_SUCCESS;
}

granit_result recorder_draw_indices(granit_backend_plugin_instance instance,
                                    granit_backend_plugin_command_recorder recorder,
                                    std::uint32_t index_count, std::uint32_t instance_count,
                                    std::uint32_t first_index, std::int32_t vertex_offset,
                                    std::uint32_t first_instance) noexcept {
  if (instance == 0 || recorder == 0 || index_count == 0 || instance_count == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const std::scoped_lock lock{instances_mutex};
  const auto found = instances.find(instance);
  if (found == instances.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  const auto command = found->second->command_recorders.find(recorder);
  if (command == found->second->command_recorders.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  const auto first_byte =
      static_cast<std::uint64_t>(first_index) * command->second.index_element_size;
  const auto draw_size =
      static_cast<std::uint64_t>(index_count) * command->second.index_element_size;
  if (command->second.pass == nullptr || !command->second.pipeline_bound ||
      command->second.finished || command->second.index_element_size == 0 ||
      first_byte > command->second.index_available ||
      draw_size > command->second.index_available - first_byte)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  wgpuRenderPassEncoderDrawIndexed(command->second.pass, index_count, instance_count, first_index,
                                   vertex_offset, first_instance);
  return GRANIT_SUCCESS;
}

granit_result recorder_end_rendering(granit_backend_plugin_instance instance,
                                     granit_backend_plugin_command_recorder recorder) noexcept {
  if (instance == 0 || recorder == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const std::scoped_lock lock{instances_mutex};
  const auto found = instances.find(instance);
  if (found == instances.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  const auto command = found->second->command_recorders.find(recorder);
  if (command == found->second->command_recorders.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  if (command->second.pass == nullptr || command->second.finished)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  wgpuRenderPassEncoderEnd(command->second.pass);
  wgpuRenderPassEncoderRelease(command->second.pass);
  command->second.pass = nullptr;
  command->second.pipeline_bound = false;
  command->second.index_available = 0;
  command->second.index_element_size = 0;
  return GRANIT_SUCCESS;
}

granit_result recorder_begin_compute(granit_backend_plugin_instance instance,
                                     granit_backend_plugin_command_recorder recorder) noexcept {
  if (instance == 0 || recorder == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const std::scoped_lock lock{instances_mutex};
  const auto found = instances.find(instance);
  if (found == instances.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  const auto command = found->second->command_recorders.find(recorder);
  if (command == found->second->command_recorders.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  if (command->second.finished || command->second.pass != nullptr ||
      command->second.compute_pass != nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  WGPUComputePassDescriptor descriptor = WGPU_COMPUTE_PASS_DESCRIPTOR_INIT;
  command->second.compute_pass =
      wgpuCommandEncoderBeginComputePass(command->second.encoder, &descriptor);
  if (command->second.compute_pass == nullptr)
    return GRANIT_ERROR_INITIALIZATION_FAILED;
  command->second.compute_pipeline_bound = false;
  return GRANIT_SUCCESS;
}

granit_result
recorder_bind_compute_pipeline(granit_backend_plugin_instance instance,
                               granit_backend_plugin_command_recorder recorder,
                               granit_backend_plugin_compute_pipeline pipeline) noexcept {
  if (instance == 0 || recorder == 0 || pipeline == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const std::scoped_lock lock{instances_mutex};
  const auto found = instances.find(instance);
  if (found == instances.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  auto& state = *found->second;
  const auto command = state.command_recorders.find(recorder);
  const auto native = state.compute_pipelines.find(pipeline);
  if (command == state.command_recorders.end() || native == state.compute_pipelines.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  if (command->second.finished || command->second.compute_pass == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  wgpuComputePassEncoderSetPipeline(command->second.compute_pass, native->second.compute_pipeline);
  command->second.compute_pipeline_bound = true;
  return GRANIT_SUCCESS;
}

granit_result recorder_bind_compute_groups(
    granit_backend_plugin_instance instance, granit_backend_plugin_command_recorder recorder,
    granit_backend_plugin_pipeline_layout pipeline_layout, std::uint32_t first_group,
    const granit_backend_plugin_bind_group* groups, std::uint32_t group_count,
    const std::uint32_t* dynamic_offsets, std::uint32_t dynamic_offset_count) noexcept {
  if (instance == 0 || recorder == 0 || pipeline_layout == 0 || groups == nullptr ||
      group_count == 0 || (dynamic_offset_count != 0 && dynamic_offsets == nullptr))
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const std::scoped_lock lock{instances_mutex};
  const auto found = instances.find(instance);
  if (found == instances.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  auto& state = *found->second;
  const auto command = state.command_recorders.find(recorder);
  const auto layout = state.pipeline_layouts.find(pipeline_layout);
  if (command == state.command_recorders.end() || layout == state.pipeline_layouts.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  if (command->second.finished || command->second.compute_pass == nullptr ||
      first_group > layout->second.bind_group_layouts.size() ||
      group_count > layout->second.bind_group_layouts.size() - first_group)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  std::uint32_t offset_index{};
  for (std::uint32_t index = 0; index < group_count; ++index) {
    const auto group = state.bind_groups.find(groups[index]);
    if (group == state.bind_groups.end())
      return GRANIT_ERROR_INVALID_HANDLE;
    if (group->second.layout != layout->second.bind_group_layouts[first_group + index])
      return GRANIT_ERROR_INVALID_ARGUMENT;
    const auto& declarations = state.bind_group_layouts.find(group->second.layout)->second.entries;
    const auto group_offset_begin = offset_index;
    for (const auto& declaration : declarations) {
      if (declaration.type != GRANIT_BACKEND_PLUGIN_BINDING_TYPE_DYNAMIC_UNIFORM_BUFFER)
        continue;
      if (offset_index >= dynamic_offset_count)
        return GRANIT_ERROR_INVALID_ARGUMENT;
      const auto write =
          std::find_if(group->second.entries.begin(), group->second.entries.end(),
                       [&](const auto& entry) { return entry.binding == declaration.binding; });
      if (write == group->second.entries.end())
        return GRANIT_ERROR_INVALID_ARGUMENT;
      const auto buffer = state.buffers.find(write->buffer);
      const auto dynamic_offset = static_cast<std::uint64_t>(dynamic_offsets[offset_index++]);
      if (buffer == state.buffers.end())
        return GRANIT_ERROR_INVALID_HANDLE;
      if ((state.capabilities.uniform_buffer_offset_alignment != 0 &&
           dynamic_offset % state.capabilities.uniform_buffer_offset_alignment != 0) ||
          write->offset > buffer->second.size ||
          dynamic_offset > buffer->second.size - write->offset ||
          write->size > buffer->second.size - write->offset - dynamic_offset)
        return GRANIT_ERROR_INVALID_ARGUMENT;
    }
    const auto count = offset_index - group_offset_begin;
    wgpuComputePassEncoderSetBindGroup(command->second.compute_pass, first_group + index,
                                       group->second.bind_group, count,
                                       count == 0 ? nullptr : dynamic_offsets + group_offset_begin);
  }
  return offset_index == dynamic_offset_count ? GRANIT_SUCCESS : GRANIT_ERROR_INVALID_ARGUMENT;
}

granit_result recorder_dispatch(granit_backend_plugin_instance instance,
                                granit_backend_plugin_command_recorder recorder, std::uint32_t x,
                                std::uint32_t y, std::uint32_t z) noexcept {
  if (instance == 0 || recorder == 0 || x == 0 || y == 0 || z == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const std::scoped_lock lock{instances_mutex};
  const auto found = instances.find(instance);
  if (found == instances.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  const auto command = found->second->command_recorders.find(recorder);
  if (command == found->second->command_recorders.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  if (command->second.finished || command->second.compute_pass == nullptr ||
      !command->second.compute_pipeline_bound)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  wgpuComputePassEncoderDispatchWorkgroups(command->second.compute_pass, x, y, z);
  return GRANIT_SUCCESS;
}

granit_result recorder_end_compute(granit_backend_plugin_instance instance,
                                   granit_backend_plugin_command_recorder recorder) noexcept {
  if (instance == 0 || recorder == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const std::scoped_lock lock{instances_mutex};
  const auto found = instances.find(instance);
  if (found == instances.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  const auto command = found->second->command_recorders.find(recorder);
  if (command == found->second->command_recorders.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  if (command->second.finished || command->second.compute_pass == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  wgpuComputePassEncoderEnd(command->second.compute_pass);
  wgpuComputePassEncoderRelease(command->second.compute_pass);
  command->second.compute_pass = nullptr;
  command->second.compute_pipeline_bound = false;
  return GRANIT_SUCCESS;
}

granit_result recorder_copy_texture_to_buffer(granit_backend_plugin_instance instance,
                                              granit_backend_plugin_command_recorder recorder,
                                              granit_backend_plugin_texture texture,
                                              granit_backend_plugin_buffer buffer,
                                              std::uint32_t width, std::uint32_t height,
                                              std::uint32_t bytes_per_row) noexcept {
  if (instance == 0 || recorder == 0 || texture == 0 || buffer == 0 || width == 0 || height == 0 ||
      bytes_per_row < static_cast<std::uint64_t>(width) * 4 || bytes_per_row % 256 != 0) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  const std::scoped_lock lock{instances_mutex};
  const auto found = instances.find(instance);
  if (found == instances.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  if (const auto ready = require_ready(*found->second); ready != GRANIT_SUCCESS)
    return ready;
  auto& state = *found->second;
  const auto recorder_found = state.command_recorders.find(recorder);
  const auto texture_found = state.textures.find(texture);
  const auto buffer_found = state.buffers.find(buffer);
  if (recorder_found == state.command_recorders.end() || texture_found == state.textures.end() ||
      buffer_found == state.buffers.end()) {
    return GRANIT_ERROR_INVALID_HANDLE;
  }
  if (recorder_found->second.finished || recorder_found->second.pass != nullptr ||
      recorder_found->second.compute_pass != nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const auto required_size = static_cast<std::uint64_t>(bytes_per_row) * (height - 1) +
                             static_cast<std::uint64_t>(width) * 4;
  if ((texture_found->second.usage & GRANIT_BACKEND_PLUGIN_TEXTURE_USAGE_COPY_SRC_BIT) == 0 ||
      (buffer_found->second.usage & GRANIT_BACKEND_PLUGIN_BUFFER_USAGE_COPY_DST_BIT) == 0 ||
      width > texture_found->second.width || height > texture_found->second.height ||
      required_size > buffer_found->second.size) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  WGPUTexelCopyTextureInfo source = WGPU_TEXEL_COPY_TEXTURE_INFO_INIT;
  source.texture = texture_found->second.texture;
  source.aspect = WGPUTextureAspect_All;
  WGPUTexelCopyBufferInfo destination = WGPU_TEXEL_COPY_BUFFER_INFO_INIT;
  destination.buffer = buffer_found->second.buffer;
  destination.layout.bytesPerRow = bytes_per_row;
  destination.layout.rowsPerImage = height;
  const WGPUExtent3D extent{width, height, 1};
  wgpuCommandEncoderCopyTextureToBuffer(recorder_found->second.encoder, &source, &destination,
                                        &extent);
  return GRANIT_SUCCESS;
}

granit_result
finish_command_recorder(granit_backend_plugin_instance instance,
                        granit_backend_plugin_command_recorder recorder,
                        granit_backend_plugin_command_buffer* out_command_buffer) noexcept {
  if (out_command_buffer != nullptr)
    *out_command_buffer = 0;
  if (instance == 0 || recorder == 0 || out_command_buffer == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const std::scoped_lock lock{instances_mutex};
  const auto found = instances.find(instance);
  if (found == instances.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  if (const auto ready = require_ready(*found->second); ready != GRANIT_SUCCESS)
    return ready;
  auto& state = *found->second;
  const auto recorder_found = state.command_recorders.find(recorder);
  if (recorder_found == state.command_recorders.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  if (recorder_found->second.finished || recorder_found->second.pass != nullptr ||
      recorder_found->second.compute_pass != nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  WGPUCommandBufferDescriptor descriptor = WGPU_COMMAND_BUFFER_DESCRIPTOR_INIT;
  const auto native = wgpuCommandEncoderFinish(recorder_found->second.encoder, &descriptor);
  if (native == nullptr)
    return GRANIT_ERROR_INITIALIZATION_FAILED;
  const auto handle = next_handle<granit_backend_plugin_command_buffer>(next_command_buffer);
  try {
    if (!state.command_buffers.emplace(handle, native).second) {
      wgpuCommandBufferRelease(native);
      return GRANIT_ERROR_INTERNAL;
    }
  } catch (const std::bad_alloc&) {
    wgpuCommandBufferRelease(native);
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    wgpuCommandBufferRelease(native);
    return GRANIT_ERROR_INTERNAL;
  }
  recorder_found->second.finished = true;
  *out_command_buffer = handle;
  return GRANIT_SUCCESS;
}

granit_result destroy_command_buffer(granit_backend_plugin_instance instance,
                                     granit_backend_plugin_command_buffer command_buffer) noexcept {
  if (instance == 0 || command_buffer == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const std::scoped_lock lock{instances_mutex};
  const auto found = instances.find(instance);
  if (found == instances.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  const auto buffer_found = found->second->command_buffers.find(command_buffer);
  if (buffer_found == found->second->command_buffers.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  wgpuCommandBufferRelease(buffer_found->second);
  found->second->command_buffers.erase(buffer_found);
  return GRANIT_SUCCESS;
}

granit_result submit_command_buffer(granit_backend_plugin_instance instance,
                                    granit_backend_plugin_command_buffer command_buffer) noexcept {
  if (instance == 0 || command_buffer == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const std::scoped_lock lock{instances_mutex};
  const auto found = instances.find(instance);
  if (found == instances.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  if (const auto ready = require_ready(*found->second); ready != GRANIT_SUCCESS)
    return ready;
  const auto buffer_found = found->second->command_buffers.find(command_buffer);
  if (buffer_found == found->second->command_buffers.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  const WGPUCommandBuffer native[]{buffer_found->second};
  wgpuQueueSubmit(found->second->queue, 1, native);
  wgpuCommandBufferRelease(buffer_found->second);
  found->second->command_buffers.erase(buffer_found);
  return GRANIT_SUCCESS;
}

template <typename NativeDesc>
granit_result create_native_surface(granit_backend_plugin_instance instance, NativeDesc source,
                                    granit_backend_plugin_surface* surface) noexcept {
  const std::scoped_lock lock{instances_mutex};
  const auto found = instances.find(instance);
  if (found == instances.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  if (const auto ready = require_ready(*found->second); ready != GRANIT_SUCCESS)
    return ready;
  WGPUSurfaceDescriptor native_desc{};
  native_desc.nextInChain = &source.chain;
  const auto native_surface = wgpuInstanceCreateSurface(found->second->instance, &native_desc);
  if (native_surface == nullptr)
    return GRANIT_ERROR_INITIALIZATION_FAILED;
  const auto handle = next_handle<granit_backend_plugin_surface>(next_surface);
  try {
    found->second->surfaces.emplace(handle, webgpu_instance::surface_record{native_surface, {}});
  } catch (const std::bad_alloc&) {
    wgpuSurfaceRelease(native_surface);
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    wgpuSurfaceRelease(native_surface);
    return GRANIT_ERROR_INTERNAL;
  }
  *surface = handle;
  return GRANIT_SUCCESS;
}

granit_result create_win32_surface(granit_backend_plugin_instance instance,
                                   const granit_backend_plugin_win32_surface_desc* desc,
                                   granit_backend_plugin_surface* surface) noexcept {
  if (surface != nullptr)
    *surface = 0;
  if (instance == 0 || desc == nullptr || surface == nullptr || desc->struct_size < sizeof(*desc) ||
      desc->reserved != 0 || desc->instance == nullptr || desc->window == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
#if (defined(_WIN32) && !defined(__EMSCRIPTEN__)) || defined(GRANIT_WEBGPU_NATIVE_SURFACE_TEST)
  WGPUSurfaceSourceWindowsHWND source{};
  source.chain.sType = WGPUSType_SurfaceSourceWindowsHWND;
  source.hinstance = desc->instance;
  source.hwnd = desc->window;
  return create_native_surface(instance, source, surface);
#else
  return GRANIT_ERROR_UNSUPPORTED;
#endif
}

granit_result create_xcb_surface(granit_backend_plugin_instance instance,
                                 const granit_backend_plugin_xcb_surface_desc* desc,
                                 granit_backend_plugin_surface* surface) noexcept {
  if (surface != nullptr)
    *surface = 0;
  if (instance == 0 || desc == nullptr || surface == nullptr || desc->struct_size < sizeof(*desc) ||
      desc->reserved != 0 || desc->reserved_2 != 0 || desc->connection == nullptr ||
      desc->window == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
#if (defined(__linux__) && !defined(__EMSCRIPTEN__)) || defined(GRANIT_WEBGPU_NATIVE_SURFACE_TEST)
  WGPUSurfaceSourceXCBWindow source{};
  source.chain.sType = WGPUSType_SurfaceSourceXCBWindow;
  source.connection = desc->connection;
  source.window = desc->window;
  return create_native_surface(instance, source, surface);
#else
  return GRANIT_ERROR_UNSUPPORTED;
#endif
}

granit_result create_wayland_surface(granit_backend_plugin_instance instance,
                                     const granit_backend_plugin_wayland_surface_desc* desc,
                                     granit_backend_plugin_surface* surface) noexcept {
  if (surface != nullptr)
    *surface = 0;
  if (instance == 0 || desc == nullptr || surface == nullptr || desc->struct_size < sizeof(*desc) ||
      desc->reserved != 0 || desc->display == nullptr || desc->surface == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
#if (defined(__linux__) && !defined(__EMSCRIPTEN__)) || defined(GRANIT_WEBGPU_NATIVE_SURFACE_TEST)
  WGPUSurfaceSourceWaylandSurface source{};
  source.chain.sType = WGPUSType_SurfaceSourceWaylandSurface;
  source.display = desc->display;
  source.surface = desc->surface;
  return create_native_surface(instance, source, surface);
#else
  return GRANIT_ERROR_UNSUPPORTED;
#endif
}

granit_result create_canvas_surface(granit_backend_plugin_instance instance,
                                    const granit_backend_plugin_canvas_surface_desc* desc,
                                    granit_backend_plugin_surface* surface) noexcept {
  if (surface != nullptr)
    *surface = 0;
  if (instance == 0 || desc == nullptr || surface == nullptr ||
      desc->struct_size < sizeof(granit_backend_plugin_canvas_surface_desc) ||
      desc->reserved != 0 || desc->selector == nullptr || desc->selector_length == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const std::scoped_lock lock{instances_mutex};
  const auto found = instances.find(instance);
  if (found == instances.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  if (const auto ready = require_ready(*found->second); ready != GRANIT_SUCCESS)
    return ready;
#if defined(__EMSCRIPTEN__) || defined(GRANIT_WEBGPU_CANVAS_SURFACE_TEST)
  try {
    std::string selector{desc->selector, desc->selector_length};
    WGPUEmscriptenSurfaceSourceCanvasHTMLSelector canvas_desc{};
    canvas_desc.chain.sType = WGPUSType_EmscriptenSurfaceSourceCanvasHTMLSelector;
    canvas_desc.selector = {selector.data(), selector.size()};
    WGPUSurfaceDescriptor native_desc{};
    native_desc.nextInChain = &canvas_desc.chain;
    const auto native_surface = wgpuInstanceCreateSurface(found->second->instance, &native_desc);
    if (native_surface == nullptr)
      return GRANIT_ERROR_INITIALIZATION_FAILED;
    const auto handle = next_handle<granit_backend_plugin_surface>(next_surface);
    try {
      found->second->surfaces.emplace(
          handle, webgpu_instance::surface_record{native_surface, std::move(selector)});
    } catch (...) {
      wgpuSurfaceRelease(native_surface);
      throw;
    }
    *surface = handle;
    return GRANIT_SUCCESS;
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
#else
  return GRANIT_ERROR_UNSUPPORTED;
#endif
}

granit_result destroy_surface(granit_backend_plugin_instance instance,
                              granit_backend_plugin_surface surface) noexcept {
  if (instance == 0 || surface == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const std::scoped_lock lock{instances_mutex};
  const auto found = instances.find(instance);
  if (found == instances.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  const auto surface_found = found->second->surfaces.find(surface);
  if (surface_found == found->second->surfaces.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  if (std::any_of(found->second->swapchains.begin(), found->second->swapchains.end(),
                  [surface](const auto& entry) { return entry.second.surface == surface; }))
    return GRANIT_ERROR_INVALID_ARGUMENT;
  wgpuSurfaceRelease(static_cast<WGPUSurface>(surface_found->second.surface));
  found->second->surfaces.erase(surface_found);
  return GRANIT_SUCCESS;
}

granit_result configure_swapchain(webgpu_instance& state, WGPUSurface surface,
                                  const granit_backend_plugin_swapchain_desc& desc,
                                  granit_backend_plugin_swapchain_info& info) noexcept {
  WGPUSurfaceCapabilities capabilities = WGPU_SURFACE_CAPABILITIES_INIT;
  if (wgpuSurfaceGetCapabilities(surface, state.adapter, &capabilities) != WGPUStatus_Success)
    return GRANIT_ERROR_UNSUPPORTED;
  const auto release_capabilities = [&capabilities] {
    wgpuSurfaceCapabilitiesFreeMembers(capabilities);
  };
  WGPUTextureFormat format{};
  for (std::size_t index = 0; index < capabilities.formatCount; ++index) {
    if (capabilities.formats[index] == WGPUTextureFormat_RGBA8Unorm) {
      format = capabilities.formats[index];
      break;
    }
  }
  if (format == 0) {
    release_capabilities();
    return GRANIT_ERROR_UNSUPPORTED;
  }
  const WGPUPresentMode requested_mode =
      desc.present_mode == GRANIT_BACKEND_PLUGIN_PRESENT_MODE_MAILBOX ? WGPUPresentMode_Mailbox
      : desc.present_mode == GRANIT_BACKEND_PLUGIN_PRESENT_MODE_IMMEDIATE
          ? WGPUPresentMode_Immediate
          : WGPUPresentMode_Fifo;
  WGPUPresentMode selected_mode = WGPUPresentMode_Fifo;
  for (std::size_t index = 0; index < capabilities.presentModeCount; ++index) {
    if (capabilities.presentModes[index] == requested_mode) {
      selected_mode = requested_mode;
      break;
    }
  }
  WGPUSurfaceConfiguration configuration = WGPU_SURFACE_CONFIGURATION_INIT;
  configuration.device = state.device;
  configuration.format = format;
  configuration.usage = WGPUTextureUsage_RenderAttachment;
  configuration.width = desc.width;
  configuration.height = desc.height;
  configuration.presentMode = selected_mode;
  configuration.alphaMode = WGPUCompositeAlphaMode_Auto;
  wgpuSurfaceConfigure(surface, &configuration);
  release_capabilities();
  info = {sizeof(granit_backend_plugin_swapchain_info),
          desc.width,
          desc.height,
          1,
          selected_mode == WGPUPresentMode_Mailbox ? GRANIT_BACKEND_PLUGIN_PRESENT_MODE_MAILBOX
          : selected_mode == WGPUPresentMode_Immediate
              ? GRANIT_BACKEND_PLUGIN_PRESENT_MODE_IMMEDIATE
              : GRANIT_BACKEND_PLUGIN_PRESENT_MODE_FIFO,
          GRANIT_BACKEND_PLUGIN_TEXTURE_FORMAT_RGBA8_UNORM};
  return GRANIT_SUCCESS;
}

granit_result create_swapchain(granit_backend_plugin_instance instance,
                               granit_backend_plugin_surface surface,
                               const granit_backend_plugin_swapchain_desc* desc,
                               granit_backend_plugin_swapchain* swapchain) noexcept {
  if (swapchain != nullptr)
    *swapchain = 0;
  if (instance == 0 || surface == 0 || desc == nullptr || swapchain == nullptr ||
      desc->struct_size < sizeof(granit_backend_plugin_swapchain_desc) || desc->width == 0 ||
      desc->height == 0 || desc->present_mode > GRANIT_BACKEND_PLUGIN_PRESENT_MODE_IMMEDIATE)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const std::scoped_lock lock{instances_mutex};
  const auto found = instances.find(instance);
  if (found == instances.end() ||
      found->second->surfaces.find(surface) == found->second->surfaces.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  if (const auto ready = require_ready(*found->second); ready != GRANIT_SUCCESS)
    return ready;
  if (std::any_of(found->second->swapchains.begin(), found->second->swapchains.end(),
                  [surface](const auto& entry) { return entry.second.surface == surface; }))
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const auto native_surface =
      static_cast<WGPUSurface>(found->second->surfaces.find(surface)->second.surface);
  granit_backend_plugin_swapchain_info info{};
  if (const auto result = configure_swapchain(*found->second, native_surface, *desc, info);
      result != GRANIT_SUCCESS)
    return result;
  const auto handle = next_handle<granit_backend_plugin_swapchain>(next_swapchain);
  try {
    found->second->swapchains.emplace(
        handle, webgpu_instance::swapchain_record{surface, native_surface, info, 0, 0});
  } catch (const std::bad_alloc&) {
    wgpuSurfaceUnconfigure(native_surface);
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    wgpuSurfaceUnconfigure(native_surface);
    return GRANIT_ERROR_INTERNAL;
  }
  *swapchain = handle;
  return GRANIT_SUCCESS;
}

granit_result recreate_swapchain(granit_backend_plugin_instance instance,
                                 granit_backend_plugin_swapchain swapchain,
                                 const granit_backend_plugin_swapchain_desc* desc) noexcept {
  if (instance == 0 || swapchain == 0 || desc == nullptr ||
      desc->struct_size < sizeof(granit_backend_plugin_swapchain_desc) || desc->width == 0 ||
      desc->height == 0 || desc->present_mode > GRANIT_BACKEND_PLUGIN_PRESENT_MODE_IMMEDIATE)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const std::scoped_lock lock{instances_mutex};
  const auto found = instances.find(instance);
  if (found == instances.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  const auto swapchain_found = found->second->swapchains.find(swapchain);
  if (swapchain_found == found->second->swapchains.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  if (swapchain_found->second.acquired_texture != 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  granit_backend_plugin_swapchain_info info{};
  const auto result = configure_swapchain(
      *found->second, static_cast<WGPUSurface>(swapchain_found->second.native_surface), *desc,
      info);
  if (result == GRANIT_SUCCESS)
    swapchain_found->second.info = info;
  return result;
}

granit_result get_swapchain_info(granit_backend_plugin_instance instance,
                                 granit_backend_plugin_swapchain swapchain,
                                 granit_backend_plugin_swapchain_info* info) noexcept {
  if (instance == 0 || swapchain == 0 || info == nullptr ||
      info->struct_size < sizeof(granit_backend_plugin_swapchain_info))
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const std::scoped_lock lock{instances_mutex};
  const auto found = instances.find(instance);
  if (found == instances.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  const auto swapchain_found = found->second->swapchains.find(swapchain);
  if (swapchain_found == found->second->swapchains.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  *info = swapchain_found->second.info;
  return GRANIT_SUCCESS;
}

granit_result acquire_swapchain(granit_backend_plugin_instance instance,
                                granit_backend_plugin_swapchain swapchain,
                                granit_backend_plugin_acquired_frame* frame) noexcept {
  if (instance == 0 || swapchain == 0 || frame == nullptr ||
      frame->struct_size < sizeof(granit_backend_plugin_acquired_frame) || frame->reserved != 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const std::scoped_lock lock{instances_mutex};
  const auto found = instances.find(instance);
  if (found == instances.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  auto swapchain_found = found->second->swapchains.find(swapchain);
  if (swapchain_found == found->second->swapchains.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  if (swapchain_found->second.acquired_texture != 0)
    return GRANIT_ERROR_NOT_READY;
  WGPUSurfaceTexture acquired{};
  wgpuSurfaceGetCurrentTexture(static_cast<WGPUSurface>(swapchain_found->second.native_surface),
                               &acquired);
  const auto suboptimal = acquired.status == WGPUSurfaceGetCurrentTextureStatus_SuccessSuboptimal;
  if (acquired.texture == nullptr)
    return acquired.status == WGPUSurfaceGetCurrentTextureStatus_Timeout ? GRANIT_ERROR_NOT_READY
           : acquired.status == WGPUSurfaceGetCurrentTextureStatus_Outdated
               ? GRANIT_ERROR_OUT_OF_DATE
           : acquired.status == WGPUSurfaceGetCurrentTextureStatus_Lost ? GRANIT_ERROR_SURFACE_LOST
                                                                        : GRANIT_ERROR_INTERNAL;
  const auto native_view = wgpuTextureCreateView(acquired.texture, nullptr);
  if (native_view == nullptr) {
    static_cast<void>(
        present_surface(static_cast<WGPUSurface>(swapchain_found->second.native_surface)));
    wgpuTextureRelease(acquired.texture);
    return GRANIT_ERROR_OUT_OF_MEMORY;
  }
  const auto texture = next_handle<granit_backend_plugin_texture>(next_texture);
  const auto view = next_handle<granit_backend_plugin_texture_view>(next_texture_view);
  try {
    found->second->textures.emplace(
        texture, webgpu_instance::texture_record{
                     acquired.texture, swapchain_found->second.info.width,
                     swapchain_found->second.info.height, swapchain_found->second.info.format, 1, 1,
                     GRANIT_BACKEND_PLUGIN_TEXTURE_USAGE_RENDER_ATTACHMENT_BIT, true});
    found->second->texture_views.emplace(
        view, webgpu_instance::texture_view_record{native_view, texture, true});
  } catch (...) {
    found->second->texture_views.erase(view);
    found->second->textures.erase(texture);
    wgpuTextureViewRelease(native_view);
    static_cast<void>(
        present_surface(static_cast<WGPUSurface>(swapchain_found->second.native_surface)));
    wgpuTextureRelease(acquired.texture);
    return GRANIT_ERROR_OUT_OF_MEMORY;
  }
  swapchain_found->second.acquired_texture = texture;
  swapchain_found->second.acquired_view = view;
  *frame = {
      sizeof(granit_backend_plugin_acquired_frame), 0, suboptimal ? 1U : 0U, 0, texture, view};
  return GRANIT_SUCCESS;
}

granit_result finish_swapchain_frame(webgpu_instance& state,
                                     webgpu_instance::swapchain_record& swapchain,
                                     std::uint32_t& needs_recreate) noexcept {
  needs_recreate = 0;
  if (swapchain.acquired_texture == 0 || swapchain.acquired_view == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const auto view = state.texture_views.find(swapchain.acquired_view);
  const auto texture = state.textures.find(swapchain.acquired_texture);
  if (view == state.texture_views.end() || texture == state.textures.end())
    return GRANIT_ERROR_INTERNAL;
  const auto present_result = present_surface(static_cast<WGPUSurface>(swapchain.native_surface));
  wgpuTextureViewRelease(view->second.view);
  wgpuTextureRelease(texture->second.texture);
  state.texture_views.erase(view);
  state.textures.erase(texture);
  swapchain.acquired_view = 0;
  swapchain.acquired_texture = 0;
  return present_result == WGPUStatus_Success ? GRANIT_SUCCESS : GRANIT_ERROR_OUT_OF_DATE;
}

granit_result present_swapchain(granit_backend_plugin_instance instance,
                                granit_backend_plugin_swapchain swapchain,
                                std::uint32_t* needs_recreate) noexcept {
  if (instance == 0 || swapchain == 0 || needs_recreate == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const std::scoped_lock lock{instances_mutex};
  const auto found = instances.find(instance);
  if (found == instances.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  const auto swapchain_found = found->second->swapchains.find(swapchain);
  if (swapchain_found == found->second->swapchains.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  return finish_swapchain_frame(*found->second, swapchain_found->second, *needs_recreate);
}

granit_result cancel_swapchain(granit_backend_plugin_instance instance,
                               granit_backend_plugin_swapchain swapchain,
                               std::uint32_t* needs_recreate) noexcept {
  if (instance == 0 || swapchain == 0 || needs_recreate == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const std::scoped_lock lock{instances_mutex};
  const auto found = instances.find(instance);
  if (found == instances.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  const auto swapchain_found = found->second->swapchains.find(swapchain);
  if (swapchain_found == found->second->swapchains.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  return finish_swapchain_frame(*found->second, swapchain_found->second, *needs_recreate);
}

granit_result destroy_swapchain(granit_backend_plugin_instance instance,
                                granit_backend_plugin_swapchain swapchain) noexcept {
  if (instance == 0 || swapchain == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const std::scoped_lock lock{instances_mutex};
  const auto found = instances.find(instance);
  if (found == instances.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  const auto swapchain_found = found->second->swapchains.find(swapchain);
  if (swapchain_found == found->second->swapchains.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  if (swapchain_found->second.acquired_texture != 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  wgpuSurfaceUnconfigure(static_cast<WGPUSurface>(swapchain_found->second.native_surface));
  found->second->swapchains.erase(swapchain_found);
  return GRANIT_SUCCESS;
}

constexpr char plugin_name[] = "Granit WebGPU (Dawn)";
constexpr granit_backend_plugin_instance_api instance_api{
    sizeof(granit_backend_plugin_instance_api),
    0,
    get_capabilities,
    create_buffer,
    destroy_buffer,
    write_buffer,
    read_buffer,
    create_texture,
    destroy_texture,
    write_texture,
    create_texture_view,
    destroy_texture_view,
    create_sampler,
    destroy_sampler,
    create_bind_group_layout,
    destroy_bind_group_layout,
    create_bind_group,
    destroy_bind_group,
    create_shader,
    destroy_shader,
    create_pipeline_layout,
    destroy_pipeline_layout,
    create_render_pipeline,
    destroy_render_pipeline,
    create_command_recorder,
    destroy_command_recorder,
    recorder_copy_buffer_to_texture,
    finish_command_recorder,
    destroy_command_buffer,
    submit_command_buffer,
    recorder_copy_texture_to_buffer,
    get_instance_status,
    process_events,
    create_win32_surface,
    create_xcb_surface,
    create_wayland_surface,
    create_canvas_surface,
    destroy_surface,
    create_swapchain,
    recreate_swapchain,
    get_swapchain_info,
    acquire_swapchain,
    present_swapchain,
    cancel_swapchain,
    destroy_swapchain,
    recorder_begin_rendering,
    recorder_bind_pipeline,
    recorder_bind_graphics_groups,
    recorder_bind_vertex_buffers,
    recorder_bind_index_buffer,
    recorder_draw_vertices,
    recorder_draw_indices,
    recorder_end_rendering,
    write_upload_batch,
    create_compute_pipeline,
    destroy_compute_pipeline,
    recorder_begin_compute,
    recorder_bind_compute_pipeline,
    recorder_bind_compute_groups,
    recorder_dispatch,
    recorder_end_compute,
    recorder_set_viewports,
    recorder_set_scissors};
constexpr granit_backend_plugin_api plugin_api{sizeof(granit_backend_plugin_api),
                                               GRANIT_BACKEND_PLUGIN_ABI_VERSION,
                                               GRANIT_BACKEND_PLUGIN_KIND_WEBGPU,
                                               0,
                                               plugin_name,
                                               sizeof(plugin_name) - 1,
                                               create_backend,
                                               destroy_backend,
                                               &instance_api};

} // namespace

extern "C" GRANIT_BACKEND_PLUGIN_EXPORT const granit_backend_plugin_api*
granit_backend_plugin_query(uint32_t requested_abi) noexcept {
  return requested_abi == GRANIT_BACKEND_PLUGIN_ABI_VERSION ? &plugin_api : nullptr;
}
