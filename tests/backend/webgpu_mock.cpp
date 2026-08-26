// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <webgpu/webgpu.h>

#include <cstring>
#include <vector>

struct WGPUInstanceImpl {};
struct WGPUAdapterImpl {};
struct WGPUDeviceImpl {};
struct WGPUQueueImpl {};
struct WGPUBufferImpl {
  std::vector<unsigned char> bytes;
  WGPUBufferUsage usage{};
  bool mapped{};
};
struct WGPUTextureImpl {
  unsigned int width;
  unsigned int height;
  WGPUTextureUsage usage;
};
struct WGPUTextureViewImpl {
  WGPUTexture texture;
};
struct WGPUSamplerImpl {
  WGPUFilterMode min_filter;
  WGPUFilterMode mag_filter;
};

extern "C" WGPUInstance wgpuCreateInstance(const WGPUInstanceDescriptor*) {
  return new WGPUInstanceImpl;
}

extern "C" void wgpuInstanceRelease(WGPUInstance instance) { delete instance; }

extern "C" WGPUFuture wgpuInstanceRequestAdapter(WGPUInstance, const WGPURequestAdapterOptions*,
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

extern "C" WGPUStatus wgpuDeviceGetLimits(WGPUDevice, WGPULimits* limits) {
  if (limits == nullptr) {
    return WGPUStatus_Error;
  }
  limits->minUniformBufferOffsetAlignment = 256;
  limits->minStorageBufferOffsetAlignment = 256;
  limits->maxUniformBufferBindingSize = 65536;
  limits->maxStorageBufferBindingSize = 134217728;
  limits->maxBufferSize = 268435456;
  limits->maxTextureDimension2D = 8192;
  limits->maxBindGroups = 4;
  limits->maxColorAttachments = 8;
  return WGPUStatus_Success;
}

extern "C" WGPUQueue wgpuDeviceGetQueue(WGPUDevice) { return new WGPUQueueImpl; }

extern "C" void wgpuQueueRelease(WGPUQueue queue) { delete queue; }

extern "C" WGPUBuffer wgpuDeviceCreateBuffer(WGPUDevice, const WGPUBufferDescriptor* descriptor) {
  if (descriptor == nullptr || descriptor->size == 0) {
    return nullptr;
  }
  return new WGPUBufferImpl{std::vector<unsigned char>(static_cast<std::size_t>(descriptor->size)),
                            descriptor->usage, false};
}

extern "C" void wgpuBufferRelease(WGPUBuffer buffer) { delete buffer; }

extern "C" void wgpuQueueWriteBuffer(WGPUQueue, WGPUBuffer buffer, unsigned long long offset,
                                     const void* data, size_t size) {
  std::memcpy(buffer->bytes.data() + static_cast<std::size_t>(offset), data, size);
}

extern "C" WGPUFuture wgpuBufferMapAsync(WGPUBuffer buffer, WGPUMapMode, size_t offset, size_t size,
                                         WGPUBufferMapCallbackInfo callback_info) {
  const auto valid = buffer != nullptr && offset <= buffer->bytes.size() &&
                     size <= buffer->bytes.size() - offset &&
                     (buffer->usage & WGPUBufferUsage_MapRead) != 0;
  buffer->mapped = valid;
  callback_info.callback(valid ? WGPUMapAsyncStatus_Success : WGPUMapAsyncStatus_Error, {},
                         callback_info.userdata1, callback_info.userdata2);
  return {3};
}

extern "C" const void* wgpuBufferGetConstMappedRange(WGPUBuffer buffer, size_t offset,
                                                     size_t size) {
  if (buffer == nullptr || !buffer->mapped || offset > buffer->bytes.size() ||
      size > buffer->bytes.size() - offset) {
    return nullptr;
  }
  return buffer->bytes.data() + offset;
}

extern "C" void wgpuBufferUnmap(WGPUBuffer buffer) { buffer->mapped = false; }

extern "C" WGPUTexture wgpuDeviceCreateTexture(WGPUDevice,
                                               const WGPUTextureDescriptor* descriptor) {
  if (descriptor == nullptr || descriptor->size.width == 0 || descriptor->size.height == 0 ||
      descriptor->format != WGPUTextureFormat_RGBA8Unorm) {
    return nullptr;
  }
  return new WGPUTextureImpl{descriptor->size.width, descriptor->size.height, descriptor->usage};
}

extern "C" void wgpuTextureRelease(WGPUTexture texture) { delete texture; }

extern "C" WGPUTextureView wgpuTextureCreateView(WGPUTexture texture,
                                                 const WGPUTextureViewDescriptor*) {
  return texture == nullptr ? nullptr : new WGPUTextureViewImpl{texture};
}

extern "C" void wgpuTextureViewRelease(WGPUTextureView view) { delete view; }

extern "C" WGPUSampler wgpuDeviceCreateSampler(WGPUDevice,
                                               const WGPUSamplerDescriptor* descriptor) {
  if (descriptor == nullptr) {
    return nullptr;
  }
  return new WGPUSamplerImpl{descriptor->minFilter, descriptor->magFilter};
}

extern "C" void wgpuSamplerRelease(WGPUSampler sampler) { delete sampler; }
