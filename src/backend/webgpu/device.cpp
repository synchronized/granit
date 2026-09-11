// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "backend/webgpu/device.h"
#include "backend/contracts/callback_lifetime.h"
#include "backend/contracts/lifecycle.h"
#include "backend/webgpu/device_state.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <mutex>
#include <new>

#include <webgpu/webgpu.h>

namespace {

using granit::detail::webgpu_device_state;
using namespace granit::detail::webgpu_native;

uint32_t texture_compression_features(WGPUDevice device) noexcept {
  uint32_t result{};
  if (wgpuDeviceHasFeature(device, WGPUFeatureName_TextureCompressionBC))
    result |= GRANIT_WEBGPU_TEXTURE_COMPRESSION_BC_BIT;
  if (wgpuDeviceHasFeature(device, WGPUFeatureName_TextureCompressionETC2))
    result |= GRANIT_WEBGPU_TEXTURE_COMPRESSION_ETC2_BIT;
  if (wgpuDeviceHasFeature(device, WGPUFeatureName_TextureCompressionASTC))
    result |= GRANIT_WEBGPU_TEXTURE_COMPRESSION_ASTC_BIT;
  return result;
}

std::size_t collect_optional_features(WGPUAdapter adapter,
                                      std::array<WGPUFeatureName, 4>& features) noexcept {
  std::size_t count{};
  // 浏览器虽可能公布 timestamp-query，但 WebGPU JS 不提供任意 CommandEncoder
  // writeTimestamp。当前公共时间戳契约无法完整实现，必须报告不支持而不是触发 JS 异常。
#if !defined(__EMSCRIPTEN__)
  if (wgpuAdapterHasFeature(adapter, WGPUFeatureName_TimestampQuery))
    features[count++] = WGPUFeatureName_TimestampQuery;
#endif
  constexpr std::array candidates{WGPUFeatureName_TextureCompressionBC,
                                  WGPUFeatureName_TextureCompressionETC2,
                                  WGPUFeatureName_TextureCompressionASTC};
  for (const auto feature : candidates) {
    if (wgpuAdapterHasFeature(adapter, feature))
      features[count++] = feature;
  }
  return count;
}

constexpr std::uint32_t device_surface_types =
#if defined(GRANIT_WEBGPU_NATIVE_SURFACE_TEST)
    GRANIT_WEBGPU_SURFACE_TYPE_WIN32_BIT | GRANIT_WEBGPU_SURFACE_TYPE_XCB_BIT |
    GRANIT_WEBGPU_SURFACE_TYPE_WAYLAND_BIT |
#elif defined(_WIN32) && !defined(__EMSCRIPTEN__)
    GRANIT_WEBGPU_SURFACE_TYPE_WIN32_BIT |
#elif defined(__linux__) && !defined(__EMSCRIPTEN__)
    GRANIT_WEBGPU_SURFACE_TYPE_XCB_BIT | GRANIT_WEBGPU_SURFACE_TYPE_WAYLAND_BIT |
#endif
#if defined(__EMSCRIPTEN__) || defined(GRANIT_WEBGPU_CANVAS_SURFACE_TEST)
    GRANIT_WEBGPU_SURFACE_TYPE_CANVAS_BIT;
#else
    UINT32_C(0);
#endif

struct adapter_request {
  WGPURequestAdapterStatus status{};
  WGPUAdapter adapter{};
  char message[256]{};
  std::uint32_t message_length{};
};

struct device_request {
  const webgpu_host_api* host{};
  WGPURequestDeviceStatus status{};
  WGPUDevice device{};
};

#if defined(GRANIT_WEBGPU_DEFER_INITIALIZATION_TEST)
#endif

void deallocate(const webgpu_host_api& host, void* memory) noexcept {
  try {
    host.deallocate(memory, sizeof(webgpu_device_state), alignof(webgpu_device_state),
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

void release_resources(webgpu_device_state& state) noexcept {
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
  state.readbacks.clear();
  state.timestamp_queries.clear();
  for (const auto& [handle, recorder] : state.command_recorders) {
    static_cast<void>(handle);
    if (recorder.pass != nullptr)
      wgpuRenderPassEncoderRelease(recorder.pass);
    if (recorder.compute_pass != nullptr)
      wgpuComputePassEncoderRelease(recorder.compute_pass);
    for (const auto buffer : recorder.temporary_buffers)
      wgpuBufferRelease(buffer);
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

void emit(const webgpu_host_api& host, granit_diagnostic_severity severity, const char* message,
          std::uint32_t message_length) noexcept {
  if (host.diagnostic_callback == nullptr) {
    return;
  }
  try {
    host.diagnostic_callback(severity, GRANIT_DIAGNOSTIC_CATEGORY_DEVICE, message, message_length,
                             host.diagnostic_user_data);
  } catch (...) {
  }
}

void emit_dawn_message(const webgpu_host_api* host, WGPUStringView message) noexcept {
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
  auto& state = *static_cast<webgpu_device_state*>(data);
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

void receive_uncaptured_error(const WGPUDevice*, WGPUErrorType, WGPUStringView message, void* data,
                              void*) noexcept {
  const auto& state = *static_cast<const webgpu_device_state*>(data);
  emit_dawn_message(&state.host, message);
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
  auto& state = *static_cast<webgpu_device_state*>(data);
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
        sizeof(webgpu_capabilities),
        0,
        limits.minUniformBufferOffsetAlignment,
        limits.minStorageBufferOffsetAlignment,
        limits.maxUniformBufferBindingSize,
        limits.maxStorageBufferBindingSize,
        limits.maxBufferSize,
        limits.maxTextureDimension2D,
        limits.maxBindGroups,
        limits.maxColorAttachments,
        device_surface_types,
        0,
        1 | 4,
        16.0F,
        wgpuDeviceHasFeature(device, WGPUFeatureName_TimestampQuery)
            ? GRANIT_WEBGPU_FEATURE_TIMESTAMP_QUERY_BIT
            : UINT64_C(0),
        texture_compression_features(device),
        0,
    };
    state.lifecycle.mark_ready();
    constexpr char diagnostic[] = "Emscripten WebGPU adapter and device are ready";
    emit(state.host, GRANIT_DIAGNOSTIC_SEVERITY_INFO, diagnostic, sizeof(diagnostic) - 1);
  }));
}

void receive_adapter_async(WGPURequestAdapterStatus status, WGPUAdapter adapter,
                           WGPUStringView message, void* data, void*) noexcept {
  auto& state = *static_cast<webgpu_device_state*>(data);
  static_cast<void>(state.adapter_ticket.invoke([&state, status, adapter, message] {
    if (status != WGPURequestAdapterStatus_Success || adapter == nullptr) {
      emit_dawn_message(&state.host, message);
      state.lifecycle.mark_failed(GRANIT_ERROR_NO_SUITABLE_DEVICE);
      return;
    }
    state.adapter = adapter;
    WGPUDeviceDescriptor descriptor = WGPU_DEVICE_DESCRIPTOR_INIT;
    std::array<WGPUFeatureName, 4> features{};
    descriptor.requiredFeatureCount = collect_optional_features(adapter, features);
    descriptor.requiredFeatures = descriptor.requiredFeatureCount == 0 ? nullptr : features.data();
    descriptor.deviceLostCallbackInfo.mode = WGPUCallbackMode_AllowSpontaneous;
    descriptor.deviceLostCallbackInfo.callback = receive_device_lost;
    descriptor.deviceLostCallbackInfo.userdata1 = &state;
    descriptor.uncapturedErrorCallbackInfo.callback = receive_uncaptured_error;
    descriptor.uncapturedErrorCallbackInfo.userdata1 = &state;
    WGPURequestDeviceCallbackInfo callback = WGPU_REQUEST_DEVICE_CALLBACK_INFO_INIT;
    callback.mode = WGPUCallbackMode_AllowSpontaneous;
    callback.callback = receive_device_async;
    callback.userdata1 = &state;
    static_cast<void>(wgpuAdapterRequestDevice(adapter, &descriptor, callback));
  }));
}
#endif

#if !defined(__EMSCRIPTEN__)
constexpr std::uint64_t request_timeout_ns = UINT64_C(10000000000);

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

granit_result register_instance(webgpu_device_state* state,
                                webgpu_instance_handle* out_instance) noexcept {
  webgpu_instance_handle handle = next_instance.fetch_add(1, std::memory_order_relaxed);
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

granit_result create_backend(const webgpu_host_api* host,
                             webgpu_instance_handle* out_instance) noexcept {
  constexpr std::size_t minimum_host_size =
      offsetof(webgpu_host_api, allocator_user_data) + sizeof(void*);
  if (host == nullptr || host->struct_size < minimum_host_size || host->reserved != 0 ||
      out_instance == nullptr || host->allocate == nullptr || host->deallocate == nullptr) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  *out_instance = 0;

  void* memory = nullptr;
  try {
    memory = host->allocate(sizeof(webgpu_device_state), alignof(webgpu_device_state),
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
  auto* state = new (memory) webgpu_device_state{*host, wgpuCreateInstance(&descriptor)};
  if (state->instance == nullptr) {
    state->~webgpu_device_state();
    deallocate(*host, memory);
    return GRANIT_ERROR_INITIALIZATION_FAILED;
  }

#if defined(__EMSCRIPTEN__)
  const auto register_result = register_instance(state, out_instance);
  if (register_result != GRANIT_SUCCESS) {
    release_resources(*state);
    state->~webgpu_device_state();
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
    state->~webgpu_device_state();
    deallocate(*host, memory);
    return GRANIT_ERROR_NO_SUITABLE_DEVICE;
  }
  state->adapter = adapter.adapter;

  device_request device{host};
  WGPUDeviceDescriptor device_descriptor = WGPU_DEVICE_DESCRIPTOR_INIT;
  std::array<WGPUFeatureName, 4> features{};
  device_descriptor.requiredFeatureCount = collect_optional_features(state->adapter, features);
  device_descriptor.requiredFeatures =
      device_descriptor.requiredFeatureCount == 0 ? nullptr : features.data();
  device_descriptor.deviceLostCallbackInfo.mode = WGPUCallbackMode_AllowSpontaneous;
  device_descriptor.deviceLostCallbackInfo.callback = receive_device_lost;
  device_descriptor.deviceLostCallbackInfo.userdata1 = state;
  device_descriptor.uncapturedErrorCallbackInfo.callback = receive_uncaptured_error;
  device_descriptor.uncapturedErrorCallbackInfo.userdata1 = state;
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
    state->~webgpu_device_state();
    deallocate(*host, memory);
    return GRANIT_ERROR_INITIALIZATION_FAILED;
  }
  state->device = device.device;
  state->queue = wgpuDeviceGetQueue(state->device);
  if (state->queue == nullptr) {
    constexpr char message[] = "Dawn WebGPU queue request failed";
    emit(*host, GRANIT_DIAGNOSTIC_SEVERITY_ERROR, message, sizeof(message) - 1);
    release_resources(*state);
    state->~webgpu_device_state();
    deallocate(*host, memory);
    return GRANIT_ERROR_INITIALIZATION_FAILED;
  }

  WGPULimits device_limits = WGPU_LIMITS_INIT;
  if (wgpuDeviceGetLimits(state->device, &device_limits) != WGPUStatus_Success) {
    constexpr char message[] = "Dawn WebGPU device limits query failed";
    emit(*host, GRANIT_DIAGNOSTIC_SEVERITY_ERROR, message, sizeof(message) - 1);
    release_resources(*state);
    state->~webgpu_device_state();
    deallocate(*host, memory);
    return GRANIT_ERROR_INITIALIZATION_FAILED;
  }
  state->capabilities = {
      sizeof(webgpu_capabilities),
      0,
      device_limits.minUniformBufferOffsetAlignment,
      device_limits.minStorageBufferOffsetAlignment,
      device_limits.maxUniformBufferBindingSize,
      device_limits.maxStorageBufferBindingSize,
      device_limits.maxBufferSize,
      device_limits.maxTextureDimension2D,
      device_limits.maxBindGroups,
      device_limits.maxColorAttachments,
      device_surface_types,
      0,
      1 | 4,
      16.0F,
      wgpuDeviceHasFeature(state->device, WGPUFeatureName_TimestampQuery)
          ? GRANIT_WEBGPU_FEATURE_TIMESTAMP_QUERY_BIT
          : UINT64_C(0),
      texture_compression_features(state->device),
      0,
  };
#if defined(GRANIT_WEBGPU_DEFER_INITIALIZATION_TEST)
  const auto extended_host = host->struct_size > sizeof(webgpu_host_api);
  state->fail_initialization_for_test = extended_host;
  state->deferred_initialization_for_test = true;
#else
  state->lifecycle.mark_ready();
#endif

  const auto register_result = register_instance(state, out_instance);
  if (register_result != GRANIT_SUCCESS) {
    release_resources(*state);
    state->~webgpu_device_state();
    deallocate(*host, memory);
    return register_result;
  }

  constexpr char message[] = "Dawn WebGPU instance, adapter and device created";
  emit(*host, GRANIT_DIAGNOSTIC_SEVERITY_INFO, message, sizeof(message) - 1);
  return GRANIT_SUCCESS;
#endif
}

void destroy_backend(webgpu_instance_handle instance) noexcept {
  webgpu_device_state* state = nullptr;
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
  state->~webgpu_device_state();
  deallocate(host, state);
  constexpr char message[] = "Dawn WebGPU device, adapter and instance destroyed";
  emit(host, GRANIT_DIAGNOSTIC_SEVERITY_INFO, message, sizeof(message) - 1);
}

granit_result get_capabilities(webgpu_instance_handle instance,
                               webgpu_capabilities* capabilities) noexcept {
  if (instance == 0 || capabilities == nullptr ||
      capabilities->struct_size < sizeof(webgpu_capabilities) || capabilities->reserved != 0) {
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

granit_result get_instance_status(webgpu_instance_handle instance,
                                  webgpu_instance_status* status) noexcept {
  if (instance == 0 || status == nullptr || status->struct_size < sizeof(webgpu_instance_status) ||
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
  webgpu_instance_state device_state{};
  switch (lifecycle.state) {
  case granit::detail::backend_lifecycle_state::initializing:
    device_state = GRANIT_WEBGPU_INSTANCE_STATE_INITIALIZING;
    break;
  case granit::detail::backend_lifecycle_state::ready:
    device_state = GRANIT_WEBGPU_INSTANCE_STATE_READY;
    break;
  case granit::detail::backend_lifecycle_state::failed:
    device_state = GRANIT_WEBGPU_INSTANCE_STATE_FAILED;
    break;
  case granit::detail::backend_lifecycle_state::device_lost:
    device_state = GRANIT_WEBGPU_INSTANCE_STATE_DEVICE_LOST;
    break;
  }
  *status = {caller_size, device_state, lifecycle.failure_result, 0};
  return GRANIT_SUCCESS;
}

granit_result process_events(webgpu_instance_handle instance) noexcept {
  if (instance == 0) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  const std::scoped_lock lock{instances_mutex};
  const auto found = instances.find(instance);
  if (found == instances.end()) {
    return GRANIT_ERROR_INVALID_HANDLE;
  }
#if defined(__EMSCRIPTEN__)
  // 浏览器通过事件循环交付 AllowSpontaneous 回调。对 Emdawnwebgpu 调用 ProcessEvents
  // 会进入仅供原生 Future 使用的 Instance 事件管理路径，并可能取消尚未完成的管线回调。
#else
  wgpuInstanceProcessEvents(found->second->instance);
#endif
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

} // namespace

namespace granit::detail {
namespace {

bool is_valid_host(const webgpu_host_api* host) noexcept {
  constexpr std::size_t minimum_size =
      offsetof(webgpu_host_api, allocator_user_data) + sizeof(void*);
  return host != nullptr && host->struct_size >= minimum_size && host->reserved == 0 &&
         host->allocate != nullptr && host->deallocate != nullptr;
}

} // namespace

webgpu_device::~webgpu_device() { close(); }

granit_result webgpu_device::open() noexcept {
  close();
  open_ = true;
  return GRANIT_SUCCESS;
}

granit_result webgpu_device::create_instance(const webgpu_host_api* host) noexcept {
  if (!open_ || instance_ != 0 || !is_valid_host(host)) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }

  webgpu_instance_handle instance = 0;
  granit_result result = GRANIT_ERROR_INTERNAL;
  try {
    result = ::create_backend(host, &instance);
  } catch (...) {
    if (instance != 0) {
      try {
        ::destroy_backend(instance);
      } catch (...) {
      }
    }
    return GRANIT_ERROR_INTERNAL;
  }
  if (result != GRANIT_SUCCESS) {
    if (instance != 0) {
      try {
        ::destroy_backend(instance);
      } catch (...) {
      }
    }
    return result;
  }
  if (instance == 0) {
    return GRANIT_ERROR_INITIALIZATION_FAILED;
  }

  instance_ = instance;
  return GRANIT_SUCCESS;
}

granit_result webgpu_device::destroy_instance() noexcept {
  if (!open_ || instance_ == 0) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  try {
    ::destroy_backend(instance_);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
  instance_ = 0;
  return GRANIT_SUCCESS;
}

granit_result webgpu_device::get_capabilities(webgpu_capabilities* capabilities) noexcept {
  constexpr std::size_t minimum_size =
      offsetof(webgpu_capabilities, reserved_2) + sizeof(std::uint32_t);
  if (!open_ || instance_ == 0 || capabilities == nullptr ||
      capabilities->struct_size < minimum_size || capabilities->reserved != 0 ||
      capabilities->reserved_2 != 0) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  try {
    return ::get_capabilities(instance_, capabilities);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result webgpu_device::get_instance_status(webgpu_instance_status* status) noexcept {
  if (!open_ || instance_ == 0 || status == nullptr ||
      status->struct_size < sizeof(webgpu_instance_status) || status->reserved != 0) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  try {
    return ::get_instance_status(instance_, status);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result webgpu_device::process_events() noexcept {
  if (!open_ || instance_ == 0) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  try {
    return ::process_events(instance_);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

void webgpu_device::close() noexcept {
  if (open_ && instance_ != 0) {
    try {
      ::destroy_backend(instance_);
    } catch (...) {
    }
  }
  instance_ = 0;
  open_ = false;
}

} // namespace granit::detail
