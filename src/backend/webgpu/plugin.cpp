// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "backend/plugin_api.h"

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <mutex>
#include <new>
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

  granit_backend_plugin_host_api host;
  WGPUInstance instance;
  WGPUAdapter adapter;
  WGPUDevice device;
  WGPUQueue queue;
  granit_backend_plugin_capabilities capabilities;
  std::unordered_map<granit_backend_plugin_buffer, buffer_record> buffers;
};

constexpr std::uint64_t request_timeout_ns = UINT64_C(10000000000);

struct adapter_request {
  const granit_backend_plugin_host_api* host{};
  WGPURequestAdapterStatus status{};
  WGPUAdapter adapter{};
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

void deallocate(const granit_backend_plugin_host_api& host, void* memory) noexcept {
  try {
    host.deallocate(memory, sizeof(webgpu_instance), alignof(webgpu_instance),
                    host.allocator_user_data);
  } catch (...) {
  }
}

void release_resources(webgpu_instance& state) noexcept {
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

void receive_adapter(WGPURequestAdapterStatus status, WGPUAdapter adapter, WGPUStringView message,
                     void* data, void*) noexcept {
  auto& request = *static_cast<adapter_request*>(data);
  request.status = status;
  request.adapter = adapter;
  if (status != WGPURequestAdapterStatus_Success) {
    emit_dawn_message(request.host, message);
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

void receive_map(WGPUMapAsyncStatus status, WGPUStringView message, void* data, void*) noexcept {
  auto& request = *static_cast<map_request*>(data);
  request.status = status;
  if (status != WGPUMapAsyncStatus_Success) {
    emit_dawn_message(request.host, message);
  }
}

template <typename Request>
bool wait_for(WGPUInstance instance, WGPUFuture future, Request& request) noexcept {
  WGPUFutureWaitInfo wait_info{future, WGPU_FALSE};
  return wgpuInstanceWaitAny(instance, 1, &wait_info, request_timeout_ns) ==
             WGPUWaitStatus_Success &&
         wait_info.completed && request.status != 0;
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
  constexpr WGPUInstanceFeatureName features[]{WGPUInstanceFeatureName_TimedWaitAny};
  const WGPUInstanceLimits instance_limits{nullptr, 1};
  const WGPUInstanceDescriptor descriptor{nullptr, 1, features, &instance_limits};
  auto* state = new (memory)
      webgpu_instance{*host, wgpuCreateInstance(&descriptor), nullptr, nullptr, nullptr, {}, {}};
  if (state->instance == nullptr) {
    state->~webgpu_instance();
    deallocate(*host, memory);
    return GRANIT_ERROR_INITIALIZATION_FAILED;
  }

  adapter_request adapter{host};
  const WGPURequestAdapterCallbackInfo adapter_callback{nullptr, WGPUCallbackMode_WaitAnyOnly,
                                                        receive_adapter, &adapter, nullptr};
  WGPURequestAdapterOptions adapter_options{};
#if defined(_WIN32)
  adapter_options.backendType = WGPUBackendType_D3D12;
#else
  adapter_options.backendType = WGPUBackendType_Vulkan;
#endif
  const auto adapter_future =
      wgpuInstanceRequestAdapter(state->instance, &adapter_options, adapter_callback);
  if (!wait_for(state->instance, adapter_future, adapter) ||
      adapter.status != WGPURequestAdapterStatus_Success || adapter.adapter == nullptr) {
    constexpr char message[] = "Dawn WebGPU adapter request failed or timed out";
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
  const WGPURequestDeviceCallbackInfo device_callback{nullptr, WGPUCallbackMode_WaitAnyOnly,
                                                      receive_device, &device, nullptr};
  const auto device_future = wgpuAdapterRequestDevice(state->adapter, nullptr, device_callback);
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

  granit_backend_plugin_instance handle = next_instance.fetch_add(1, std::memory_order_relaxed);
  if (handle == 0) {
    handle = next_instance.fetch_add(1, std::memory_order_relaxed);
  }
  try {
    const std::scoped_lock lock{instances_mutex};
    const auto [iterator, inserted] = instances.emplace(handle, state);
    static_cast<void>(iterator);
    if (!inserted) {
      release_resources(*state);
      state->~webgpu_instance();
      deallocate(*host, memory);
      return GRANIT_ERROR_INTERNAL;
    }
  } catch (const std::bad_alloc&) {
    release_resources(*state);
    state->~webgpu_instance();
    deallocate(*host, memory);
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    release_resources(*state);
    state->~webgpu_instance();
    deallocate(*host, memory);
    return GRANIT_ERROR_INTERNAL;
  }

  constexpr char message[] = "Dawn WebGPU instance, adapter and device created";
  emit(*host, GRANIT_DIAGNOSTIC_SEVERITY_INFO, message, sizeof(message) - 1);
  *out_instance = handle;
  return GRANIT_SUCCESS;
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
  const auto caller_size = capabilities->struct_size;
  *capabilities = found->second->capabilities;
  capabilities->struct_size = caller_size;
  return GRANIT_SUCCESS;
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

constexpr char plugin_name[] = "Granit WebGPU (Dawn)";
constexpr granit_backend_plugin_instance_api instance_api{
    sizeof(granit_backend_plugin_instance_api),
    0,
    get_capabilities,
    create_buffer,
    destroy_buffer,
    write_buffer,
    read_buffer};
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
