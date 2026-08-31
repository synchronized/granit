// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <webgpu/webgpu.h>

#include <algorithm>
#include <cstring>
#include <vector>

struct WGPUInstanceImpl {};
struct WGPUAdapterImpl {};
struct WGPUDeviceImpl {
  WGPUDeviceLostCallbackInfo lost_callback{};
};
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
  WGPUTextureFormat format;
  unsigned int mip_levels;
  std::vector<unsigned char> bytes;
};
struct WGPUTextureViewImpl {
  WGPUTexture texture;
};
struct WGPUSurfaceImpl {
  WGPUDevice device{};
  unsigned int width{};
  unsigned int height{};
  WGPUTextureFormat format{};
  WGPUPresentMode present_mode{};
  WGPUTexture current{};
};
struct WGPUSamplerImpl {
  WGPUFilterMode min_filter;
  WGPUFilterMode mag_filter;
  WGPUMipmapFilterMode mipmap_filter;
  WGPUAddressMode address_mode_u;
  WGPUAddressMode address_mode_v;
  WGPUAddressMode address_mode_w;
  WGPUCompareFunction compare;
  unsigned short max_anisotropy;
  float min_lod;
  float max_lod;
};
struct WGPUBindGroupLayoutImpl {};
struct WGPUBindGroupImpl {};
struct WGPUPipelineLayoutImpl {};
struct WGPUShaderModuleImpl {};
struct WGPURenderPipelineImpl {};
struct WGPUCommandEncoderImpl {
  bool finished{};
};
struct WGPUCommandBufferImpl {};
struct WGPURenderPassEncoderImpl {
  WGPUTexture target;
};

extern "C" WGPUInstance wgpuCreateInstance(const WGPUInstanceDescriptor*) {
  return new WGPUInstanceImpl;
}

extern "C" WGPUSurface wgpuInstanceCreateSurface(WGPUInstance,
                                                 const WGPUSurfaceDescriptor* descriptor) {
  if (descriptor == nullptr || descriptor->nextInChain == nullptr)
    return nullptr;
  switch (descriptor->nextInChain->sType) {
  case WGPUSType_EmscriptenSurfaceSourceCanvasHTMLSelector: {
    const auto* canvas = reinterpret_cast<const WGPUEmscriptenSurfaceSourceCanvasHTMLSelector*>(
        descriptor->nextInChain);
    return canvas->selector.data != nullptr && canvas->selector.length != 0 ? new WGPUSurfaceImpl
                                                                            : nullptr;
  }
  case WGPUSType_SurfaceSourceWindowsHWND: {
    const auto* source =
        reinterpret_cast<const WGPUSurfaceSourceWindowsHWND*>(descriptor->nextInChain);
    return source->hinstance != nullptr && source->hwnd != nullptr ? new WGPUSurfaceImpl : nullptr;
  }
  case WGPUSType_SurfaceSourceXCBWindow: {
    const auto* source =
        reinterpret_cast<const WGPUSurfaceSourceXCBWindow*>(descriptor->nextInChain);
    return source->connection != nullptr && source->window != 0 ? new WGPUSurfaceImpl : nullptr;
  }
  case WGPUSType_SurfaceSourceWaylandSurface: {
    const auto* source =
        reinterpret_cast<const WGPUSurfaceSourceWaylandSurface*>(descriptor->nextInChain);
    return source->display != nullptr && source->surface != nullptr ? new WGPUSurfaceImpl : nullptr;
  }
  default:
    return nullptr;
  }
}

extern "C" void wgpuSurfaceRelease(WGPUSurface surface) {
  delete surface->current;
  delete surface;
}

extern "C" WGPUStatus wgpuSurfaceGetCapabilities(WGPUSurface surface, WGPUAdapter,
                                                 WGPUSurfaceCapabilities* capabilities) {
  static constexpr WGPUTextureFormat formats[]{WGPUTextureFormat_RGBA8Unorm};
  static constexpr WGPUPresentMode modes[]{WGPUPresentMode_Fifo};
  static constexpr WGPUCompositeAlphaMode alpha_modes[]{WGPUCompositeAlphaMode_Auto};
  if (surface == nullptr || capabilities == nullptr)
    return WGPUStatus_Error;
  capabilities->usages = WGPUTextureUsage_RenderAttachment;
  capabilities->formatCount = 1;
  capabilities->formats = formats;
  capabilities->presentModeCount = 1;
  capabilities->presentModes = modes;
  capabilities->alphaModeCount = 1;
  capabilities->alphaModes = alpha_modes;
  return WGPUStatus_Success;
}

extern "C" void wgpuSurfaceCapabilitiesFreeMembers(WGPUSurfaceCapabilities) {}

extern "C" void wgpuSurfaceConfigure(WGPUSurface surface,
                                     const WGPUSurfaceConfiguration* configuration) {
  if (surface == nullptr || configuration == nullptr)
    return;
  surface->device = configuration->device;
  surface->width = configuration->width;
  surface->height = configuration->height;
  surface->format = configuration->format;
  surface->present_mode = configuration->presentMode;
}

extern "C" void wgpuSurfaceUnconfigure(WGPUSurface surface) {
  if (surface != nullptr)
    surface->device = nullptr;
}

extern "C" void wgpuSurfaceGetCurrentTexture(WGPUSurface surface,
                                             WGPUSurfaceTexture* surface_texture) {
  if (surface_texture == nullptr)
    return;
  *surface_texture = {};
  if (surface == nullptr || surface->device == nullptr || surface->current != nullptr) {
    surface_texture->status = WGPUSurfaceGetCurrentTextureStatus_Error;
    return;
  }
  surface->current = new WGPUTextureImpl{
      surface->width, surface->height, WGPUTextureUsage_RenderAttachment,
      surface->format, 1,
      std::vector<unsigned char>(static_cast<std::size_t>(surface->width) * surface->height * 4)};
  surface_texture->texture = surface->current;
  surface_texture->status = WGPUSurfaceGetCurrentTextureStatus_SuccessOptimal;
}

extern "C" WGPUStatus wgpuSurfacePresent(WGPUSurface surface) {
  if (surface == nullptr || surface->current == nullptr)
    return WGPUStatus_Error;
  surface->current = nullptr;
  return WGPUStatus_Success;
}

extern "C" void wgpuInstanceRelease(WGPUInstance instance) { delete instance; }

extern "C" void wgpuInstanceProcessEvents(WGPUInstance) {}

extern "C" WGPUFuture wgpuInstanceRequestAdapter(WGPUInstance,
                                                 const WGPURequestAdapterOptions* options,
                                                 WGPURequestAdapterCallbackInfo callback_info) {
  if (options != nullptr && options->forceFallbackAdapter) {
    constexpr char message[] = "mock fallback adapter unavailable";
    callback_info.callback(0, nullptr, {message, sizeof(message) - 1}, callback_info.userdata1,
                           callback_info.userdata2);
    return {1};
  }
  callback_info.callback(WGPURequestAdapterStatus_Success, new WGPUAdapterImpl, {},
                         callback_info.userdata1, callback_info.userdata2);
  return {1};
}

extern "C" WGPUWaitStatus wgpuInstanceWaitAny(WGPUInstance, size_t, WGPUFutureWaitInfo* futures,
                                              unsigned long long) {
  futures[0].completed = 1;
  return WGPUWaitStatus_Success;
}

extern "C" WGPUFuture wgpuAdapterRequestDevice(WGPUAdapter, const WGPUDeviceDescriptor* descriptor,
                                               WGPURequestDeviceCallbackInfo callback_info) {
  auto* device = new WGPUDeviceImpl;
  if (descriptor != nullptr)
    device->lost_callback = descriptor->deviceLostCallbackInfo;
  callback_info.callback(WGPURequestDeviceStatus_Success, device, {}, callback_info.userdata1,
                         callback_info.userdata2);
  return {2};
}

extern "C" void wgpuAdapterRelease(WGPUAdapter adapter) { delete adapter; }

extern "C" void wgpuDeviceRelease(WGPUDevice device) {
  if (device->lost_callback.callback != nullptr) {
    device->lost_callback.callback(&device, WGPUDeviceLostReason_Destroyed, {},
                                   device->lost_callback.userdata1,
                                   device->lost_callback.userdata2);
  }
  delete device;
}

extern "C" void wgpuDeviceForceLoss(WGPUDevice device, WGPUDeviceLostReason reason,
                                    WGPUStringView message) {
  if (device->lost_callback.callback != nullptr) {
    device->lost_callback.callback(&device, reason, message, device->lost_callback.userdata1,
                                   device->lost_callback.userdata2);
  }
}

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

extern "C" void wgpuQueueWriteTexture(WGPUQueue, const WGPUTexelCopyTextureInfo* destination,
                                       const void* data, size_t data_size,
                                       const WGPUTexelCopyBufferLayout* layout,
                                       const WGPUExtent3D* write_size) {
  if (destination == nullptr || destination->texture == nullptr || data == nullptr ||
      layout == nullptr || write_size == nullptr)
    return;
  auto* texture = destination->texture;
  const auto bytes_per_pixel = texture->format == WGPUTextureFormat_R8Unorm
                                   ? 1U
                                   : texture->format == WGPUTextureFormat_RG8Unorm ? 2U : 4U;
  std::size_t mip_offset{};
  for (unsigned int mip = 0; mip < destination->mipLevel; ++mip) {
    mip_offset += static_cast<std::size_t>(std::max(1U, texture->width >> mip)) *
                  std::max(1U, texture->height >> mip) * bytes_per_pixel;
  }
  const auto mip_width = std::max(1U, texture->width >> destination->mipLevel);
  const auto source_row = layout->bytesPerRow;
  const auto tight_row = static_cast<std::size_t>(write_size->width) * bytes_per_pixel;
  if (data_size < (static_cast<std::size_t>(write_size->height) - 1) * source_row + tight_row)
    return;
  for (unsigned int row = 0; row < write_size->height; ++row) {
    const auto destination_offset =
        mip_offset + (static_cast<std::size_t>(destination->origin.y + row) * mip_width +
                      destination->origin.x) *
                         bytes_per_pixel;
    std::memcpy(texture->bytes.data() + destination_offset,
                static_cast<const unsigned char*>(data) + layout->offset +
                    static_cast<std::size_t>(row) * source_row,
                tight_row);
  }
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
  const auto supported_format = descriptor != nullptr &&
                                (descriptor->format == WGPUTextureFormat_R8Unorm ||
                                 descriptor->format == WGPUTextureFormat_RG8Unorm ||
                                 descriptor->format == WGPUTextureFormat_RGBA8Unorm ||
                                 descriptor->format == WGPUTextureFormat_RGBA8UnormSrgb ||
                                 descriptor->format == WGPUTextureFormat_Depth32Float);
  if (descriptor == nullptr || descriptor->size.width == 0 || descriptor->size.height == 0 ||
      descriptor->mipLevelCount == 0 || !supported_format) {
    return nullptr;
  }
  const auto bytes_per_pixel = descriptor->format == WGPUTextureFormat_R8Unorm
                                   ? 1U
                                   : descriptor->format == WGPUTextureFormat_RG8Unorm ? 2U : 4U;
  std::size_t byte_count{};
  for (unsigned int mip = 0; mip < descriptor->mipLevelCount; ++mip)
    byte_count += static_cast<std::size_t>(std::max(1U, descriptor->size.width >> mip)) *
                  std::max(1U, descriptor->size.height >> mip) * bytes_per_pixel;
  return new WGPUTextureImpl{descriptor->size.width, descriptor->size.height, descriptor->usage,
                             descriptor->format, descriptor->mipLevelCount,
                             std::vector<unsigned char>(byte_count)};
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
  return new WGPUSamplerImpl{descriptor->minFilter,
                             descriptor->magFilter,
                             descriptor->mipmapFilter,
                             descriptor->addressModeU,
                             descriptor->addressModeV,
                             descriptor->addressModeW,
                             descriptor->compare,
                             descriptor->maxAnisotropy,
                             descriptor->lodMinClamp,
                             descriptor->lodMaxClamp};
}

extern "C" void wgpuSamplerRelease(WGPUSampler sampler) { delete sampler; }

extern "C" WGPUBindGroupLayout
wgpuDeviceCreateBindGroupLayout(WGPUDevice, const WGPUBindGroupLayoutDescriptor* descriptor) {
  return descriptor != nullptr && descriptor->entryCount == 2 ? new WGPUBindGroupLayoutImpl
                                                              : nullptr;
}
extern "C" void wgpuBindGroupLayoutRelease(WGPUBindGroupLayout layout) { delete layout; }
extern "C" WGPUBindGroup wgpuDeviceCreateBindGroup(WGPUDevice,
                                                   const WGPUBindGroupDescriptor* descriptor) {
  return descriptor != nullptr && descriptor->layout != nullptr && descriptor->entryCount == 2
             ? new WGPUBindGroupImpl
             : nullptr;
}
extern "C" void wgpuBindGroupRelease(WGPUBindGroup bind_group) { delete bind_group; }
extern "C" WGPUPipelineLayout
wgpuDeviceCreatePipelineLayout(WGPUDevice, const WGPUPipelineLayoutDescriptor* descriptor) {
  return descriptor != nullptr && descriptor->bindGroupLayoutCount == 1 ? new WGPUPipelineLayoutImpl
                                                                        : nullptr;
}
extern "C" void wgpuPipelineLayoutRelease(WGPUPipelineLayout layout) { delete layout; }
extern "C" WGPUShaderModule
wgpuDeviceCreateShaderModule(WGPUDevice, const WGPUShaderModuleDescriptor* descriptor) {
  return descriptor != nullptr && descriptor->nextInChain != nullptr ? new WGPUShaderModuleImpl
                                                                     : nullptr;
}
extern "C" void wgpuShaderModuleRelease(WGPUShaderModule shader_module) { delete shader_module; }
extern "C" WGPURenderPipeline
wgpuDeviceCreateRenderPipeline(WGPUDevice, const WGPURenderPipelineDescriptor* descriptor) {
  return descriptor != nullptr && descriptor->layout != nullptr &&
                 descriptor->vertex.module != nullptr && descriptor->fragment != nullptr
             ? new WGPURenderPipelineImpl
             : nullptr;
}
extern "C" void wgpuRenderPipelineRelease(WGPURenderPipeline pipeline) { delete pipeline; }

extern "C" WGPUCommandEncoder wgpuDeviceCreateCommandEncoder(WGPUDevice,
                                                             const WGPUCommandEncoderDescriptor*) {
  return new WGPUCommandEncoderImpl;
}
extern "C" void wgpuCommandEncoderRelease(WGPUCommandEncoder encoder) { delete encoder; }
extern "C" void wgpuCommandEncoderCopyBufferToTexture(WGPUCommandEncoder,
                                                      const WGPUTexelCopyBufferInfo* source,
                                                      const WGPUTexelCopyTextureInfo* destination,
                                                      const WGPUExtent3D* size) {
  for (unsigned int row = 0; row < size->height; ++row) {
    std::memcpy(destination->texture->bytes.data() +
                    static_cast<std::size_t>(row) * destination->texture->width * 4,
                source->buffer->bytes.data() + source->layout.offset +
                    static_cast<std::size_t>(row) * source->layout.bytesPerRow,
                static_cast<std::size_t>(size->width) * 4);
  }
}
extern "C" void wgpuCommandEncoderCopyTextureToBuffer(WGPUCommandEncoder,
                                                      const WGPUTexelCopyTextureInfo* source,
                                                      const WGPUTexelCopyBufferInfo* destination,
                                                      const WGPUExtent3D* size) {
  for (unsigned int row = 0; row < size->height; ++row) {
    std::memcpy(destination->buffer->bytes.data() + destination->layout.offset +
                    static_cast<std::size_t>(row) * destination->layout.bytesPerRow,
                source->texture->bytes.data() +
                    static_cast<std::size_t>(row) * source->texture->width * 4,
                static_cast<std::size_t>(size->width) * 4);
  }
}
extern "C" WGPURenderPassEncoder
wgpuCommandEncoderBeginRenderPass(WGPUCommandEncoder encoder,
                                  const WGPURenderPassDescriptor* descriptor) {
  if (encoder == nullptr || encoder->finished || descriptor == nullptr ||
      descriptor->colorAttachmentCount != 1 || descriptor->colorAttachments == nullptr ||
      descriptor->colorAttachments[0].view == nullptr) {
    return nullptr;
  }
  const auto texture = descriptor->colorAttachments[0].view->texture;
  for (std::size_t index = 0; index < texture->bytes.size(); index += 4) {
    texture->bytes[index] = 0;
    texture->bytes[index + 1] = 0;
    texture->bytes[index + 2] = 0;
    texture->bytes[index + 3] = 255;
  }
  return new WGPURenderPassEncoderImpl{texture};
}
extern "C" void wgpuRenderPassEncoderSetPipeline(WGPURenderPassEncoder, WGPURenderPipeline) {}
extern "C" void wgpuRenderPassEncoderSetBindGroup(WGPURenderPassEncoder, unsigned int,
                                                  WGPUBindGroup, size_t, const unsigned int*) {}
extern "C" void wgpuRenderPassEncoderDraw(WGPURenderPassEncoder pass, unsigned int, unsigned int,
                                          unsigned int, unsigned int) {
  const auto width = pass->target->width;
  const auto height = pass->target->height;
  for (unsigned int y = 0; y < height; ++y) {
    for (unsigned int x = 0; x < width; ++x) {
      const auto nx = 2.0 * (static_cast<double>(x) + 0.5) / width - 1.0;
      const auto ny = 2.0 * (static_cast<double>(y) + 0.5) / height - 1.0;
      if (ny < -0.7 || ny > 0.7 || nx < -(ny + 0.7) / 2.0 || nx > (ny + 0.7) / 2.0)
        continue;
      const auto index = (static_cast<std::size_t>(y) * width + x) * 4;
      pass->target->bytes[index] = 51;
      pass->target->bytes[index + 1] = 179;
      pass->target->bytes[index + 2] = 102;
      pass->target->bytes[index + 3] = 255;
    }
  }
}

extern "C" void wgpuRenderPassEncoderSetVertexBuffer(WGPURenderPassEncoder, unsigned int,
                                                     WGPUBuffer, uint64_t, uint64_t) {}
extern "C" void wgpuRenderPassEncoderSetIndexBuffer(WGPURenderPassEncoder, WGPUBuffer,
                                                    WGPUIndexFormat, uint64_t, uint64_t) {}
extern "C" void wgpuRenderPassEncoderDrawIndexed(WGPURenderPassEncoder pass, unsigned int count,
                                                 unsigned int instances, unsigned int first, int,
                                                 unsigned int first_instance) {
  wgpuRenderPassEncoderDraw(pass, count, instances, first, first_instance);
}
extern "C" void wgpuRenderPassEncoderEnd(WGPURenderPassEncoder) {}
extern "C" void wgpuRenderPassEncoderRelease(WGPURenderPassEncoder pass) { delete pass; }
extern "C" WGPUCommandBuffer wgpuCommandEncoderFinish(WGPUCommandEncoder encoder,
                                                      const WGPUCommandBufferDescriptor*) {
  if (encoder == nullptr || encoder->finished)
    return nullptr;
  encoder->finished = true;
  return new WGPUCommandBufferImpl;
}
extern "C" void wgpuCommandBufferRelease(WGPUCommandBuffer command_buffer) {
  delete command_buffer;
}
extern "C" void wgpuQueueSubmit(WGPUQueue, size_t, const WGPUCommandBuffer*) {}
