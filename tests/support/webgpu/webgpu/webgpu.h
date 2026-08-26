// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_TEST_WEBGPU_H_
#define GRANIT_TEST_WEBGPU_H_

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct WGPUInstanceImpl* WGPUInstance;
typedef struct WGPUAdapterImpl* WGPUAdapter;
typedef struct WGPUDeviceImpl* WGPUDevice;

typedef unsigned int WGPUBool;
typedef unsigned int WGPUInstanceFeatureName;
typedef unsigned int WGPUCallbackMode;
typedef unsigned int WGPURequestAdapterStatus;
typedef unsigned int WGPURequestDeviceStatus;
typedef unsigned int WGPUWaitStatus;
typedef unsigned int WGPUStatus;

#define WGPU_FALSE 0
#define WGPU_TRUE 1
#define WGPU_STRLEN ((size_t)-1)
#define WGPUInstanceFeatureName_TimedWaitAny 1
#define WGPUCallbackMode_WaitAnyOnly 1
#define WGPURequestAdapterStatus_Success 1
#define WGPURequestDeviceStatus_Success 1
#define WGPUWaitStatus_Success 1
#define WGPUStatus_Success 1
#define WGPUStatus_Error 2
#define WGPUBackendType_D3D12 4
#define WGPUBackendType_Vulkan 6

typedef struct WGPUStringView {
  const char* data;
  size_t length;
} WGPUStringView;

typedef struct WGPUInstanceLimits {
  void* nextInChain;
  size_t timedWaitAnyMaxCount;
} WGPUInstanceLimits;

typedef struct WGPUInstanceDescriptor {
  void* nextInChain;
  size_t requiredFeatureCount;
  const WGPUInstanceFeatureName* requiredFeatures;
  const WGPUInstanceLimits* requiredLimits;
} WGPUInstanceDescriptor;

typedef struct WGPULimits {
  void* nextInChain;
  unsigned int maxTextureDimension2D;
  unsigned int maxBindGroups;
  unsigned int maxColorAttachments;
  unsigned long long maxUniformBufferBindingSize;
  unsigned long long maxStorageBufferBindingSize;
  unsigned int minUniformBufferOffsetAlignment;
  unsigned int minStorageBufferOffsetAlignment;
  unsigned long long maxBufferSize;
} WGPULimits;

#define WGPU_LIMITS_INIT                                                                           \
  {                                                                                                \
  }

typedef struct WGPUFuture {
  unsigned long long id;
} WGPUFuture;

typedef struct WGPUFutureWaitInfo {
  WGPUFuture future;
  WGPUBool completed;
} WGPUFutureWaitInfo;

typedef struct WGPURequestAdapterOptions {
  void* nextInChain;
  unsigned int featureLevel;
  unsigned int powerPreference;
  WGPUBool forceFallbackAdapter;
  unsigned int backendType;
  void* compatibleSurface;
} WGPURequestAdapterOptions;

typedef void (*WGPURequestAdapterCallback)(WGPURequestAdapterStatus, WGPUAdapter, WGPUStringView,
                                           void*, void*);
typedef void (*WGPURequestDeviceCallback)(WGPURequestDeviceStatus, WGPUDevice, WGPUStringView,
                                          void*, void*);

typedef struct WGPURequestAdapterCallbackInfo {
  void* nextInChain;
  WGPUCallbackMode mode;
  WGPURequestAdapterCallback callback;
  void* userdata1;
  void* userdata2;
} WGPURequestAdapterCallbackInfo;

typedef struct WGPURequestDeviceCallbackInfo {
  void* nextInChain;
  WGPUCallbackMode mode;
  WGPURequestDeviceCallback callback;
  void* userdata1;
  void* userdata2;
} WGPURequestDeviceCallbackInfo;

WGPUInstance wgpuCreateInstance(const WGPUInstanceDescriptor* descriptor);
void wgpuInstanceRelease(WGPUInstance instance);
WGPUFuture wgpuInstanceRequestAdapter(WGPUInstance instance,
                                      const WGPURequestAdapterOptions* options,
                                      WGPURequestAdapterCallbackInfo callbackInfo);
WGPUWaitStatus wgpuInstanceWaitAny(WGPUInstance instance, size_t futureCount,
                                   WGPUFutureWaitInfo* futures, unsigned long long timeoutNS);
WGPUFuture wgpuAdapterRequestDevice(WGPUAdapter adapter, const void* descriptor,
                                    WGPURequestDeviceCallbackInfo callbackInfo);
void wgpuAdapterRelease(WGPUAdapter adapter);
void wgpuDeviceRelease(WGPUDevice device);
WGPUStatus wgpuDeviceGetLimits(WGPUDevice device, WGPULimits* limits);

#ifdef __cplusplus
}
#endif

#endif
