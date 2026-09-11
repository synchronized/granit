// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "backend/webgpu/device.h"
#include "backend/contracts/callback_lifetime.h"
#include "backend/contracts/lifecycle.h"
#include "backend/webgpu/device_state.h"
#include "backend/webgpu/device_utils.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <mutex>
#include <new>
#include <string>
#include <unordered_map>
#include <vector>

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

struct timestamp_map_request {
  std::shared_ptr<webgpu_device_state::timestamp_query_record> query;
};

struct pipeline_warmup_request {
  std::shared_ptr<webgpu_device_state::pipeline_warmup_record> warmup;
  webgpu_host_api host{};
  WGPUInstance instance{};
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

void receive_timestamp_map(WGPUMapAsyncStatus status, WGPUStringView, void* data, void*) noexcept {
  std::unique_ptr<timestamp_map_request> request{static_cast<timestamp_map_request*>(data)};
  auto& query = *request->query;
  if (status != WGPUMapAsyncStatus_Success) {
    query.map_state.store(3, std::memory_order_release);
    return;
  }
  const auto size = static_cast<std::size_t>(query.count) * sizeof(std::uint64_t);
  const auto* mapped = wgpuBufferGetConstMappedRange(query.read_buffer, 0, size);
  if (mapped == nullptr) {
    query.map_state.store(3, std::memory_order_release);
    return;
  }
  std::memcpy(query.values.data(), mapped, size);
  wgpuBufferUnmap(query.read_buffer);
  query.map_state.store(2, std::memory_order_release);
}

granit_result pipeline_warmup_result(WGPUCreatePipelineAsyncStatus status) noexcept {
  switch (status) {
  case WGPUCreatePipelineAsyncStatus_Success:
    return GRANIT_SUCCESS;
  case WGPUCreatePipelineAsyncStatus_CallbackCancelled:
    return GRANIT_ERROR_CANCELLED;
  case WGPUCreatePipelineAsyncStatus_ValidationError:
    return GRANIT_ERROR_INVALID_ARGUMENT;
  default:
    return GRANIT_ERROR_INTERNAL;
  }
}

void receive_render_pipeline_warmup(WGPUCreatePipelineAsyncStatus status,
                                    WGPURenderPipeline pipeline, WGPUStringView message, void* data,
                                    void*) noexcept {
  std::unique_ptr<pipeline_warmup_request> request{static_cast<pipeline_warmup_request*>(data)};
  if (pipeline != nullptr)
    wgpuRenderPipelineRelease(pipeline);
  if (status != WGPUCreatePipelineAsyncStatus_Success) {
    emit_dawn_message(&request->host, message);
    char fallback[96]{};
    const auto length = std::snprintf(fallback, sizeof(fallback),
                                      "WebGPU render pipeline warmup failed with status %u",
                                      static_cast<unsigned>(status));
    if (length > 0)
      emit(request->host, GRANIT_DIAGNOSTIC_SEVERITY_ERROR, fallback,
           static_cast<std::uint32_t>(length));
  }
  request->warmup->result.store(pipeline_warmup_result(status), std::memory_order_release);
  wgpuInstanceRelease(request->instance);
}

void receive_compute_pipeline_warmup(WGPUCreatePipelineAsyncStatus status,
                                     WGPUComputePipeline pipeline, WGPUStringView message,
                                     void* data, void*) noexcept {
  std::unique_ptr<pipeline_warmup_request> request{static_cast<pipeline_warmup_request*>(data)};
  if (pipeline != nullptr)
    wgpuComputePipelineRelease(pipeline);
  if (status != WGPUCreatePipelineAsyncStatus_Success) {
    emit_dawn_message(&request->host, message);
    char fallback[96]{};
    const auto length = std::snprintf(fallback, sizeof(fallback),
                                      "WebGPU compute pipeline warmup failed with status %u",
                                      static_cast<unsigned>(status));
    if (length > 0)
      emit(request->host, GRANIT_DIAGNOSTIC_SEVERITY_ERROR, fallback,
           static_cast<std::uint32_t>(length));
  }
  request->warmup->result.store(pipeline_warmup_result(status), std::memory_order_release);
  wgpuInstanceRelease(request->instance);
}

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

bool valid_transfer_recorder(
    const webgpu_device_state::command_recorder_record& recorder) noexcept {
  return !recorder.finished && recorder.pass == nullptr && recorder.compute_pass == nullptr;
}

bool valid_texture_buffer_copy(const webgpu_device_state::texture_record& texture,
                               const webgpu_device_state::buffer_record& buffer,
                               const webgpu_texture_buffer_copy& region,
                               bool buffer_is_source) noexcept {
  if (region.width == 0 || region.height == 0 || region.depth == 0 ||
      region.array_layer_count == 0 || region.mip_level >= texture.mip_level_count ||
      region.base_array_layer >= texture.array_layer_count ||
      region.array_layer_count > texture.array_layer_count - region.base_array_layer ||
      map_texture_aspect(region.aspect) == WGPUTextureAspect_Undefined)
    return false;
  const auto mip_width = (std::max)(UINT32_C(1), texture.width >> region.mip_level);
  const auto mip_height = (std::max)(UINT32_C(1), texture.height >> region.mip_level);
  if (region.x >= mip_width || region.width > mip_width - region.x || region.y >= mip_height ||
      region.height > mip_height - region.y || region.z != 0 || region.depth != 1)
    return false;
  const auto block = texture_block(texture.format);
  if (block.bytes == 0)
    return false;
  const std::uint64_t columns = (std::uint64_t{region.width} + block.width - 1) / block.width;
  const std::uint64_t rows = (std::uint64_t{region.height} + block.height - 1) / block.height;
  const std::uint64_t tight_row = columns * block.bytes;
  if (!valid_texture_block_region(texture.format, region.x, region.y, region.width, region.height,
                                  mip_width, mip_height) ||
      region.bytes_per_row < tight_row || region.bytes_per_row % block.bytes != 0 ||
      region.rows_per_image < rows)
    return false;
  const auto max = (std::numeric_limits<std::uint64_t>::max)();
  if (region.rows_per_image > max / region.bytes_per_row)
    return false;
  const auto image_pitch = std::uint64_t{region.rows_per_image} * region.bytes_per_row;
  if (region.array_layer_count - 1 > max / image_pitch || rows - 1 > max / region.bytes_per_row)
    return false;
  const auto required = std::uint64_t{region.array_layer_count - 1} * image_pitch +
                        (rows - 1) * region.bytes_per_row + tight_row;
  if (region.buffer_offset > buffer.size || required > buffer.size - region.buffer_offset)
    return false;
  const auto required_buffer_usage = buffer_is_source ? GRANIT_WEBGPU_BUFFER_USAGE_COPY_SRC_BIT
                                                      : GRANIT_WEBGPU_BUFFER_USAGE_COPY_DST_BIT;
  const auto required_texture_usage = buffer_is_source ? GRANIT_WEBGPU_TEXTURE_USAGE_COPY_DST_BIT
                                                       : GRANIT_WEBGPU_TEXTURE_USAGE_COPY_SRC_BIT;
  return (buffer.usage & required_buffer_usage) != 0 &&
         (texture.usage & required_texture_usage) != 0;
}

granit_result create_shader(webgpu_instance_handle instance, const webgpu_shader_desc* desc,
                            webgpu_shader* out_shader) noexcept {
  if (out_shader != nullptr)
    *out_shader = 0;
  if (instance == 0 || desc == nullptr || out_shader == nullptr ||
      desc->struct_size < sizeof(*desc) || desc->wgsl == nullptr || desc->wgsl_length == 0 ||
      desc->entry_point == nullptr || desc->entry_point_length == 0 ||
      (desc->stage != GRANIT_WEBGPU_SHADER_STAGE_VERTEX &&
       desc->stage != GRANIT_WEBGPU_SHADER_STAGE_FRAGMENT &&
       desc->stage != GRANIT_WEBGPU_SHADER_STAGE_COMPUTE))
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
  const auto handle = next_handle<webgpu_shader>(next_shader);
  try {
    webgpu_device_state::shader_record record{
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

granit_result destroy_shader(webgpu_instance_handle instance, webgpu_shader shader) noexcept {
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

granit_result create_pipeline_layout(webgpu_instance_handle instance,
                                     const webgpu_pipeline_layout_desc* desc,
                                     webgpu_pipeline_layout* out_pipeline_layout) noexcept {
  if (out_pipeline_layout != nullptr)
    *out_pipeline_layout = 0;
  if (instance == 0 || desc == nullptr || out_pipeline_layout == nullptr ||
      desc->struct_size < sizeof(webgpu_pipeline_layout_desc) || desc->reserved != 0 ||
      desc->bind_group_layout_count > 8 ||
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
    std::vector<webgpu_bind_group_layout> dependencies;
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
    const auto handle = next_handle<webgpu_pipeline_layout>(next_pipeline_layout);
    try {
      webgpu_device_state::pipeline_layout_record record{native, std::move(dependencies)};
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

granit_result destroy_pipeline_layout(webgpu_instance_handle instance,
                                      webgpu_pipeline_layout layout) noexcept {
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

WGPUVertexFormat to_vertex_format(webgpu_vertex_format format) noexcept {
  switch (format) {
  case GRANIT_WEBGPU_VERTEX_FORMAT_FLOAT32:
    return WGPUVertexFormat_Float32;
  case GRANIT_WEBGPU_VERTEX_FORMAT_FLOAT32X2:
    return WGPUVertexFormat_Float32x2;
  case GRANIT_WEBGPU_VERTEX_FORMAT_FLOAT32X3:
    return WGPUVertexFormat_Float32x3;
  case GRANIT_WEBGPU_VERTEX_FORMAT_FLOAT32X4:
    return WGPUVertexFormat_Float32x4;
  case GRANIT_WEBGPU_VERTEX_FORMAT_UINT32:
    return WGPUVertexFormat_Uint32;
  case GRANIT_WEBGPU_VERTEX_FORMAT_UINT32X2:
    return WGPUVertexFormat_Uint32x2;
  case GRANIT_WEBGPU_VERTEX_FORMAT_UINT32X3:
    return WGPUVertexFormat_Uint32x3;
  case GRANIT_WEBGPU_VERTEX_FORMAT_UINT32X4:
    return WGPUVertexFormat_Uint32x4;
  case GRANIT_WEBGPU_VERTEX_FORMAT_SINT32:
    return WGPUVertexFormat_Sint32;
  case GRANIT_WEBGPU_VERTEX_FORMAT_SINT32X2:
    return WGPUVertexFormat_Sint32x2;
  case GRANIT_WEBGPU_VERTEX_FORMAT_SINT32X3:
    return WGPUVertexFormat_Sint32x3;
  case GRANIT_WEBGPU_VERTEX_FORMAT_SINT32X4:
    return WGPUVertexFormat_Sint32x4;
  default:
    return static_cast<WGPUVertexFormat>(0);
  }
}

WGPUFrontFace to_native_front_face(webgpu_front_face front_face) noexcept {
  // Granit 的正面绕序以 Vulkan 正高度 Viewport 为基准；WebGPU 窗口映射的 Y 方向相反。
  return front_face == GRANIT_WEBGPU_FRONT_FACE_COUNTER_CLOCKWISE ? WGPUFrontFace_CW
                                                                  : WGPUFrontFace_CCW;
}

WGPUCullMode to_native_cull_mode(webgpu_cull_mode cull_mode) noexcept {
  if (cull_mode == GRANIT_WEBGPU_CULL_MODE_NONE)
    return WGPUCullMode_None;
  return cull_mode == GRANIT_WEBGPU_CULL_MODE_FRONT ? WGPUCullMode_Front : WGPUCullMode_Back;
}

std::uint32_t vertex_format_size(webgpu_vertex_format format) noexcept {
  if (format == GRANIT_WEBGPU_VERTEX_FORMAT_FLOAT32 ||
      format == GRANIT_WEBGPU_VERTEX_FORMAT_UINT32 || format == GRANIT_WEBGPU_VERTEX_FORMAT_SINT32)
    return 4;
  if (format == GRANIT_WEBGPU_VERTEX_FORMAT_FLOAT32X2 ||
      format == GRANIT_WEBGPU_VERTEX_FORMAT_UINT32X2 ||
      format == GRANIT_WEBGPU_VERTEX_FORMAT_SINT32X2)
    return 8;
  if (format == GRANIT_WEBGPU_VERTEX_FORMAT_FLOAT32X3 ||
      format == GRANIT_WEBGPU_VERTEX_FORMAT_UINT32X3 ||
      format == GRANIT_WEBGPU_VERTEX_FORMAT_SINT32X3)
    return 12;
  if (format == GRANIT_WEBGPU_VERTEX_FORMAT_FLOAT32X4 ||
      format == GRANIT_WEBGPU_VERTEX_FORMAT_UINT32X4 ||
      format == GRANIT_WEBGPU_VERTEX_FORMAT_SINT32X4)
    return 16;
  return 0;
}

granit_result create_render_pipeline_common(webgpu_instance_handle instance,
                                            const webgpu_render_pipeline_desc* desc,
                                            webgpu_render_pipeline* out_render_pipeline,
                                            webgpu_pipeline_warmup* out_warmup) noexcept {
  if (out_render_pipeline != nullptr)
    *out_render_pipeline = 0;
  if (out_warmup != nullptr)
    *out_warmup = 0;
  if (instance == 0 || desc == nullptr ||
      ((out_render_pipeline == nullptr) == (out_warmup == nullptr)) ||
      desc->struct_size < sizeof(*desc) || desc->reserved != 0 || desc->layout == 0 ||
      desc->vertex_shader == 0 || desc->fragment_shader == 0 ||
      (desc->vertex_buffer_layout_count != 0 && desc->vertex_buffer_layouts == nullptr) ||
      (desc->color_format != 0 && desc->color_format != GRANIT_WEBGPU_TEXTURE_FORMAT_RGBA8_UNORM &&
       desc->color_format != GRANIT_WEBGPU_TEXTURE_FORMAT_BGRA8_UNORM &&
       desc->color_format != GRANIT_WEBGPU_TEXTURE_FORMAT_RGBA16_FLOAT) ||
      (desc->color_format == 0 && desc->depth_stencil_format == 0) ||
      (desc->depth_stencil_format != 0 &&
       desc->depth_stencil_format != GRANIT_WEBGPU_TEXTURE_FORMAT_D32_FLOAT) ||
      desc->depth_test_enabled > 1 || desc->depth_write_enabled > 1 || desc->blend_enabled > 1 ||
      (desc->color_write_mask & ~GRANIT_WEBGPU_COLOR_WRITE_ALL_BITS) != 0 ||
      (desc->blend_enabled != 0 &&
       (to_native_blend_factor(desc->source_color_factor) == WGPUBlendFactor_Undefined ||
        to_native_blend_factor(desc->destination_color_factor) == WGPUBlendFactor_Undefined ||
        to_native_blend_operation(desc->color_operation) == WGPUBlendOperation_Undefined ||
        to_native_blend_factor(desc->source_alpha_factor) == WGPUBlendFactor_Undefined ||
        to_native_blend_factor(desc->destination_alpha_factor) == WGPUBlendFactor_Undefined ||
        to_native_blend_operation(desc->alpha_operation) == WGPUBlendOperation_Undefined)) ||
      (desc->depth_stencil_format == 0 &&
       (desc->depth_test_enabled != 0 || desc->depth_write_enabled != 0)) ||
      (desc->depth_test_enabled != 0 &&
       to_native_compare_operation(desc->depth_compare) == WGPUCompareFunction_Undefined) ||
      desc->topology != GRANIT_WEBGPU_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST ||
      (desc->front_face != GRANIT_WEBGPU_FRONT_FACE_COUNTER_CLOCKWISE &&
       desc->front_face != GRANIT_WEBGPU_FRONT_FACE_CLOCKWISE) ||
      (desc->cull_mode != GRANIT_WEBGPU_CULL_MODE_NONE &&
       desc->cull_mode != GRANIT_WEBGPU_CULL_MODE_FRONT &&
       desc->cull_mode != GRANIT_WEBGPU_CULL_MODE_BACK) ||
      desc->polygon_mode != GRANIT_WEBGPU_POLYGON_MODE_FILL ||
      (desc->sample_count != 1 && desc->sample_count != 4))
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
  if (vertex->second.stage != GRANIT_WEBGPU_SHADER_STAGE_VERTEX ||
      fragment_shader->second.stage != GRANIT_WEBGPU_SHADER_STAGE_FRAGMENT)
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
          (source.step_mode != GRANIT_WEBGPU_VERTEX_STEP_MODE_VERTEX &&
           source.step_mode != GRANIT_WEBGPU_VERTEX_STEP_MODE_INSTANCE))
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
      native_layout.stepMode = source.step_mode == GRANIT_WEBGPU_VERTEX_STEP_MODE_VERTEX
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
  target.format = to_native_texture_format(desc->color_format);
  target.writeMask = static_cast<WGPUColorWriteMask>(desc->color_write_mask);
  WGPUBlendState blend = WGPU_BLEND_STATE_INIT;
  if (desc->blend_enabled != 0) {
    blend.color.srcFactor = to_native_blend_factor(desc->source_color_factor);
    blend.color.dstFactor = to_native_blend_factor(desc->destination_color_factor);
    blend.color.operation = to_native_blend_operation(desc->color_operation);
    blend.alpha.srcFactor = to_native_blend_factor(desc->source_alpha_factor);
    blend.alpha.dstFactor = to_native_blend_factor(desc->destination_alpha_factor);
    blend.alpha.operation = to_native_blend_operation(desc->alpha_operation);
    target.blend = &blend;
  }
  WGPUFragmentState fragment = WGPU_FRAGMENT_STATE_INIT;
  fragment.module = fragment_shader->second.shader;
  fragment.entryPoint = {fragment_shader->second.entry_point.data(),
                         fragment_shader->second.entry_point.size()};
  fragment.targetCount = desc->color_format == 0 ? 0 : 1;
  fragment.targets = desc->color_format == 0 ? nullptr : &target;
  WGPURenderPipelineDescriptor descriptor = WGPU_RENDER_PIPELINE_DESCRIPTOR_INIT;
  descriptor.layout = layout->second.pipeline_layout;
  descriptor.vertex.module = vertex->second.shader;
  descriptor.vertex.entryPoint = {vertex->second.entry_point.data(),
                                  vertex->second.entry_point.size()};
  descriptor.vertex.bufferCount = vertex_buffers.size();
  descriptor.vertex.buffers = vertex_buffers.data();
  descriptor.primitive.topology = WGPUPrimitiveTopology_TriangleList;
  descriptor.primitive.frontFace = to_native_front_face(desc->front_face);
  descriptor.primitive.cullMode = to_native_cull_mode(desc->cull_mode);
  descriptor.multisample.count = desc->sample_count;
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
  if (out_warmup != nullptr) {
    std::shared_ptr<webgpu_device_state::pipeline_warmup_record> record;
    try {
      record = std::make_shared<webgpu_device_state::pipeline_warmup_record>();
      const auto handle = next_handle<webgpu_pipeline_warmup>(next_pipeline_warmup);
      if (!state.pipeline_warmups.emplace(handle, record).second)
        return GRANIT_ERROR_INTERNAL;
      auto* request =
          new (std::nothrow) pipeline_warmup_request{record, state.host, state.instance};
      if (request == nullptr) {
        state.pipeline_warmups.erase(handle);
        return GRANIT_ERROR_OUT_OF_MEMORY;
      }
      wgpuInstanceAddRef(state.instance);
      WGPUCreateRenderPipelineAsyncCallbackInfo callback =
          WGPU_CREATE_RENDER_PIPELINE_ASYNC_CALLBACK_INFO_INIT;
#if defined(__EMSCRIPTEN__)
      callback.mode = WGPUCallbackMode_WaitAnyOnly;
#else
      callback.mode = WGPUCallbackMode_AllowSpontaneous;
#endif
      callback.callback = receive_render_pipeline_warmup;
      callback.userdata1 = request;
      record->future = wgpuDeviceCreateRenderPipelineAsync(state.device, &descriptor, callback);
      *out_warmup = handle;
      return GRANIT_SUCCESS;
    } catch (const std::bad_alloc&) {
      return GRANIT_ERROR_OUT_OF_MEMORY;
    } catch (...) {
      return GRANIT_ERROR_INTERNAL;
    }
  }
  const auto native = wgpuDeviceCreateRenderPipeline(state.device, &descriptor);
  if (native == nullptr)
    return GRANIT_ERROR_INITIALIZATION_FAILED;
  const auto handle = next_handle<webgpu_render_pipeline>(next_render_pipeline);
  try {
    const auto record = webgpu_device_state::render_pipeline_record{
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

granit_result create_render_pipeline(webgpu_instance_handle instance,
                                     const webgpu_render_pipeline_desc* desc,
                                     webgpu_render_pipeline* out_render_pipeline) noexcept {
  return create_render_pipeline_common(instance, desc, out_render_pipeline, nullptr);
}

granit_result begin_render_pipeline_warmup(webgpu_instance_handle instance,
                                           const webgpu_render_pipeline_desc* desc,
                                           webgpu_pipeline_warmup* warmup) noexcept {
  return create_render_pipeline_common(instance, desc, nullptr, warmup);
}

granit_result destroy_render_pipeline(webgpu_instance_handle instance,
                                      webgpu_render_pipeline pipeline) noexcept {
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

granit_result create_compute_pipeline_common(webgpu_instance_handle instance,
                                             const webgpu_compute_pipeline_desc* desc,
                                             webgpu_compute_pipeline* out_pipeline,
                                             webgpu_pipeline_warmup* out_warmup) noexcept {
  if (out_pipeline != nullptr)
    *out_pipeline = 0;
  if (out_warmup != nullptr)
    *out_warmup = 0;
  if (instance == 0 || desc == nullptr || ((out_pipeline == nullptr) == (out_warmup == nullptr)) ||
      desc->struct_size < sizeof(webgpu_compute_pipeline_desc) || desc->reserved != 0 ||
      desc->layout == 0 || desc->shader == 0)
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
  if (shader->second.stage != GRANIT_WEBGPU_SHADER_STAGE_COMPUTE)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  WGPUComputePipelineDescriptor descriptor = WGPU_COMPUTE_PIPELINE_DESCRIPTOR_INIT;
  descriptor.layout = layout->second.pipeline_layout;
  descriptor.compute.module = shader->second.shader;
  descriptor.compute.entryPoint = {shader->second.entry_point.data(),
                                   shader->second.entry_point.size()};
  if (out_warmup != nullptr) {
    try {
      auto record = std::make_shared<webgpu_device_state::pipeline_warmup_record>();
      const auto handle = next_handle<webgpu_pipeline_warmup>(next_pipeline_warmup);
      if (!state.pipeline_warmups.emplace(handle, record).second)
        return GRANIT_ERROR_INTERNAL;
      auto* request =
          new (std::nothrow) pipeline_warmup_request{record, state.host, state.instance};
      if (request == nullptr) {
        state.pipeline_warmups.erase(handle);
        return GRANIT_ERROR_OUT_OF_MEMORY;
      }
      wgpuInstanceAddRef(state.instance);
      WGPUCreateComputePipelineAsyncCallbackInfo callback =
          WGPU_CREATE_COMPUTE_PIPELINE_ASYNC_CALLBACK_INFO_INIT;
#if defined(__EMSCRIPTEN__)
      callback.mode = WGPUCallbackMode_WaitAnyOnly;
#else
      callback.mode = WGPUCallbackMode_AllowSpontaneous;
#endif
      callback.callback = receive_compute_pipeline_warmup;
      callback.userdata1 = request;
      record->future = wgpuDeviceCreateComputePipelineAsync(state.device, &descriptor, callback);
      *out_warmup = handle;
      return GRANIT_SUCCESS;
    } catch (const std::bad_alloc&) {
      return GRANIT_ERROR_OUT_OF_MEMORY;
    } catch (...) {
      return GRANIT_ERROR_INTERNAL;
    }
  }
  const auto native = wgpuDeviceCreateComputePipeline(state.device, &descriptor);
  if (native == nullptr)
    return GRANIT_ERROR_OUT_OF_MEMORY;
  const auto handle = next_handle<webgpu_compute_pipeline>(next_compute_pipeline);
  try {
    const webgpu_device_state::compute_pipeline_record record{native, desc->layout, desc->shader};
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

granit_result create_compute_pipeline(webgpu_instance_handle instance,
                                      const webgpu_compute_pipeline_desc* desc,
                                      webgpu_compute_pipeline* out_pipeline) noexcept {
  return create_compute_pipeline_common(instance, desc, out_pipeline, nullptr);
}

granit_result begin_compute_pipeline_warmup(webgpu_instance_handle instance,
                                            const webgpu_compute_pipeline_desc* desc,
                                            webgpu_pipeline_warmup* warmup) noexcept {
  return create_compute_pipeline_common(instance, desc, nullptr, warmup);
}

granit_result destroy_compute_pipeline(webgpu_instance_handle instance,
                                       webgpu_compute_pipeline pipeline) noexcept {
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

granit_result poll_pipeline_warmup(webgpu_instance_handle instance,
                                   webgpu_pipeline_warmup warmup) noexcept {
  if (instance == 0 || warmup == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const std::scoped_lock lock{instances_mutex};
  const auto found = instances.find(instance);
  if (found == instances.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  const auto operation = found->second->pipeline_warmups.find(warmup);
  if (operation == found->second->pipeline_warmups.end())
    return GRANIT_ERROR_INVALID_HANDLE;
#if defined(__EMSCRIPTEN__)
  if (operation->second->result.load(std::memory_order_acquire) == GRANIT_ERROR_NOT_READY) {
    WGPUFutureWaitInfo wait_info{operation->second->future, WGPU_FALSE};
    static_cast<void>(wgpuInstanceWaitAny(found->second->instance, 1, &wait_info, 0));
  }
#endif
  return operation->second->result.load(std::memory_order_acquire);
}

granit_result destroy_pipeline_warmup(webgpu_instance_handle instance,
                                      webgpu_pipeline_warmup warmup) noexcept {
  if (instance == 0 || warmup == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const std::scoped_lock lock{instances_mutex};
  const auto found = instances.find(instance);
  if (found == instances.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  return found->second->pipeline_warmups.erase(warmup) == 1 ? GRANIT_SUCCESS
                                                            : GRANIT_ERROR_INVALID_HANDLE;
}

granit_result create_command_recorder(webgpu_instance_handle instance,
                                      webgpu_command_recorder* out_recorder) noexcept {
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
  const auto handle = next_handle<webgpu_command_recorder>(next_command_recorder);
  try {
    const auto record = webgpu_device_state::command_recorder_record{
        native, nullptr, nullptr, false, false, false, 0, 0, {}, {}};
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

granit_result destroy_command_recorder(webgpu_instance_handle instance,
                                       webgpu_command_recorder recorder) noexcept {
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
  for (const auto buffer : recorder_found->second.temporary_buffers)
    wgpuBufferRelease(buffer);
  wgpuCommandEncoderRelease(recorder_found->second.encoder);
  found->second->command_recorders.erase(recorder_found);
  return GRANIT_SUCCESS;
}

granit_result recorder_copy_buffer_to_texture(webgpu_instance_handle instance,
                                              webgpu_command_recorder recorder,
                                              webgpu_buffer buffer, webgpu_texture texture,
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
  if ((buffer_found->second.usage & GRANIT_WEBGPU_BUFFER_USAGE_COPY_SRC_BIT) == 0 ||
      (texture_found->second.usage & GRANIT_WEBGPU_TEXTURE_USAGE_COPY_DST_BIT) == 0 ||
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

granit_result recorder_copy_buffer(webgpu_instance_handle instance,
                                   webgpu_command_recorder recorder, webgpu_buffer source,
                                   webgpu_buffer destination,
                                   const webgpu_buffer_copy_region* regions,
                                   std::uint32_t region_count) noexcept {
  if (instance == 0 || recorder == 0 || source == 0 || destination == 0 || regions == nullptr ||
      region_count == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const std::scoped_lock lock{instances_mutex};
  const auto found = instances.find(instance);
  if (found == instances.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  if (const auto ready = require_ready(*found->second); ready != GRANIT_SUCCESS)
    return ready;
  auto& state = *found->second;
  const auto command = state.command_recorders.find(recorder);
  const auto source_buffer = state.buffers.find(source);
  const auto destination_buffer = state.buffers.find(destination);
  if (command == state.command_recorders.end() || source_buffer == state.buffers.end() ||
      destination_buffer == state.buffers.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  if (!valid_transfer_recorder(command->second) ||
      (source_buffer->second.usage & GRANIT_WEBGPU_BUFFER_USAGE_COPY_SRC_BIT) == 0 ||
      (destination_buffer->second.usage & GRANIT_WEBGPU_BUFFER_USAGE_COPY_DST_BIT) == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  for (std::uint32_t index = 0; index < region_count; ++index) {
    const auto& region = regions[index];
    if (region.size == 0 || region.source_offset % 4 != 0 || region.destination_offset % 4 != 0 ||
        region.size % 4 != 0 || region.source_offset > source_buffer->second.size ||
        region.size > source_buffer->second.size - region.source_offset ||
        region.destination_offset > destination_buffer->second.size ||
        region.size > destination_buffer->second.size - region.destination_offset)
      return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  for (std::uint32_t index = 0; index < region_count; ++index) {
    const auto& region = regions[index];
    wgpuCommandEncoderCopyBufferToBuffer(command->second.encoder, source_buffer->second.buffer,
                                         region.source_offset, destination_buffer->second.buffer,
                                         region.destination_offset, region.size);
  }
  return GRANIT_SUCCESS;
}

granit_result recorder_copy_buffer_to_texture_v2(
    webgpu_instance_handle instance, webgpu_command_recorder recorder, webgpu_buffer source,
    webgpu_texture destination, const webgpu_texture_buffer_copy* region) noexcept {
  if (instance == 0 || recorder == 0 || source == 0 || destination == 0 || region == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const std::scoped_lock lock{instances_mutex};
  const auto found = instances.find(instance);
  if (found == instances.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  if (const auto ready = require_ready(*found->second); ready != GRANIT_SUCCESS)
    return ready;
  auto& state = *found->second;
  const auto command = state.command_recorders.find(recorder);
  const auto buffer = state.buffers.find(source);
  const auto texture = state.textures.find(destination);
  if (command == state.command_recorders.end() || buffer == state.buffers.end() ||
      texture == state.textures.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  if (!valid_transfer_recorder(command->second) ||
      !valid_texture_buffer_copy(texture->second, buffer->second, *region, true))
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const auto encode = [&](std::uint64_t offset, std::uint32_t y, std::uint32_t layer,
                          std::uint32_t height, std::uint32_t layers, bool omit_strides) {
    WGPUTexelCopyBufferInfo native_source = WGPU_TEXEL_COPY_BUFFER_INFO_INIT;
    native_source.buffer = buffer->second.buffer;
    native_source.layout.offset = offset;
    native_source.layout.bytesPerRow =
        omit_strides ? WGPU_COPY_STRIDE_UNDEFINED : region->bytes_per_row;
    native_source.layout.rowsPerImage =
        omit_strides ? WGPU_COPY_STRIDE_UNDEFINED : region->rows_per_image;
    WGPUTexelCopyTextureInfo native_destination = WGPU_TEXEL_COPY_TEXTURE_INFO_INIT;
    native_destination.texture = texture->second.texture;
    native_destination.mipLevel = region->mip_level;
    native_destination.origin = {region->x, y, layer};
    native_destination.aspect = map_texture_aspect(region->aspect);
    const WGPUExtent3D extent{region->width, height, layers};
    wgpuCommandEncoderCopyBufferToTexture(command->second.encoder, &native_source,
                                          &native_destination, &extent);
  };
  if (region->bytes_per_row % 256 == 0) {
    encode(region->buffer_offset, region->y, region->base_array_layer, region->height,
           region->array_layer_count, false);
  } else {
    const auto block = texture_block(texture->second.format);
    const auto block_rows = (region->height + block.height - 1) / block.height;
    const auto image_pitch = std::uint64_t{region->rows_per_image} * region->bytes_per_row;
    for (std::uint32_t layer = 0; layer < region->array_layer_count; ++layer) {
      for (std::uint32_t row = 0; row < block_rows; ++row) {
        const auto y = region->y + row * block.height;
        const auto height = (std::min)(block.height, region->y + region->height - y);
        encode(region->buffer_offset + std::uint64_t{layer} * image_pitch +
                   std::uint64_t{row} * region->bytes_per_row,
               y, region->base_array_layer + layer, height, 1, true);
      }
    }
  }
  return GRANIT_SUCCESS;
}

granit_result recorder_copy_texture_to_buffer_v2(
    webgpu_instance_handle instance, webgpu_command_recorder recorder, webgpu_texture source,
    webgpu_buffer destination, const webgpu_texture_buffer_copy* region) noexcept {
  if (instance == 0 || recorder == 0 || source == 0 || destination == 0 || region == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const std::scoped_lock lock{instances_mutex};
  const auto found = instances.find(instance);
  if (found == instances.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  if (const auto ready = require_ready(*found->second); ready != GRANIT_SUCCESS)
    return ready;
  auto& state = *found->second;
  const auto command = state.command_recorders.find(recorder);
  const auto texture = state.textures.find(source);
  const auto buffer = state.buffers.find(destination);
  if (command == state.command_recorders.end() || texture == state.textures.end() ||
      buffer == state.buffers.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  if (!valid_transfer_recorder(command->second) ||
      !valid_texture_buffer_copy(texture->second, buffer->second, *region, false))
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const auto encode = [&](std::uint64_t offset, std::uint32_t y, std::uint32_t layer,
                          std::uint32_t height, std::uint32_t layers, bool omit_strides) {
    WGPUTexelCopyTextureInfo native_source = WGPU_TEXEL_COPY_TEXTURE_INFO_INIT;
    native_source.texture = texture->second.texture;
    native_source.mipLevel = region->mip_level;
    native_source.origin = {region->x, y, layer};
    native_source.aspect = map_texture_aspect(region->aspect);
    WGPUTexelCopyBufferInfo native_destination = WGPU_TEXEL_COPY_BUFFER_INFO_INIT;
    native_destination.buffer = buffer->second.buffer;
    native_destination.layout.offset = offset;
    native_destination.layout.bytesPerRow =
        omit_strides ? WGPU_COPY_STRIDE_UNDEFINED : region->bytes_per_row;
    native_destination.layout.rowsPerImage =
        omit_strides ? WGPU_COPY_STRIDE_UNDEFINED : region->rows_per_image;
    const WGPUExtent3D extent{region->width, height, layers};
    wgpuCommandEncoderCopyTextureToBuffer(command->second.encoder, &native_source,
                                          &native_destination, &extent);
  };
  if (region->bytes_per_row % 256 == 0) {
    encode(region->buffer_offset, region->y, region->base_array_layer, region->height,
           region->array_layer_count, false);
  } else {
    const auto block = texture_block(texture->second.format);
    const auto block_rows = (region->height + block.height - 1) / block.height;
    const auto image_pitch = std::uint64_t{region->rows_per_image} * region->bytes_per_row;
    for (std::uint32_t layer = 0; layer < region->array_layer_count; ++layer) {
      for (std::uint32_t row = 0; row < block_rows; ++row) {
        const auto y = region->y + row * block.height;
        const auto height = (std::min)(block.height, region->y + region->height - y);
        encode(region->buffer_offset + std::uint64_t{layer} * image_pitch +
                   std::uint64_t{row} * region->bytes_per_row,
               y, region->base_array_layer + layer, height, 1, true);
      }
    }
  }
  return GRANIT_SUCCESS;
}

granit_result recorder_copy_texture(webgpu_instance_handle instance,
                                    webgpu_command_recorder recorder, webgpu_texture source,
                                    webgpu_texture destination,
                                    const webgpu_texture_copy_region* region) noexcept {
  if (instance == 0 || recorder == 0 || source == 0 || destination == 0 || region == nullptr ||
      region->width == 0 || region->height == 0 || region->depth == 0 ||
      region->array_layer_count == 0 ||
      map_texture_aspect(region->aspect) == WGPUTextureAspect_Undefined)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const std::scoped_lock lock{instances_mutex};
  const auto found = instances.find(instance);
  if (found == instances.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  if (const auto ready = require_ready(*found->second); ready != GRANIT_SUCCESS)
    return ready;
  auto& state = *found->second;
  const auto command = state.command_recorders.find(recorder);
  const auto source_texture = state.textures.find(source);
  const auto destination_texture = state.textures.find(destination);
  if (command == state.command_recorders.end() || source_texture == state.textures.end() ||
      destination_texture == state.textures.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  const auto& source_desc = source_texture->second;
  const auto& destination_desc = destination_texture->second;
  if (!valid_transfer_recorder(command->second) || source_desc.format != destination_desc.format ||
      (source_desc.usage & GRANIT_WEBGPU_TEXTURE_USAGE_COPY_SRC_BIT) == 0 ||
      (destination_desc.usage & GRANIT_WEBGPU_TEXTURE_USAGE_COPY_DST_BIT) == 0 ||
      region->source_mip_level >= source_desc.mip_level_count ||
      region->destination_mip_level >= destination_desc.mip_level_count ||
      region->source_base_array_layer >= source_desc.array_layer_count ||
      region->array_layer_count > source_desc.array_layer_count - region->source_base_array_layer ||
      region->destination_base_array_layer >= destination_desc.array_layer_count ||
      region->array_layer_count >
          destination_desc.array_layer_count - region->destination_base_array_layer)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const auto source_width = (std::max)(UINT32_C(1), source_desc.width >> region->source_mip_level);
  const auto source_height =
      (std::max)(UINT32_C(1), source_desc.height >> region->source_mip_level);
  const auto destination_width =
      (std::max)(UINT32_C(1), destination_desc.width >> region->destination_mip_level);
  const auto destination_height =
      (std::max)(UINT32_C(1), destination_desc.height >> region->destination_mip_level);
  if (region->source_x >= source_width || region->width > source_width - region->source_x ||
      region->source_y >= source_height || region->height > source_height - region->source_y ||
      region->destination_x >= destination_width ||
      region->width > destination_width - region->destination_x ||
      region->destination_y >= destination_height ||
      region->height > destination_height - region->destination_y || region->source_z != 0 ||
      region->destination_z != 0 || region->depth != 1)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  WGPUTexelCopyTextureInfo native_source = WGPU_TEXEL_COPY_TEXTURE_INFO_INIT;
  native_source.texture = source_desc.texture;
  native_source.mipLevel = region->source_mip_level;
  native_source.origin = {region->source_x, region->source_y, region->source_base_array_layer};
  native_source.aspect = map_texture_aspect(region->aspect);
  WGPUTexelCopyTextureInfo native_destination = WGPU_TEXEL_COPY_TEXTURE_INFO_INIT;
  native_destination.texture = destination_desc.texture;
  native_destination.mipLevel = region->destination_mip_level;
  native_destination.origin = {region->destination_x, region->destination_y,
                               region->destination_base_array_layer};
  native_destination.aspect = map_texture_aspect(region->aspect);
  const WGPUExtent3D extent{region->width, region->height, region->array_layer_count};
  wgpuCommandEncoderCopyTextureToTexture(command->second.encoder, &native_source,
                                         &native_destination, &extent);
  return GRANIT_SUCCESS;
}

granit_result recorder_fill_buffer(webgpu_instance_handle instance,
                                   webgpu_command_recorder recorder, webgpu_buffer buffer,
                                   std::uint64_t offset, std::uint64_t size,
                                   std::uint32_t value) noexcept {
  if (instance == 0 || recorder == 0 || buffer == 0 || size == 0 || offset % 4 != 0 ||
      size % 4 != 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const std::scoped_lock lock{instances_mutex};
  const auto found = instances.find(instance);
  if (found == instances.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  if (const auto ready = require_ready(*found->second); ready != GRANIT_SUCCESS)
    return ready;
  auto& state = *found->second;
  const auto command = state.command_recorders.find(recorder);
  const auto destination = state.buffers.find(buffer);
  if (command == state.command_recorders.end() || destination == state.buffers.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  if (!valid_transfer_recorder(command->second) || offset > destination->second.size ||
      size > destination->second.size - offset ||
      (destination->second.usage & GRANIT_WEBGPU_BUFFER_USAGE_COPY_DST_BIT) == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  if (value == 0) {
    wgpuCommandEncoderClearBuffer(command->second.encoder, destination->second.buffer, offset,
                                  size);
    return GRANIT_SUCCESS;
  }
  if (size > (std::numeric_limits<std::size_t>::max)())
    return GRANIT_ERROR_UNSUPPORTED;
  WGPUBufferDescriptor descriptor = WGPU_BUFFER_DESCRIPTOR_INIT;
  descriptor.usage = WGPUBufferUsage_CopySrc;
  descriptor.size = size;
  descriptor.mappedAtCreation = true;
  const auto staging = wgpuDeviceCreateBuffer(state.device, &descriptor);
  if (staging == nullptr)
    return GRANIT_ERROR_OUT_OF_MEMORY;
  auto* words = static_cast<std::uint32_t*>(
      wgpuBufferGetMappedRange(staging, 0, static_cast<std::size_t>(size)));
  if (words == nullptr) {
    wgpuBufferRelease(staging);
    return GRANIT_ERROR_OUT_OF_MEMORY;
  }
  std::fill_n(words, static_cast<std::size_t>(size / 4), value);
  wgpuBufferUnmap(staging);
  try {
    command->second.temporary_buffers.push_back(staging);
  } catch (const std::bad_alloc&) {
    wgpuBufferRelease(staging);
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    wgpuBufferRelease(staging);
    return GRANIT_ERROR_INTERNAL;
  }
  wgpuCommandEncoderCopyBufferToBuffer(command->second.encoder, staging, 0,
                                       destination->second.buffer, offset, size);
  return GRANIT_SUCCESS;
}

granit_result recorder_generate_mipmaps(webgpu_instance_handle instance,
                                        webgpu_command_recorder recorder, webgpu_texture texture,
                                        const webgpu_texture_mipmap_range* range) noexcept {
  if (instance == 0 || recorder == 0 || texture == 0 || range == nullptr ||
      range->level_count < 2 || range->array_layer_count == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const std::scoped_lock lock{instances_mutex};
  const auto found = instances.find(instance);
  if (found == instances.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  if (const auto ready = require_ready(*found->second); ready != GRANIT_SUCCESS)
    return ready;
  auto& state = *found->second;
  const auto command = state.command_recorders.find(recorder);
  const auto texture_found = state.textures.find(texture);
  if (command == state.command_recorders.end() || texture_found == state.textures.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  const auto& record = texture_found->second;
  if (!valid_transfer_recorder(command->second) || record.borrowed || record.sample_count != 1 ||
      record.format == GRANIT_WEBGPU_TEXTURE_FORMAT_D32_FLOAT ||
      range->base_mip_level >= record.mip_level_count ||
      range->level_count > record.mip_level_count - range->base_mip_level ||
      range->base_array_layer >= record.array_layer_count ||
      range->array_layer_count > record.array_layer_count - range->base_array_layer ||
      (record.usage & GRANIT_WEBGPU_TEXTURE_USAGE_COPY_SRC_BIT) == 0 ||
      (record.usage & GRANIT_WEBGPU_TEXTURE_USAGE_COPY_DST_BIT) == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;

  constexpr char mipmap_wgsl[] = R"(
struct vertex_output {
  @builtin(position) position: vec4f,
  @location(0) uv: vec2f,
};
@group(0) @binding(0) var source_texture: texture_2d<f32>;
@group(0) @binding(1) var source_sampler: sampler;
@vertex fn vs_main(@builtin(vertex_index) index: u32) -> vertex_output {
  let positions = array<vec2f, 3>(vec2f(-1.0, -1.0), vec2f(3.0, -1.0), vec2f(-1.0, 3.0));
  let coordinates = array<vec2f, 3>(vec2f(0.0, 1.0), vec2f(2.0, 1.0), vec2f(0.0, -1.0));
  var output: vertex_output;
  output.position = vec4f(positions[index], 0.0, 1.0);
  output.uv = coordinates[index];
  return output;
}
@fragment fn fs_main(input: vertex_output) -> @location(0) vec4f {
  return textureSampleLevel(source_texture, source_sampler, input.uv, 0.0);
})";
  WGPUShaderSourceWGSL source = WGPU_SHADER_SOURCE_WGSL_INIT;
  source.code = {mipmap_wgsl, sizeof(mipmap_wgsl) - 1};
  WGPUShaderModuleDescriptor shader_desc = WGPU_SHADER_MODULE_DESCRIPTOR_INIT;
  shader_desc.nextInChain = &source.chain;
  const auto shader = wgpuDeviceCreateShaderModule(state.device, &shader_desc);
  if (shader == nullptr)
    return GRANIT_ERROR_INITIALIZATION_FAILED;

  WGPUColorTargetState target = WGPU_COLOR_TARGET_STATE_INIT;
  target.format = to_native_texture_format(record.format);
  target.writeMask = WGPUColorWriteMask_All;
  WGPUFragmentState fragment = WGPU_FRAGMENT_STATE_INIT;
  fragment.module = shader;
  fragment.entryPoint = {"fs_main", 7};
  fragment.targetCount = 1;
  fragment.targets = &target;
  WGPURenderPipelineDescriptor pipeline_desc = WGPU_RENDER_PIPELINE_DESCRIPTOR_INIT;
  pipeline_desc.vertex.module = shader;
  pipeline_desc.vertex.entryPoint = {"vs_main", 7};
  pipeline_desc.primitive.topology = WGPUPrimitiveTopology_TriangleList;
  pipeline_desc.primitive.frontFace = WGPUFrontFace_CCW;
  pipeline_desc.primitive.cullMode = WGPUCullMode_None;
  pipeline_desc.multisample.count = 1;
  pipeline_desc.multisample.mask = UINT32_MAX;
  pipeline_desc.fragment = &fragment;
  const auto pipeline = wgpuDeviceCreateRenderPipeline(state.device, &pipeline_desc);
  if (pipeline == nullptr) {
    wgpuShaderModuleRelease(shader);
    return GRANIT_ERROR_INITIALIZATION_FAILED;
  }
  const auto bind_group_layout = wgpuRenderPipelineGetBindGroupLayout(pipeline, 0);
  WGPUSamplerDescriptor sampler_desc = WGPU_SAMPLER_DESCRIPTOR_INIT;
  sampler_desc.addressModeU = WGPUAddressMode_ClampToEdge;
  sampler_desc.addressModeV = WGPUAddressMode_ClampToEdge;
  sampler_desc.addressModeW = WGPUAddressMode_ClampToEdge;
  sampler_desc.magFilter = WGPUFilterMode_Linear;
  sampler_desc.minFilter = WGPUFilterMode_Linear;
  sampler_desc.mipmapFilter = WGPUMipmapFilterMode_Nearest;
  const auto sampler = wgpuDeviceCreateSampler(state.device, &sampler_desc);
  if (bind_group_layout == nullptr || sampler == nullptr) {
    if (sampler != nullptr)
      wgpuSamplerRelease(sampler);
    if (bind_group_layout != nullptr)
      wgpuBindGroupLayoutRelease(bind_group_layout);
    wgpuRenderPipelineRelease(pipeline);
    wgpuShaderModuleRelease(shader);
    return GRANIT_ERROR_INITIALIZATION_FAILED;
  }

  for (std::uint32_t layer = range->base_array_layer;
       layer < range->base_array_layer + range->array_layer_count; ++layer) {
    for (std::uint32_t level = range->base_mip_level + 1;
         level < range->base_mip_level + range->level_count; ++level) {
      WGPUTextureViewDescriptor source_view_desc = WGPU_TEXTURE_VIEW_DESCRIPTOR_INIT;
      source_view_desc.format = target.format;
      source_view_desc.dimension = WGPUTextureViewDimension_2D;
      source_view_desc.baseMipLevel = level - 1;
      source_view_desc.mipLevelCount = 1;
      source_view_desc.baseArrayLayer = layer;
      source_view_desc.arrayLayerCount = 1;
      source_view_desc.aspect = WGPUTextureAspect_All;
      WGPUTextureViewDescriptor destination_view_desc = source_view_desc;
      destination_view_desc.baseMipLevel = level;
      const auto source_view = wgpuTextureCreateView(record.texture, &source_view_desc);
      const auto destination_view = wgpuTextureCreateView(record.texture, &destination_view_desc);
      if (source_view == nullptr || destination_view == nullptr) {
        if (source_view != nullptr)
          wgpuTextureViewRelease(source_view);
        if (destination_view != nullptr)
          wgpuTextureViewRelease(destination_view);
        wgpuSamplerRelease(sampler);
        wgpuBindGroupLayoutRelease(bind_group_layout);
        wgpuRenderPipelineRelease(pipeline);
        wgpuShaderModuleRelease(shader);
        return GRANIT_ERROR_OUT_OF_MEMORY;
      }
      WGPUBindGroupEntry entries[2]{};
      entries[0].binding = 0;
      entries[0].textureView = source_view;
      entries[1].binding = 1;
      entries[1].sampler = sampler;
      WGPUBindGroupDescriptor bind_group_desc = WGPU_BIND_GROUP_DESCRIPTOR_INIT;
      bind_group_desc.layout = bind_group_layout;
      bind_group_desc.entryCount = 2;
      bind_group_desc.entries = entries;
      const auto bind_group = wgpuDeviceCreateBindGroup(state.device, &bind_group_desc);
      if (bind_group == nullptr) {
        wgpuTextureViewRelease(destination_view);
        wgpuTextureViewRelease(source_view);
        wgpuSamplerRelease(sampler);
        wgpuBindGroupLayoutRelease(bind_group_layout);
        wgpuRenderPipelineRelease(pipeline);
        wgpuShaderModuleRelease(shader);
        return GRANIT_ERROR_INITIALIZATION_FAILED;
      }
      WGPURenderPassColorAttachment color = WGPU_RENDER_PASS_COLOR_ATTACHMENT_INIT;
      color.view = destination_view;
      color.loadOp = WGPULoadOp_Clear;
      color.storeOp = WGPUStoreOp_Store;
      WGPURenderPassDescriptor pass_desc = WGPU_RENDER_PASS_DESCRIPTOR_INIT;
      pass_desc.colorAttachmentCount = 1;
      pass_desc.colorAttachments = &color;
      const auto pass = wgpuCommandEncoderBeginRenderPass(command->second.encoder, &pass_desc);
      if (pass == nullptr) {
        wgpuBindGroupRelease(bind_group);
        wgpuTextureViewRelease(destination_view);
        wgpuTextureViewRelease(source_view);
        wgpuSamplerRelease(sampler);
        wgpuBindGroupLayoutRelease(bind_group_layout);
        wgpuRenderPipelineRelease(pipeline);
        wgpuShaderModuleRelease(shader);
        return GRANIT_ERROR_INITIALIZATION_FAILED;
      }
      wgpuRenderPassEncoderSetPipeline(pass, pipeline);
      wgpuRenderPassEncoderSetBindGroup(pass, 0, bind_group, 0, nullptr);
      wgpuRenderPassEncoderDraw(pass, 3, 1, 0, 0);
      wgpuRenderPassEncoderEnd(pass);
      wgpuRenderPassEncoderRelease(pass);
      wgpuBindGroupRelease(bind_group);
      wgpuTextureViewRelease(destination_view);
      wgpuTextureViewRelease(source_view);
    }
  }
  wgpuSamplerRelease(sampler);
  wgpuBindGroupLayoutRelease(bind_group_layout);
  wgpuRenderPipelineRelease(pipeline);
  wgpuShaderModuleRelease(shader);
  return GRANIT_SUCCESS;
}

granit_result recorder_begin_rendering(
    webgpu_instance_handle instance, webgpu_command_recorder recorder, webgpu_texture_view target,
    webgpu_texture_view resolve_target, webgpu_load_operation load_operation,
    webgpu_store_operation store_operation, float clear_r, float clear_g, float clear_b,
    float clear_a, webgpu_texture_view depth_target, webgpu_load_operation depth_load_operation,
    webgpu_store_operation depth_store_operation, float clear_depth) noexcept {
  if (instance == 0 || recorder == 0 || (target == 0 && depth_target == 0) ||
      (resolve_target != 0 && target == 0) ||
      (target != 0 && ((load_operation != GRANIT_WEBGPU_LOAD_OPERATION_LOAD &&
                        load_operation != GRANIT_WEBGPU_LOAD_OPERATION_CLEAR) ||
                       (store_operation != GRANIT_WEBGPU_STORE_OPERATION_STORE &&
                        store_operation != GRANIT_WEBGPU_STORE_OPERATION_DISCARD))) ||
      (depth_target != 0 &&
       ((depth_load_operation != GRANIT_WEBGPU_LOAD_OPERATION_LOAD &&
         depth_load_operation != GRANIT_WEBGPU_LOAD_OPERATION_CLEAR) ||
        (depth_store_operation != GRANIT_WEBGPU_STORE_OPERATION_STORE &&
         depth_store_operation != GRANIT_WEBGPU_STORE_OPERATION_DISCARD) ||
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
  const auto resolve_view = state.texture_views.find(resolve_target);
  const auto depth_view = state.texture_views.find(depth_target);
  if (command == state.command_recorders.end() ||
      (target != 0 && view == state.texture_views.end()) ||
      (resolve_target != 0 && resolve_view == state.texture_views.end())) {
    return GRANIT_ERROR_INVALID_HANDLE;
  }
  const auto texture =
      target == 0 ? state.textures.end() : state.textures.find(view->second.texture);
  const auto resolve_texture = resolve_target == 0
                                   ? state.textures.end()
                                   : state.textures.find(resolve_view->second.texture);
  if (command->second.finished || command->second.pass != nullptr ||
      command->second.compute_pass != nullptr ||
      (target != 0 &&
       (texture == state.textures.end() ||
        (texture->second.usage & GRANIT_WEBGPU_TEXTURE_USAGE_RENDER_ATTACHMENT_BIT) == 0)) ||
      (resolve_target != 0 &&
       (resolve_texture == state.textures.end() || texture->second.sample_count != 4 ||
        resolve_texture->second.sample_count != 1 ||
        texture->second.format != resolve_texture->second.format ||
        texture->second.width != resolve_texture->second.width ||
        texture->second.height != resolve_texture->second.height ||
        (resolve_texture->second.usage & GRANIT_WEBGPU_TEXTURE_USAGE_RENDER_ATTACHMENT_BIT) ==
            0))) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  WGPURenderPassDepthStencilAttachment depth_attachment =
      WGPU_RENDER_PASS_DEPTH_STENCIL_ATTACHMENT_INIT;
  if (depth_target != 0) {
    if (depth_view == state.texture_views.end()) {
      return GRANIT_ERROR_INVALID_HANDLE;
    }
    const auto depth_texture = state.textures.find(depth_view->second.texture);
    if (depth_texture == state.textures.end() ||
        depth_texture->second.format != GRANIT_WEBGPU_TEXTURE_FORMAT_D32_FLOAT ||
        (depth_texture->second.usage & GRANIT_WEBGPU_TEXTURE_USAGE_RENDER_ATTACHMENT_BIT) == 0) {
      return GRANIT_ERROR_INVALID_ARGUMENT;
    }
    depth_attachment.view = depth_view->second.view;
    depth_attachment.depthLoadOp = depth_load_operation == GRANIT_WEBGPU_LOAD_OPERATION_LOAD
                                       ? WGPULoadOp_Load
                                       : WGPULoadOp_Clear;
    depth_attachment.depthStoreOp = depth_store_operation == GRANIT_WEBGPU_STORE_OPERATION_STORE
                                        ? WGPUStoreOp_Store
                                        : WGPUStoreOp_Discard;
    depth_attachment.depthClearValue = clear_depth;
    depth_attachment.depthReadOnly = false;
    depth_attachment.stencilLoadOp = WGPULoadOp_Undefined;
    depth_attachment.stencilStoreOp = WGPUStoreOp_Undefined;
    depth_attachment.stencilReadOnly = true;
  }
  WGPURenderPassColorAttachment color = WGPU_RENDER_PASS_COLOR_ATTACHMENT_INIT;
  if (target != 0) {
    color.view = view->second.view;
    color.resolveTarget = resolve_target == 0 ? nullptr : resolve_view->second.view;
    color.loadOp =
        load_operation == GRANIT_WEBGPU_LOAD_OPERATION_LOAD ? WGPULoadOp_Load : WGPULoadOp_Clear;
    color.storeOp = store_operation == GRANIT_WEBGPU_STORE_OPERATION_STORE ? WGPUStoreOp_Store
                                                                           : WGPUStoreOp_Discard;
    color.clearValue = {clear_r, clear_g, clear_b, clear_a};
  }
  WGPURenderPassDescriptor descriptor = WGPU_RENDER_PASS_DESCRIPTOR_INIT;
  descriptor.colorAttachmentCount = target == 0 ? 0 : 1;
  descriptor.colorAttachments = target == 0 ? nullptr : &color;
  descriptor.depthStencilAttachment = depth_target == 0 ? nullptr : &depth_attachment;
  command->second.pass = wgpuCommandEncoderBeginRenderPass(command->second.encoder, &descriptor);
  if (command->second.pass == nullptr)
    return GRANIT_ERROR_INITIALIZATION_FAILED;
  command->second.pipeline_bound = false;
  command->second.index_available = 0;
  command->second.index_element_size = 0;
  return GRANIT_SUCCESS;
}

granit_result recorder_bind_pipeline(webgpu_instance_handle instance,
                                     webgpu_command_recorder recorder,
                                     webgpu_render_pipeline pipeline) noexcept {
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

granit_result
recorder_bind_graphics_groups(webgpu_instance_handle instance, webgpu_command_recorder recorder,
                              webgpu_pipeline_layout pipeline_layout, std::uint32_t first_group,
                              const webgpu_bind_group* groups, std::uint32_t group_count,
                              const std::uint32_t* dynamic_offsets,
                              std::uint32_t dynamic_offset_count) noexcept {
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
      if (declaration.type != GRANIT_WEBGPU_BINDING_TYPE_DYNAMIC_UNIFORM_BUFFER)
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
          return declaration.type == GRANIT_WEBGPU_BINDING_TYPE_DYNAMIC_UNIFORM_BUFFER;
        }));
    wgpuRenderPassEncoderSetBindGroup(command->second.pass, first_group + group_index,
                                      group.bind_group, count,
                                      count == 0 ? nullptr : dynamic_offsets + offset_index);
    offset_index += count;
  }
  return GRANIT_SUCCESS;
}

granit_result recorder_bind_vertex_buffers(webgpu_instance_handle instance,
                                           webgpu_command_recorder recorder, std::uint32_t first,
                                           const webgpu_vertex_buffer_binding* bindings,
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
    if ((buffer->second.usage & GRANIT_WEBGPU_BUFFER_USAGE_VERTEX_BIT) == 0 ||
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

granit_result recorder_bind_index_buffer(webgpu_instance_handle instance,
                                         webgpu_command_recorder recorder, webgpu_buffer buffer,
                                         std::uint64_t offset,
                                         webgpu_index_format format) noexcept {
  if (instance == 0 || recorder == 0 || buffer == 0 ||
      (format != GRANIT_WEBGPU_INDEX_FORMAT_UINT16 && format != GRANIT_WEBGPU_INDEX_FORMAT_UINT32))
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
  const auto element_size = format == GRANIT_WEBGPU_INDEX_FORMAT_UINT16 ? 2U : 4U;
  if (command->second.pass == nullptr || command->second.finished ||
      (native->second.usage & GRANIT_WEBGPU_BUFFER_USAGE_INDEX_BIT) == 0 ||
      offset >= native->second.size || offset % element_size != 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  wgpuRenderPassEncoderSetIndexBuffer(
      command->second.pass, native->second.buffer,
      format == GRANIT_WEBGPU_INDEX_FORMAT_UINT16 ? WGPUIndexFormat_Uint16 : WGPUIndexFormat_Uint32,
      offset, native->second.size - offset);
  command->second.index_available = native->second.size - offset;
  command->second.index_element_size = element_size;
  return GRANIT_SUCCESS;
}

granit_result recorder_set_viewports(webgpu_instance_handle instance,
                                     webgpu_command_recorder recorder, std::uint32_t first,
                                     const webgpu_viewport* viewports,
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

granit_result recorder_set_scissors(webgpu_instance_handle instance,
                                    webgpu_command_recorder recorder, std::uint32_t first,
                                    const webgpu_scissor* scissors, std::uint32_t count) noexcept {
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

granit_result recorder_draw_vertices(webgpu_instance_handle instance,
                                     webgpu_command_recorder recorder, std::uint32_t vertex_count,
                                     std::uint32_t instance_count, std::uint32_t first_vertex,
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

granit_result recorder_draw_indices(webgpu_instance_handle instance,
                                    webgpu_command_recorder recorder, std::uint32_t index_count,
                                    std::uint32_t instance_count, std::uint32_t first_index,
                                    std::int32_t vertex_offset,
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

granit_result recorder_end_rendering(webgpu_instance_handle instance,
                                     webgpu_command_recorder recorder) noexcept {
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

granit_result recorder_begin_compute(webgpu_instance_handle instance,
                                     webgpu_command_recorder recorder) noexcept {
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

granit_result recorder_bind_compute_pipeline(webgpu_instance_handle instance,
                                             webgpu_command_recorder recorder,
                                             webgpu_compute_pipeline pipeline) noexcept {
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

granit_result
recorder_bind_compute_groups(webgpu_instance_handle instance, webgpu_command_recorder recorder,
                             webgpu_pipeline_layout pipeline_layout, std::uint32_t first_group,
                             const webgpu_bind_group* groups, std::uint32_t group_count,
                             const std::uint32_t* dynamic_offsets,
                             std::uint32_t dynamic_offset_count) noexcept {
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
      if (declaration.type != GRANIT_WEBGPU_BINDING_TYPE_DYNAMIC_UNIFORM_BUFFER)
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

granit_result recorder_dispatch(webgpu_instance_handle instance, webgpu_command_recorder recorder,
                                std::uint32_t x, std::uint32_t y, std::uint32_t z) noexcept {
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

granit_result recorder_end_compute(webgpu_instance_handle instance,
                                   webgpu_command_recorder recorder) noexcept {
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

granit_result recorder_copy_texture_to_buffer(webgpu_instance_handle instance,
                                              webgpu_command_recorder recorder,
                                              webgpu_texture texture, webgpu_buffer buffer,
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
  if ((texture_found->second.usage & GRANIT_WEBGPU_TEXTURE_USAGE_COPY_SRC_BIT) == 0 ||
      (buffer_found->second.usage & GRANIT_WEBGPU_BUFFER_USAGE_COPY_DST_BIT) == 0 ||
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

granit_result finish_command_recorder(webgpu_instance_handle instance,
                                      webgpu_command_recorder recorder,
                                      webgpu_command_buffer* out_command_buffer) noexcept {
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
  for (const auto pool : recorder_found->second.timestamp_pools) {
    const auto query = state.timestamp_queries.find(pool);
    if (query == state.timestamp_queries.end())
      return GRANIT_ERROR_INVALID_HANDLE;
    const auto size = static_cast<std::uint64_t>(query->second->count) * sizeof(std::uint64_t);
    wgpuCommandEncoderResolveQuerySet(recorder_found->second.encoder, query->second->query_set, 0,
                                      query->second->count, query->second->resolve_buffer, 0);
    wgpuCommandEncoderCopyBufferToBuffer(recorder_found->second.encoder,
                                         query->second->resolve_buffer, 0,
                                         query->second->read_buffer, 0, size);
  }
  WGPUCommandBufferDescriptor descriptor = WGPU_COMMAND_BUFFER_DESCRIPTOR_INIT;
  const auto native = wgpuCommandEncoderFinish(recorder_found->second.encoder, &descriptor);
  if (native == nullptr)
    return GRANIT_ERROR_INITIALIZATION_FAILED;
  for (const auto buffer : recorder_found->second.temporary_buffers)
    wgpuBufferRelease(buffer);
  recorder_found->second.temporary_buffers.clear();
  const auto handle = next_handle<webgpu_command_buffer>(next_command_buffer);
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

granit_result create_timestamp_query_pool(webgpu_instance_handle instance,
                                          std::uint32_t query_count,
                                          webgpu_timestamp_query_pool* out_pool) noexcept {
  if (out_pool != nullptr)
    *out_pool = 0;
  if (instance == 0 || query_count == 0 || out_pool == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const std::scoped_lock lock{instances_mutex};
  const auto found = instances.find(instance);
  if (found == instances.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  auto& state = *found->second;
  if (const auto ready = require_ready(state); ready != GRANIT_SUCCESS)
    return ready;
  if ((state.capabilities.renderer_features & GRANIT_WEBGPU_FEATURE_TIMESTAMP_QUERY_BIT) == 0)
    return GRANIT_ERROR_UNSUPPORTED;

  std::shared_ptr<webgpu_device_state::timestamp_query_record> query;
  try {
    query = std::make_shared<webgpu_device_state::timestamp_query_record>();
    query->values.resize(query_count);
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }

  WGPUQuerySetDescriptor query_desc = WGPU_QUERY_SET_DESCRIPTOR_INIT;
  query_desc.type = WGPUQueryType_Timestamp;
  query_desc.count = query_count;
  const auto query_set = wgpuDeviceCreateQuerySet(state.device, &query_desc);
  if (query_set == nullptr)
    return GRANIT_ERROR_OUT_OF_MEMORY;
  const auto size = static_cast<std::uint64_t>(query_count) * sizeof(std::uint64_t);
  WGPUBufferDescriptor resolve_desc = WGPU_BUFFER_DESCRIPTOR_INIT;
  resolve_desc.size = size;
  resolve_desc.usage = WGPUBufferUsage_QueryResolve | WGPUBufferUsage_CopySrc;
  const auto resolve_buffer = wgpuDeviceCreateBuffer(state.device, &resolve_desc);
  WGPUBufferDescriptor read_desc = WGPU_BUFFER_DESCRIPTOR_INIT;
  read_desc.size = size;
  read_desc.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_MapRead;
  const auto read_buffer = wgpuDeviceCreateBuffer(state.device, &read_desc);
  if (resolve_buffer == nullptr || read_buffer == nullptr) {
    if (read_buffer != nullptr)
      wgpuBufferRelease(read_buffer);
    if (resolve_buffer != nullptr)
      wgpuBufferRelease(resolve_buffer);
    wgpuQuerySetRelease(query_set);
    return GRANIT_ERROR_OUT_OF_MEMORY;
  }
  query->query_set = query_set;
  query->resolve_buffer = resolve_buffer;
  query->read_buffer = read_buffer;
  query->count = query_count;
  try {
    const auto handle = next_handle<webgpu_timestamp_query_pool>(next_timestamp_query_pool);
    if (!state.timestamp_queries.emplace(handle, std::move(query)).second)
      throw std::bad_alloc{};
    *out_pool = handle;
    return GRANIT_SUCCESS;
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result destroy_timestamp_query_pool(webgpu_instance_handle instance,
                                           webgpu_timestamp_query_pool pool) noexcept {
  if (instance == 0 || pool == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const std::scoped_lock lock{instances_mutex};
  const auto found = instances.find(instance);
  if (found == instances.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  const auto query = found->second->timestamp_queries.find(pool);
  if (query == found->second->timestamp_queries.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  if (query->second->map_state.load(std::memory_order_acquire) == 1)
    return GRANIT_ERROR_NOT_READY;
  found->second->timestamp_queries.erase(query);
  return GRANIT_SUCCESS;
}

granit_result recorder_reset_timestamp_queries(webgpu_instance_handle instance,
                                               webgpu_command_recorder recorder,
                                               webgpu_timestamp_query_pool pool,
                                               std::uint32_t first, std::uint32_t count) noexcept {
  if (instance == 0 || recorder == 0 || pool == 0 || count == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const std::scoped_lock lock{instances_mutex};
  const auto found = instances.find(instance);
  if (found == instances.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  auto& state = *found->second;
  const auto command = state.command_recorders.find(recorder);
  const auto query = state.timestamp_queries.find(pool);
  if (command == state.command_recorders.end() || query == state.timestamp_queries.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  if (command->second.finished || first > query->second->count ||
      count > query->second->count - first)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  if (query->second->map_state.load(std::memory_order_acquire) == 1)
    return GRANIT_ERROR_NOT_READY;
  query->second->map_state.store(0, std::memory_order_release);
  if (std::find(command->second.timestamp_pools.begin(), command->second.timestamp_pools.end(),
                pool) == command->second.timestamp_pools.end())
    command->second.timestamp_pools.push_back(pool);
  return GRANIT_SUCCESS;
}

granit_result recorder_write_timestamp(webgpu_instance_handle instance,
                                       webgpu_command_recorder recorder,
                                       webgpu_timestamp_query_pool pool,
                                       std::uint32_t query_index) noexcept {
  if (instance == 0 || recorder == 0 || pool == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const std::scoped_lock lock{instances_mutex};
  const auto found = instances.find(instance);
  if (found == instances.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  auto& state = *found->second;
  const auto command = state.command_recorders.find(recorder);
  const auto query = state.timestamp_queries.find(pool);
  if (command == state.command_recorders.end() || query == state.timestamp_queries.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  if (command->second.finished || command->second.pass != nullptr ||
      command->second.compute_pass != nullptr || query_index >= query->second->count)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  wgpuCommandEncoderWriteTimestamp(command->second.encoder, query->second->query_set, query_index);
  if (std::find(command->second.timestamp_pools.begin(), command->second.timestamp_pools.end(),
                pool) == command->second.timestamp_pools.end())
    command->second.timestamp_pools.push_back(pool);
  return GRANIT_SUCCESS;
}

granit_result read_timestamp_query_results(webgpu_instance_handle instance,
                                           webgpu_timestamp_query_pool pool, std::uint32_t first,
                                           std::uint64_t* values, std::uint32_t count) noexcept {
  if (instance == 0 || pool == 0 || values == nullptr || count == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  std::shared_ptr<webgpu_device_state::timestamp_query_record> query;
  {
    const std::scoped_lock lock{instances_mutex};
    const auto found = instances.find(instance);
    if (found == instances.end())
      return GRANIT_ERROR_INVALID_HANDLE;
    const auto query_found = found->second->timestamp_queries.find(pool);
    if (query_found == found->second->timestamp_queries.end())
      return GRANIT_ERROR_INVALID_HANDLE;
    query = query_found->second;
  }
  if (first > query->count || count > query->count - first)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  auto state = query->map_state.load(std::memory_order_acquire);
  if (state == 0) {
    auto* request = new (std::nothrow) timestamp_map_request{query};
    if (request == nullptr)
      return GRANIT_ERROR_OUT_OF_MEMORY;
    query->map_state.store(1, std::memory_order_release);
#if defined(__EMSCRIPTEN__)
    constexpr auto callback_mode = WGPUCallbackMode_AllowSpontaneous;
#else
    constexpr auto callback_mode = WGPUCallbackMode_AllowProcessEvents;
#endif
    const WGPUBufferMapCallbackInfo callback{nullptr, callback_mode, receive_timestamp_map, request,
                                             nullptr};
    static_cast<void>(wgpuBufferMapAsync(
        query->read_buffer, WGPUMapMode_Read, 0,
        static_cast<std::size_t>(query->count) * sizeof(std::uint64_t), callback));
    return GRANIT_ERROR_NOT_READY;
  }
  if (state == 1)
    return GRANIT_ERROR_NOT_READY;
  if (state == 3)
    return GRANIT_ERROR_INTERNAL;
  std::copy_n(query->values.data() + first, count, values);
  return GRANIT_SUCCESS;
}

granit_result destroy_command_buffer(webgpu_instance_handle instance,
                                     webgpu_command_buffer command_buffer) noexcept {
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

granit_result submit_command_buffer(webgpu_instance_handle instance,
                                    webgpu_command_buffer command_buffer) noexcept {
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
granit_result create_native_surface(webgpu_instance_handle instance, NativeDesc source,
                                    webgpu_surface* surface) noexcept {
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
  const auto handle = next_handle<webgpu_surface>(next_surface);
  try {
    found->second->surfaces.emplace(handle,
                                    webgpu_device_state::surface_record{native_surface, {}});
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

granit_result create_win32_surface(webgpu_instance_handle instance,
                                   const webgpu_win32_surface_desc* desc,
                                   webgpu_surface* surface) noexcept {
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

granit_result create_xcb_surface(webgpu_instance_handle instance,
                                 const webgpu_xcb_surface_desc* desc,
                                 webgpu_surface* surface) noexcept {
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

granit_result create_wayland_surface(webgpu_instance_handle instance,
                                     const webgpu_wayland_surface_desc* desc,
                                     webgpu_surface* surface) noexcept {
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

granit_result create_canvas_surface(webgpu_instance_handle instance,
                                    const webgpu_canvas_surface_desc* desc,
                                    webgpu_surface* surface) noexcept {
  if (surface != nullptr)
    *surface = 0;
  if (instance == 0 || desc == nullptr || surface == nullptr ||
      desc->struct_size < sizeof(webgpu_canvas_surface_desc) || desc->reserved != 0 ||
      desc->selector == nullptr || desc->selector_length == 0)
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
    const auto handle = next_handle<webgpu_surface>(next_surface);
    try {
      found->second->surfaces.emplace(
          handle, webgpu_device_state::surface_record{native_surface, std::move(selector)});
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

granit_result destroy_surface(webgpu_instance_handle instance, webgpu_surface surface) noexcept {
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

granit_result configure_swapchain(webgpu_device_state& state, WGPUSurface surface,
                                  const webgpu_swapchain_desc& desc,
                                  webgpu_swapchain_info& info) noexcept {
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
  if (format == WGPUTextureFormat_Undefined) {
    for (std::size_t index = 0; index < capabilities.formatCount; ++index) {
      if (capabilities.formats[index] == WGPUTextureFormat_BGRA8Unorm) {
        format = capabilities.formats[index];
        break;
      }
    }
  }
  if (format == 0) {
    release_capabilities();
    return GRANIT_ERROR_UNSUPPORTED;
  }
  const WGPUPresentMode requested_mode =
      desc.present_mode == GRANIT_WEBGPU_PRESENT_MODE_MAILBOX     ? WGPUPresentMode_Mailbox
      : desc.present_mode == GRANIT_WEBGPU_PRESENT_MODE_IMMEDIATE ? WGPUPresentMode_Immediate
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
  info = {sizeof(webgpu_swapchain_info),
          desc.width,
          desc.height,
          1,
          selected_mode == WGPUPresentMode_Mailbox     ? GRANIT_WEBGPU_PRESENT_MODE_MAILBOX
          : selected_mode == WGPUPresentMode_Immediate ? GRANIT_WEBGPU_PRESENT_MODE_IMMEDIATE
                                                       : GRANIT_WEBGPU_PRESENT_MODE_FIFO,
          format == WGPUTextureFormat_BGRA8Unorm ? GRANIT_WEBGPU_TEXTURE_FORMAT_BGRA8_UNORM
                                                 : GRANIT_WEBGPU_TEXTURE_FORMAT_RGBA8_UNORM};
  return GRANIT_SUCCESS;
}

granit_result create_swapchain(webgpu_instance_handle instance, webgpu_surface surface,
                               const webgpu_swapchain_desc* desc,
                               webgpu_swapchain* swapchain) noexcept {
  if (swapchain != nullptr)
    *swapchain = 0;
  if (instance == 0 || surface == 0 || desc == nullptr || swapchain == nullptr ||
      desc->struct_size < sizeof(webgpu_swapchain_desc) || desc->width == 0 || desc->height == 0 ||
      desc->present_mode > GRANIT_WEBGPU_PRESENT_MODE_IMMEDIATE)
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
  webgpu_swapchain_info info{};
  if (const auto result = configure_swapchain(*found->second, native_surface, *desc, info);
      result != GRANIT_SUCCESS)
    return result;
  const auto handle = next_handle<webgpu_swapchain>(next_swapchain);
  try {
    found->second->swapchains.emplace(
        handle, webgpu_device_state::swapchain_record{surface, native_surface, info, 0, 0});
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

granit_result recreate_swapchain(webgpu_instance_handle instance, webgpu_swapchain swapchain,
                                 const webgpu_swapchain_desc* desc) noexcept {
  if (instance == 0 || swapchain == 0 || desc == nullptr ||
      desc->struct_size < sizeof(webgpu_swapchain_desc) || desc->width == 0 || desc->height == 0 ||
      desc->present_mode > GRANIT_WEBGPU_PRESENT_MODE_IMMEDIATE)
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
  webgpu_swapchain_info info{};
  const auto result = configure_swapchain(
      *found->second, static_cast<WGPUSurface>(swapchain_found->second.native_surface), *desc,
      info);
  if (result == GRANIT_SUCCESS)
    swapchain_found->second.info = info;
  return result;
}

granit_result get_swapchain_info(webgpu_instance_handle instance, webgpu_swapchain swapchain,
                                 webgpu_swapchain_info* info) noexcept {
  if (instance == 0 || swapchain == 0 || info == nullptr ||
      info->struct_size < sizeof(webgpu_swapchain_info))
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

granit_result acquire_swapchain(webgpu_instance_handle instance, webgpu_swapchain swapchain,
                                webgpu_acquired_frame* frame) noexcept {
  if (instance == 0 || swapchain == 0 || frame == nullptr ||
      frame->struct_size < sizeof(webgpu_acquired_frame) || frame->reserved != 0)
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
  const auto texture = next_handle<webgpu_texture>(next_texture);
  const auto view = next_handle<webgpu_texture_view>(next_texture_view);
  try {
    found->second->textures.emplace(
        texture, webgpu_device_state::texture_record{
                     acquired.texture, swapchain_found->second.info.width,
                     swapchain_found->second.info.height, swapchain_found->second.info.format, 1, 1,
                     1, GRANIT_WEBGPU_TEXTURE_USAGE_RENDER_ATTACHMENT_BIT, true});
    found->second->texture_views.emplace(
        view, webgpu_device_state::texture_view_record{native_view, texture, true});
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
  *frame = {sizeof(webgpu_acquired_frame), 0, suboptimal ? 1U : 0U, 0, texture, view};
  return GRANIT_SUCCESS;
}

granit_result finish_swapchain_frame(webgpu_device_state& state,
                                     webgpu_device_state::swapchain_record& swapchain,
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

granit_result present_swapchain(webgpu_instance_handle instance, webgpu_swapchain swapchain,
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

granit_result cancel_swapchain(webgpu_instance_handle instance, webgpu_swapchain swapchain,
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

granit_result destroy_swapchain(webgpu_instance_handle instance,
                                webgpu_swapchain swapchain) noexcept {
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

granit_result webgpu_device::create_win32_surface(const webgpu_win32_surface_desc* desc,
                                                  webgpu_surface* surface) noexcept {
  if (!open_ || desc == nullptr || surface == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    return ::create_win32_surface(instance_, desc, surface);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result webgpu_device::create_xcb_surface(const webgpu_xcb_surface_desc* desc,
                                                webgpu_surface* surface) noexcept {
  if (!open_ || desc == nullptr || surface == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    return ::create_xcb_surface(instance_, desc, surface);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result webgpu_device::create_wayland_surface(const webgpu_wayland_surface_desc* desc,
                                                    webgpu_surface* surface) noexcept {
  if (!open_ || desc == nullptr || surface == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    return ::create_wayland_surface(instance_, desc, surface);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result webgpu_device::create_canvas_surface(const webgpu_canvas_surface_desc* desc,
                                                   webgpu_surface* surface) noexcept {
  if (!open_ || desc == nullptr || surface == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    return ::create_canvas_surface(instance_, desc, surface);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result webgpu_device::destroy_surface(webgpu_surface surface) noexcept {
  if (!open_ || surface == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    return ::destroy_surface(instance_, surface);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result webgpu_device::create_swapchain(webgpu_surface surface,
                                              const webgpu_swapchain_desc* desc,
                                              webgpu_swapchain* swapchain) noexcept {
  if (!open_ || surface == 0 || desc == nullptr || swapchain == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    return ::create_swapchain(instance_, surface, desc, swapchain);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result webgpu_device::recreate_swapchain(webgpu_swapchain swapchain,
                                                const webgpu_swapchain_desc* desc) noexcept {
  if (!open_ || swapchain == 0 || desc == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    return ::recreate_swapchain(instance_, swapchain, desc);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result webgpu_device::get_swapchain_info(webgpu_swapchain swapchain,
                                                webgpu_swapchain_info* info) noexcept {
  if (!open_ || swapchain == 0 || info == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    return ::get_swapchain_info(instance_, swapchain, info);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result webgpu_device::acquire_swapchain(webgpu_swapchain swapchain,
                                               webgpu_acquired_frame* frame) noexcept {
  if (!open_ || swapchain == 0 || frame == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    return ::acquire_swapchain(instance_, swapchain, frame);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result webgpu_device::present_swapchain(webgpu_swapchain swapchain,
                                               std::uint32_t* needs_recreate) noexcept {
  if (!open_ || swapchain == 0 || needs_recreate == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    return ::present_swapchain(instance_, swapchain, needs_recreate);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result webgpu_device::cancel_swapchain(webgpu_swapchain swapchain,
                                              std::uint32_t* needs_recreate) noexcept {
  if (!open_ || swapchain == 0 || needs_recreate == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    return ::cancel_swapchain(instance_, swapchain, needs_recreate);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result webgpu_device::destroy_swapchain(webgpu_swapchain swapchain) noexcept {
  if (!open_ || swapchain == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    return ::destroy_swapchain(instance_, swapchain);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result webgpu_device::create_shader(const webgpu_shader_desc* desc,
                                           webgpu_shader* shader) noexcept {
  if (!open_ || instance_ == 0 || desc == nullptr || shader == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    return ::create_shader(instance_, desc, shader);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result webgpu_device::destroy_shader(webgpu_shader shader) noexcept {
  if (!open_ || instance_ == 0 || shader == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    return ::destroy_shader(instance_, shader);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}
granit_result
webgpu_device::create_pipeline_layout(const webgpu_pipeline_layout_desc* desc,
                                      webgpu_pipeline_layout* pipeline_layout) noexcept {
  if (!open_ || instance_ == 0 || desc == nullptr || pipeline_layout == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    return ::create_pipeline_layout(instance_, desc, pipeline_layout);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result
webgpu_device::destroy_pipeline_layout(webgpu_pipeline_layout pipeline_layout) noexcept {
  if (!open_ || instance_ == 0 || pipeline_layout == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    return ::destroy_pipeline_layout(instance_, pipeline_layout);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result webgpu_device::create_compute_pipeline(const webgpu_compute_pipeline_desc* desc,
                                                     webgpu_compute_pipeline* pipeline) noexcept {
  if (!open_ || instance_ == 0 || desc == nullptr || pipeline == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    return ::create_compute_pipeline(instance_, desc, pipeline);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result webgpu_device::destroy_compute_pipeline(webgpu_compute_pipeline pipeline) noexcept {
  if (!open_ || instance_ == 0 || pipeline == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    return ::destroy_compute_pipeline(instance_, pipeline);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result
webgpu_device::begin_compute_pipeline_warmup(const webgpu_compute_pipeline_desc* desc,
                                             webgpu_pipeline_warmup* warmup) noexcept {
  if (!open_ || instance_ == 0 || desc == nullptr || warmup == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    return ::begin_compute_pipeline_warmup(instance_, desc, warmup);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result webgpu_device::recorder_begin_compute(webgpu_command_recorder recorder) noexcept {
  if (!open_ || instance_ == 0 || recorder == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    return ::recorder_begin_compute(instance_, recorder);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result
webgpu_device::recorder_bind_compute_pipeline(webgpu_command_recorder recorder,
                                              webgpu_compute_pipeline pipeline) noexcept {
  if (!open_ || instance_ == 0 || recorder == 0 || pipeline == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    return ::recorder_bind_compute_pipeline(instance_, recorder, pipeline);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result webgpu_device::recorder_bind_compute_groups(
    webgpu_command_recorder recorder, webgpu_pipeline_layout layout, std::uint32_t first_group,
    std::span<const webgpu_bind_group> groups,
    std::span<const std::uint32_t> dynamic_offsets) noexcept {
  if (!open_ || instance_ == 0 || recorder == 0 || layout == 0 || groups.empty() ||
      groups.size() > UINT32_MAX || dynamic_offsets.size() > UINT32_MAX)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    return ::recorder_bind_compute_groups(instance_, recorder, layout, first_group, groups.data(),
                                          static_cast<std::uint32_t>(groups.size()),
                                          dynamic_offsets.data(),
                                          static_cast<std::uint32_t>(dynamic_offsets.size()));
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result webgpu_device::recorder_dispatch(webgpu_command_recorder recorder, std::uint32_t x,
                                               std::uint32_t y, std::uint32_t z) noexcept {
  if (!open_ || instance_ == 0 || recorder == 0 || x == 0 || y == 0 || z == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    return ::recorder_dispatch(instance_, recorder, x, y, z);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result webgpu_device::recorder_end_compute(webgpu_command_recorder recorder) noexcept {
  if (!open_ || instance_ == 0 || recorder == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    return ::recorder_end_compute(instance_, recorder);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}
granit_result webgpu_device::create_render_pipeline(const webgpu_render_pipeline_desc* desc,
                                                    webgpu_render_pipeline* pipeline) noexcept {
  if (!open_ || instance_ == 0 || desc == nullptr || pipeline == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    return ::create_render_pipeline(instance_, desc, pipeline);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result webgpu_device::destroy_render_pipeline(webgpu_render_pipeline pipeline) noexcept {
  if (!open_ || instance_ == 0 || pipeline == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    return ::destroy_render_pipeline(instance_, pipeline);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result webgpu_device::begin_render_pipeline_warmup(const webgpu_render_pipeline_desc* desc,
                                                          webgpu_pipeline_warmup* warmup) noexcept {
  if (!open_ || instance_ == 0 || desc == nullptr || warmup == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    return ::begin_render_pipeline_warmup(instance_, desc, warmup);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result webgpu_device::poll_pipeline_warmup(webgpu_pipeline_warmup warmup) noexcept {
  if (!open_ || instance_ == 0 || warmup == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    return ::poll_pipeline_warmup(instance_, warmup);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result webgpu_device::destroy_pipeline_warmup(webgpu_pipeline_warmup warmup) noexcept {
  if (!open_ || instance_ == 0 || warmup == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    return ::destroy_pipeline_warmup(instance_, warmup);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result webgpu_device::create_command_recorder(webgpu_command_recorder* recorder) noexcept {
  if (!open_ || instance_ == 0 || recorder == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    return ::create_command_recorder(instance_, recorder);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result webgpu_device::destroy_command_recorder(webgpu_command_recorder recorder) noexcept {
  if (!open_ || instance_ == 0 || recorder == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    return ::destroy_command_recorder(instance_, recorder);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result webgpu_device::recorder_copy_buffer_to_texture(
    webgpu_command_recorder recorder, webgpu_buffer buffer, webgpu_texture texture,
    std::uint32_t width, std::uint32_t height, std::uint32_t bytes_per_row) noexcept {
  if (!open_ || instance_ == 0 || recorder == 0 || buffer == 0 || texture == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    return ::recorder_copy_buffer_to_texture(instance_, recorder, buffer, texture, width, height,
                                             bytes_per_row);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result webgpu_device::recorder_begin_rendering(
    webgpu_command_recorder recorder, webgpu_texture_view target, webgpu_load_operation load,
    webgpu_store_operation store, const float clear[4], webgpu_texture_view resolve_target,
    webgpu_texture_view depth_target, webgpu_load_operation depth_load,
    webgpu_store_operation depth_store, float clear_depth) noexcept {
  if (!open_ || instance_ == 0 || recorder == 0 || (target == 0 && depth_target == 0) ||
      clear == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    return ::recorder_begin_rendering(instance_, recorder, target, resolve_target, load, store,
                                      clear[0], clear[1], clear[2], clear[3], depth_target,
                                      depth_load, depth_store, clear_depth);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result webgpu_device::recorder_bind_pipeline(webgpu_command_recorder recorder,
                                                    webgpu_render_pipeline pipeline) noexcept {
  if (!open_ || instance_ == 0 || recorder == 0 || pipeline == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    return ::recorder_bind_pipeline(instance_, recorder, pipeline);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result webgpu_device::recorder_bind_graphics_groups(
    webgpu_command_recorder recorder, webgpu_pipeline_layout layout, std::uint32_t first_group,
    std::span<const webgpu_bind_group> groups,
    std::span<const std::uint32_t> dynamic_offsets) noexcept {
  if (!open_ || instance_ == 0 || recorder == 0 || layout == 0 || groups.empty() ||
      groups.size() > UINT32_MAX || dynamic_offsets.size() > UINT32_MAX)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    return ::recorder_bind_graphics_groups(instance_, recorder, layout, first_group, groups.data(),
                                           static_cast<std::uint32_t>(groups.size()),
                                           dynamic_offsets.data(),
                                           static_cast<std::uint32_t>(dynamic_offsets.size()));
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result webgpu_device::recorder_bind_vertex_buffers(
    webgpu_command_recorder recorder, std::uint32_t first,
    std::span<const webgpu_vertex_buffer_binding> bindings) noexcept {
  if (!open_ || instance_ == 0 || recorder == 0 || bindings.empty())
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    return ::recorder_bind_vertex_buffers(instance_, recorder, first, bindings.data(),
                                          static_cast<std::uint32_t>(bindings.size()));
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result webgpu_device::recorder_bind_index_buffer(webgpu_command_recorder recorder,
                                                        webgpu_buffer buffer, std::uint64_t offset,
                                                        webgpu_index_format format) noexcept {
  if (!open_ || instance_ == 0 || recorder == 0 || buffer == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    return ::recorder_bind_index_buffer(instance_, recorder, buffer, offset, format);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result
webgpu_device::recorder_set_viewports(webgpu_command_recorder recorder, std::uint32_t first,
                                      std::span<const webgpu_viewport> viewports) noexcept {
  if (!open_ || instance_ == 0 || recorder == 0 || viewports.empty() ||
      viewports.size() > UINT32_MAX)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    return ::recorder_set_viewports(instance_, recorder, first, viewports.data(),
                                    static_cast<std::uint32_t>(viewports.size()));
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result
webgpu_device::recorder_set_scissors(webgpu_command_recorder recorder, std::uint32_t first,
                                     std::span<const webgpu_scissor> scissors) noexcept {
  if (!open_ || instance_ == 0 || recorder == 0 || scissors.empty() || scissors.size() > UINT32_MAX)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    return ::recorder_set_scissors(instance_, recorder, first, scissors.data(),
                                   static_cast<std::uint32_t>(scissors.size()));
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result webgpu_device::recorder_draw_vertices(webgpu_command_recorder recorder,
                                                    std::uint32_t vertex_count,
                                                    std::uint32_t instance_count,
                                                    std::uint32_t first_vertex,
                                                    std::uint32_t first_instance) noexcept {
  if (!open_ || instance_ == 0 || recorder == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    return ::recorder_draw_vertices(instance_, recorder, vertex_count, instance_count, first_vertex,
                                    first_instance);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result webgpu_device::recorder_draw_indices(
    webgpu_command_recorder recorder, std::uint32_t index_count, std::uint32_t instance_count,
    std::uint32_t first_index, std::int32_t vertex_offset, std::uint32_t first_instance) noexcept {
  if (!open_ || instance_ == 0 || recorder == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    return ::recorder_draw_indices(instance_, recorder, index_count, instance_count, first_index,
                                   vertex_offset, first_instance);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result webgpu_device::recorder_end_rendering(webgpu_command_recorder recorder) noexcept {
  if (!open_ || instance_ == 0 || recorder == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    return ::recorder_end_rendering(instance_, recorder);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result
webgpu_device::finish_command_recorder(webgpu_command_recorder recorder,
                                       webgpu_command_buffer* command_buffer) noexcept {
  if (!open_ || instance_ == 0 || recorder == 0 || command_buffer == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    return ::finish_command_recorder(instance_, recorder, command_buffer);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result webgpu_device::destroy_command_buffer(webgpu_command_buffer command_buffer) noexcept {
  if (!open_ || instance_ == 0 || command_buffer == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    return ::destroy_command_buffer(instance_, command_buffer);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result webgpu_device::submit_command_buffer(webgpu_command_buffer command_buffer) noexcept {
  if (!open_ || instance_ == 0 || command_buffer == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    return ::submit_command_buffer(instance_, command_buffer);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result webgpu_device::recorder_copy_texture_to_buffer(
    webgpu_command_recorder recorder, webgpu_texture texture, webgpu_buffer buffer,
    std::uint32_t width, std::uint32_t height, std::uint32_t bytes_per_row) noexcept {
  if (!open_ || instance_ == 0 || recorder == 0 || texture == 0 || buffer == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    return ::recorder_copy_texture_to_buffer(instance_, recorder, texture, buffer, width, height,
                                             bytes_per_row);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result
webgpu_device::recorder_copy_buffer(webgpu_command_recorder recorder, webgpu_buffer source,
                                    webgpu_buffer destination,
                                    std::span<const webgpu_buffer_copy_region> regions) noexcept {
  if (!open_ || instance_ == 0 || recorder == 0 || source == 0 || destination == 0 ||
      regions.empty() || regions.size() > UINT32_MAX)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    return ::recorder_copy_buffer(instance_, recorder, source, destination, regions.data(),
                                  static_cast<std::uint32_t>(regions.size()));
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result webgpu_device::recorder_copy_buffer_to_texture_v2(
    webgpu_command_recorder recorder, webgpu_buffer source, webgpu_texture destination,
    const webgpu_texture_buffer_copy& region) noexcept {
  if (!open_ || instance_ == 0 || recorder == 0 || source == 0 || destination == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    return ::recorder_copy_buffer_to_texture_v2(instance_, recorder, source, destination, &region);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result webgpu_device::recorder_copy_texture_to_buffer_v2(
    webgpu_command_recorder recorder, webgpu_texture source, webgpu_buffer destination,
    const webgpu_texture_buffer_copy& region) noexcept {
  if (!open_ || instance_ == 0 || recorder == 0 || source == 0 || destination == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    return ::recorder_copy_texture_to_buffer_v2(instance_, recorder, source, destination, &region);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result
webgpu_device::recorder_copy_texture(webgpu_command_recorder recorder, webgpu_texture source,
                                     webgpu_texture destination,
                                     const webgpu_texture_copy_region& region) noexcept {
  if (!open_ || instance_ == 0 || recorder == 0 || source == 0 || destination == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    return ::recorder_copy_texture(instance_, recorder, source, destination, &region);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result webgpu_device::recorder_fill_buffer(webgpu_command_recorder recorder,
                                                  webgpu_buffer buffer, std::uint64_t offset,
                                                  std::uint64_t size,
                                                  std::uint32_t value) noexcept {
  if (!open_ || instance_ == 0 || recorder == 0 || buffer == 0 || size == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    return ::recorder_fill_buffer(instance_, recorder, buffer, offset, size, value);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result
webgpu_device::recorder_generate_mipmaps(webgpu_command_recorder recorder, webgpu_texture texture,
                                         const webgpu_texture_mipmap_range& range) noexcept {
  if (!open_ || instance_ == 0 || recorder == 0 || texture == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    return ::recorder_generate_mipmaps(instance_, recorder, texture, &range);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result
webgpu_device::create_timestamp_query_pool(std::uint32_t count,
                                           webgpu_timestamp_query_pool* pool) noexcept {
  return open_ ? ::create_timestamp_query_pool(instance_, count, pool) : GRANIT_ERROR_NOT_READY;
}

granit_result
webgpu_device::destroy_timestamp_query_pool(webgpu_timestamp_query_pool pool) noexcept {
  return open_ ? ::destroy_timestamp_query_pool(instance_, pool) : GRANIT_ERROR_NOT_READY;
}

granit_result webgpu_device::recorder_reset_timestamp_queries(webgpu_command_recorder recorder,
                                                              webgpu_timestamp_query_pool pool,
                                                              std::uint32_t first,
                                                              std::uint32_t count) noexcept {
  return open_ ? ::recorder_reset_timestamp_queries(instance_, recorder, pool, first, count)
               : GRANIT_ERROR_NOT_READY;
}

granit_result webgpu_device::recorder_write_timestamp(webgpu_command_recorder recorder,
                                                      webgpu_timestamp_query_pool pool,
                                                      std::uint32_t index) noexcept {
  return open_ ? ::recorder_write_timestamp(instance_, recorder, pool, index)
               : GRANIT_ERROR_NOT_READY;
}

granit_result webgpu_device::read_timestamp_query_results(webgpu_timestamp_query_pool pool,
                                                          std::uint32_t first,
                                                          std::uint64_t* values,
                                                          std::uint32_t count) noexcept {
  return open_ ? ::read_timestamp_query_results(instance_, pool, first, values, count)
               : GRANIT_ERROR_NOT_READY;
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
