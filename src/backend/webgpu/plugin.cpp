// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "backend/callback_lifetime.h"
#include "backend/lifecycle.h"
#include "backend/plugin_api.h"

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <mutex>
#include <new>
#include <string>
#include <unordered_map>

#include <webgpu/webgpu.h>

#if defined(_WIN32)
#define GRANIT_BACKEND_PLUGIN_EXPORT __declspec(dllexport)
#else
#define GRANIT_BACKEND_PLUGIN_EXPORT __attribute__((visibility("default")))
#endif

namespace {

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
    granit_backend_plugin_texture_view texture_view;
    granit_backend_plugin_sampler sampler;
  };
  struct pipeline_layout_record {
    WGPUPipelineLayout pipeline_layout;
    granit_backend_plugin_bind_group_layout bind_group_layout;
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
  struct command_recorder_record {
    WGPUCommandEncoder encoder;
    bool finished;
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
  std::unordered_map<granit_backend_plugin_bind_group_layout, WGPUBindGroupLayout>
      bind_group_layouts;
  std::unordered_map<granit_backend_plugin_bind_group, bind_group_record> bind_groups;
  std::unordered_map<granit_backend_plugin_shader, shader_record> shaders;
  std::unordered_map<granit_backend_plugin_pipeline_layout, pipeline_layout_record>
      pipeline_layouts;
  std::unordered_map<granit_backend_plugin_render_pipeline, render_pipeline_record>
      render_pipelines;
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
std::atomic_uint64_t next_command_recorder{1};
std::atomic_uint64_t next_command_buffer{1};
std::atomic_uint64_t next_swapchain{1};
#if defined(__EMSCRIPTEN__) || defined(GRANIT_WEBGPU_CANVAS_SURFACE_TEST)
std::atomic_uint64_t next_surface{1};
#endif
#if defined(GRANIT_WEBGPU_DEFER_INITIALIZATION_TEST)
std::atomic_uint32_t initialization_sequence_for_test{};
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
    wgpuCommandEncoderRelease(recorder.encoder);
  }
  state.command_recorders.clear();
  for (const auto& [handle, pipeline] : state.render_pipelines) {
    static_cast<void>(handle);
    wgpuRenderPipelineRelease(pipeline.render_pipeline);
  }
  state.render_pipelines.clear();
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
    wgpuBindGroupLayoutRelease(layout);
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
  };
#if defined(GRANIT_WEBGPU_DEFER_INITIALIZATION_TEST)
  const auto initialization_index = initialization_sequence_for_test.fetch_add(1);
  const auto extended_host = host->struct_size > sizeof(granit_backend_plugin_host_api);
  state->fail_initialization_for_test = initialization_index == 1 && extended_host;
  state->deferred_initialization_for_test =
      initialization_index == 0 || state->fail_initialization_for_test;
  if (!state->deferred_initialization_for_test)
    state->lifecycle.mark_ready();
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
                               GRANIT_BACKEND_PLUGIN_BUFFER_USAGE_COPY_DST_BIT;
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
  if (instance == 0 || desc == nullptr || out_texture == nullptr ||
      desc->struct_size < sizeof(granit_backend_plugin_texture_desc) || desc->reserved != 0 ||
      desc->reserved_flags != 0 || desc->width == 0 || desc->height == 0 || desc->usage == 0 ||
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
  descriptor.size = {desc->width, desc->height, 1};
  descriptor.format = WGPUTextureFormat_RGBA8Unorm;
  descriptor.mipLevelCount = 1;
  descriptor.sampleCount = 1;
  const auto native = wgpuDeviceCreateTexture(state.device, &descriptor);
  if (native == nullptr) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  }
  const auto handle = next_handle<granit_backend_plugin_texture>(next_texture);
  try {
    if (!state.textures
             .emplace(handle, webgpu_instance::texture_record{native, desc->width, desc->height,
                                                              desc->usage, false})
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

granit_result create_texture_view(granit_backend_plugin_instance instance,
                                  granit_backend_plugin_texture texture,
                                  granit_backend_plugin_texture_view* out_view) noexcept {
  if (out_view != nullptr)
    *out_view = 0;
  if (instance == 0 || texture == 0 || out_view == nullptr)
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
  const auto native = wgpuTextureCreateView(texture_found->second.texture, nullptr);
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
                  [view](const auto& entry) { return entry.second.texture_view == view; }))
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
  if (instance == 0 || desc == nullptr || out_sampler == nullptr ||
      desc->struct_size < sizeof(granit_backend_plugin_sampler_desc) || desc->reserved != 0 ||
      !valid_filter(desc->min_filter) || !valid_filter(desc->mag_filter)) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  const std::scoped_lock lock{instances_mutex};
  const auto found = instances.find(instance);
  if (found == instances.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  if (const auto ready = require_ready(*found->second); ready != GRANIT_SUCCESS)
    return ready;
  WGPUSamplerDescriptor descriptor = WGPU_SAMPLER_DESCRIPTOR_INIT;
  descriptor.addressModeU = WGPUAddressMode_ClampToEdge;
  descriptor.addressModeV = WGPUAddressMode_ClampToEdge;
  descriptor.addressModeW = WGPUAddressMode_ClampToEdge;
  descriptor.minFilter = desc->min_filter == GRANIT_BACKEND_PLUGIN_FILTER_LINEAR
                             ? WGPUFilterMode_Linear
                             : WGPUFilterMode_Nearest;
  descriptor.magFilter = desc->mag_filter == GRANIT_BACKEND_PLUGIN_FILTER_LINEAR
                             ? WGPUFilterMode_Linear
                             : WGPUFilterMode_Nearest;
  descriptor.mipmapFilter = WGPUMipmapFilterMode_Nearest;
  descriptor.maxAnisotropy = 1;
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
                  [sampler](const auto& entry) { return entry.second.sampler == sampler; }))
    return GRANIT_ERROR_INVALID_ARGUMENT;
  wgpuSamplerRelease(sampler_found->second);
  found->second->samplers.erase(sampler_found);
  return GRANIT_SUCCESS;
}

granit_result
create_bind_group_layout(granit_backend_plugin_instance instance,
                         granit_backend_plugin_bind_group_layout* out_layout) noexcept {
  if (out_layout != nullptr)
    *out_layout = 0;
  if (instance == 0 || out_layout == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const std::scoped_lock lock{instances_mutex};
  const auto found = instances.find(instance);
  if (found == instances.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  if (const auto ready = require_ready(*found->second); ready != GRANIT_SUCCESS)
    return ready;

  WGPUBindGroupLayoutEntry entries[2]{WGPU_BIND_GROUP_LAYOUT_ENTRY_INIT,
                                      WGPU_BIND_GROUP_LAYOUT_ENTRY_INIT};
  entries[0].binding = 0;
  entries[0].visibility = WGPUShaderStage_Fragment;
  entries[0].texture.sampleType = WGPUTextureSampleType_Float;
  entries[0].texture.viewDimension = WGPUTextureViewDimension_2D;
  entries[1].binding = 1;
  entries[1].visibility = WGPUShaderStage_Fragment;
  entries[1].sampler.type = WGPUSamplerBindingType_Filtering;
  WGPUBindGroupLayoutDescriptor descriptor = WGPU_BIND_GROUP_LAYOUT_DESCRIPTOR_INIT;
  descriptor.entryCount = 2;
  descriptor.entries = entries;
  const auto native = wgpuDeviceCreateBindGroupLayout(found->second->device, &descriptor);
  if (native == nullptr)
    return GRANIT_ERROR_OUT_OF_MEMORY;
  const auto handle = next_handle<granit_backend_plugin_bind_group_layout>(next_bind_group_layout);
  try {
    if (!found->second->bind_group_layouts.emplace(handle, native).second) {
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
  const auto used_by_pipeline =
      std::any_of(state.pipeline_layouts.begin(), state.pipeline_layouts.end(),
                  [layout](const auto& entry) { return entry.second.bind_group_layout == layout; });
  if (used_by_group || used_by_pipeline)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  wgpuBindGroupLayoutRelease(layout_found->second);
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
      desc->layout == 0 || desc->texture_view == 0 || desc->sampler == 0) {
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
  const auto view = state.texture_views.find(desc->texture_view);
  const auto sampler = state.samplers.find(desc->sampler);
  if (layout == state.bind_group_layouts.end() || view == state.texture_views.end() ||
      sampler == state.samplers.end()) {
    return GRANIT_ERROR_INVALID_HANDLE;
  }
  WGPUBindGroupEntry entries[2]{WGPU_BIND_GROUP_ENTRY_INIT, WGPU_BIND_GROUP_ENTRY_INIT};
  entries[0].binding = 0;
  entries[0].textureView = view->second.view;
  entries[1].binding = 1;
  entries[1].sampler = sampler->second;
  WGPUBindGroupDescriptor descriptor = WGPU_BIND_GROUP_DESCRIPTOR_INIT;
  descriptor.layout = layout->second;
  descriptor.entryCount = 2;
  descriptor.entries = entries;
  const auto native = wgpuDeviceCreateBindGroup(state.device, &descriptor);
  if (native == nullptr)
    return GRANIT_ERROR_OUT_OF_MEMORY;
  const auto handle = next_handle<granit_backend_plugin_bind_group>(next_bind_group);
  try {
    const auto record =
        webgpu_instance::bind_group_record{native, desc->layout, desc->texture_view, desc->sampler};
    if (!state.bind_groups.emplace(handle, record).second) {
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
       desc->stage != GRANIT_BACKEND_PLUGIN_SHADER_STAGE_FRAGMENT))
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
                  }))
    return GRANIT_ERROR_INVALID_ARGUMENT;
  wgpuShaderModuleRelease(shader_found->second.shader);
  state.shaders.erase(shader_found);
  return GRANIT_SUCCESS;
}

granit_result
create_pipeline_layout(granit_backend_plugin_instance instance,
                       granit_backend_plugin_bind_group_layout bind_group_layout,
                       granit_backend_plugin_pipeline_layout* out_pipeline_layout) noexcept {
  if (out_pipeline_layout != nullptr)
    *out_pipeline_layout = 0;
  if (instance == 0 || bind_group_layout == 0 || out_pipeline_layout == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const std::scoped_lock lock{instances_mutex};
  const auto found = instances.find(instance);
  if (found == instances.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  if (const auto ready = require_ready(*found->second); ready != GRANIT_SUCCESS)
    return ready;
  auto& state = *found->second;
  const auto layout = state.bind_group_layouts.find(bind_group_layout);
  if (layout == state.bind_group_layouts.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  WGPUPipelineLayoutDescriptor descriptor = WGPU_PIPELINE_LAYOUT_DESCRIPTOR_INIT;
  descriptor.bindGroupLayoutCount = 1;
  descriptor.bindGroupLayouts = &layout->second;
  const auto native = wgpuDeviceCreatePipelineLayout(state.device, &descriptor);
  if (native == nullptr)
    return GRANIT_ERROR_OUT_OF_MEMORY;
  const auto handle = next_handle<granit_backend_plugin_pipeline_layout>(next_pipeline_layout);
  try {
    const auto record = webgpu_instance::pipeline_layout_record{native, bind_group_layout};
    if (!state.pipeline_layouts.emplace(handle, record).second) {
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
                  [layout](const auto& entry) { return entry.second.pipeline_layout == layout; }))
    return GRANIT_ERROR_INVALID_ARGUMENT;
  wgpuPipelineLayoutRelease(layout_found->second.pipeline_layout);
  state.pipeline_layouts.erase(layout_found);
  return GRANIT_SUCCESS;
}

granit_result
create_render_pipeline(granit_backend_plugin_instance instance,
                       const granit_backend_plugin_render_pipeline_desc* desc,
                       granit_backend_plugin_render_pipeline* out_render_pipeline) noexcept {
  if (out_render_pipeline != nullptr)
    *out_render_pipeline = 0;
  if (instance == 0 || desc == nullptr || out_render_pipeline == nullptr ||
      desc->struct_size < sizeof(*desc) || desc->reserved != 0 || desc->layout == 0 ||
      desc->vertex_shader == 0 || desc->fragment_shader == 0)
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
  WGPUColorTargetState target = WGPU_COLOR_TARGET_STATE_INIT;
  target.format = WGPUTextureFormat_RGBA8Unorm;
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
  descriptor.primitive.topology = WGPUPrimitiveTopology_TriangleList;
  descriptor.multisample.count = 1;
  descriptor.multisample.mask = UINT32_MAX;
  descriptor.fragment = &fragment;
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
    const auto record = webgpu_instance::command_recorder_record{native, false};
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
  if (recorder_found->second.finished)
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

granit_result recorder_draw(granit_backend_plugin_instance instance,
                            granit_backend_plugin_command_recorder recorder,
                            granit_backend_plugin_texture_view target,
                            granit_backend_plugin_render_pipeline pipeline,
                            granit_backend_plugin_bind_group bind_group) noexcept {
  if (instance == 0 || recorder == 0 || target == 0 || pipeline == 0 || bind_group == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const std::scoped_lock lock{instances_mutex};
  const auto found = instances.find(instance);
  if (found == instances.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  if (const auto ready = require_ready(*found->second); ready != GRANIT_SUCCESS)
    return ready;
  auto& state = *found->second;
  const auto recorder_found = state.command_recorders.find(recorder);
  const auto view_found = state.texture_views.find(target);
  const auto pipeline_found = state.render_pipelines.find(pipeline);
  const auto group_found = state.bind_groups.find(bind_group);
  if (recorder_found == state.command_recorders.end() || view_found == state.texture_views.end() ||
      pipeline_found == state.render_pipelines.end() || group_found == state.bind_groups.end()) {
    return GRANIT_ERROR_INVALID_HANDLE;
  }
  if (recorder_found->second.finished)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const auto texture_found = state.textures.find(view_found->second.texture);
  if (texture_found == state.textures.end() ||
      (texture_found->second.usage & GRANIT_BACKEND_PLUGIN_TEXTURE_USAGE_RENDER_ATTACHMENT_BIT) ==
          0) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  WGPURenderPassColorAttachment color = WGPU_RENDER_PASS_COLOR_ATTACHMENT_INIT;
  color.view = view_found->second.view;
  color.loadOp = WGPULoadOp_Clear;
  color.storeOp = WGPUStoreOp_Store;
  color.clearValue = {0.0, 0.0, 0.0, 1.0};
  WGPURenderPassDescriptor descriptor = WGPU_RENDER_PASS_DESCRIPTOR_INIT;
  descriptor.colorAttachmentCount = 1;
  descriptor.colorAttachments = &color;
  const auto pass = wgpuCommandEncoderBeginRenderPass(recorder_found->second.encoder, &descriptor);
  if (pass == nullptr)
    return GRANIT_ERROR_INITIALIZATION_FAILED;
  wgpuRenderPassEncoderSetPipeline(pass, pipeline_found->second.render_pipeline);
  wgpuRenderPassEncoderSetBindGroup(pass, 0, group_found->second.bind_group, 0, nullptr);
  wgpuRenderPassEncoderDraw(pass, 3, 1, 0, 0);
  wgpuRenderPassEncoderEnd(pass);
  wgpuRenderPassEncoderRelease(pass);
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
  if (recorder_found->second.finished)
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
  if (recorder_found->second.finished)
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
#if defined(__EMSCRIPTEN__) || defined(GRANIT_WEBGPU_CANVAS_SURFACE_TEST)
  wgpuSurfaceRelease(static_cast<WGPUSurface>(surface_found->second.surface));
#endif
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
                     swapchain_found->second.info.height,
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
    recorder_draw,
    finish_command_recorder,
    destroy_command_buffer,
    submit_command_buffer,
    recorder_copy_texture_to_buffer,
    get_instance_status,
    process_events,
    create_canvas_surface,
    destroy_surface,
    create_swapchain,
    recreate_swapchain,
    get_swapchain_info,
    acquire_swapchain,
    present_swapchain,
    cancel_swapchain,
    destroy_swapchain};
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
