// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "backend/plugin_api.h"

#include <atomic>
#include <cstddef>
#include <cstdint>
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
  granit_backend_plugin_host_api host;
  WGPUInstance instance;
  WGPUAdapter adapter;
  WGPUDevice device;
};

constexpr std::uint64_t request_timeout_ns = UINT64_C(10000000000);

struct adapter_request {
  WGPURequestAdapterStatus status{};
  WGPUAdapter adapter{};
};

struct device_request {
  WGPURequestDeviceStatus status{};
  WGPUDevice device{};
};

std::mutex instances_mutex;
std::unordered_map<granit_backend_plugin_instance, webgpu_instance*> instances;
std::atomic_uint64_t next_instance{1};

void deallocate(const granit_backend_plugin_host_api& host, void* memory) noexcept {
  try {
    host.deallocate(memory, sizeof(webgpu_instance), alignof(webgpu_instance),
                    host.allocator_user_data);
  } catch (...) {
  }
}

void release_resources(webgpu_instance& state) noexcept {
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

void receive_adapter(WGPURequestAdapterStatus status, WGPUAdapter adapter, WGPUStringView,
                     void* data, void*) noexcept {
  auto& request = *static_cast<adapter_request*>(data);
  request.status = status;
  request.adapter = adapter;
}

void receive_device(WGPURequestDeviceStatus status, WGPUDevice device, WGPUStringView, void* data,
                    void*) noexcept {
  auto& request = *static_cast<device_request*>(data);
  request.status = status;
  request.device = device;
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
  const WGPUInstanceLimits limits{nullptr, 1};
  const WGPUInstanceDescriptor descriptor{nullptr, 1, features, &limits};
  auto* state =
      new (memory) webgpu_instance{*host, wgpuCreateInstance(&descriptor), nullptr, nullptr};
  if (state->instance == nullptr) {
    state->~webgpu_instance();
    deallocate(*host, memory);
    return GRANIT_ERROR_INITIALIZATION_FAILED;
  }

  adapter_request adapter{};
  const WGPURequestAdapterCallbackInfo adapter_callback{nullptr, WGPUCallbackMode_WaitAnyOnly,
                                                        receive_adapter, &adapter, nullptr};
  WGPURequestAdapterOptions adapter_options{};
  adapter_options.forceFallbackAdapter = WGPU_TRUE;
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

  device_request device{};
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

constexpr char plugin_name[] = "Granit WebGPU (Dawn)";
constexpr granit_backend_plugin_api plugin_api{sizeof(granit_backend_plugin_api),
                                               GRANIT_BACKEND_PLUGIN_ABI_VERSION,
                                               GRANIT_BACKEND_PLUGIN_KIND_WEBGPU,
                                               0,
                                               plugin_name,
                                               sizeof(plugin_name) - 1,
                                               create_backend,
                                               destroy_backend};

} // namespace

extern "C" GRANIT_BACKEND_PLUGIN_EXPORT const granit_backend_plugin_api*
granit_backend_plugin_query(uint32_t requested_abi) noexcept {
  return requested_abi == GRANIT_BACKEND_PLUGIN_ABI_VERSION ? &plugin_api : nullptr;
}
