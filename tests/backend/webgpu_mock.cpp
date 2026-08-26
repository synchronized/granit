// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <webgpu/webgpu.h>

struct WGPUInstanceImpl {};
struct WGPUAdapterImpl {};
struct WGPUDeviceImpl {};

extern "C" WGPUInstance wgpuCreateInstance(const WGPUInstanceDescriptor*) {
  return new WGPUInstanceImpl;
}

extern "C" void wgpuInstanceRelease(WGPUInstance instance) { delete instance; }

extern "C" WGPUFuture wgpuInstanceRequestAdapter(WGPUInstance, const void*,
                                                 WGPURequestAdapterCallbackInfo callback_info) {
  callback_info.callback(WGPURequestAdapterStatus_Success, new WGPUAdapterImpl, {},
                         callback_info.userdata1, callback_info.userdata2);
  return {1};
}

extern "C" WGPUWaitStatus wgpuInstanceWaitAny(WGPUInstance, size_t, WGPUFutureWaitInfo* futures,
                                              unsigned long long) {
  futures[0].completed = 1;
  return WGPUWaitStatus_Success;
}

extern "C" WGPUFuture wgpuAdapterRequestDevice(WGPUAdapter, const void*,
                                               WGPURequestDeviceCallbackInfo callback_info) {
  callback_info.callback(WGPURequestDeviceStatus_Success, new WGPUDeviceImpl, {},
                         callback_info.userdata1, callback_info.userdata2);
  return {2};
}

extern "C" void wgpuAdapterRelease(WGPUAdapter adapter) { delete adapter; }

extern "C" void wgpuDeviceRelease(WGPUDevice device) { delete device; }
