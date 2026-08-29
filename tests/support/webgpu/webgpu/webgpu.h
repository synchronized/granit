// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_TEST_WEBGPU_H_
#define GRANIT_TEST_WEBGPU_H_

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct WGPUInstanceImpl* WGPUInstance;
typedef struct WGPUAdapterImpl* WGPUAdapter;
typedef struct WGPUDeviceImpl* WGPUDevice;
typedef struct WGPUQueueImpl* WGPUQueue;
typedef struct WGPUBufferImpl* WGPUBuffer;
typedef struct WGPUTextureImpl* WGPUTexture;
typedef struct WGPUTextureViewImpl* WGPUTextureView;
typedef struct WGPUSamplerImpl* WGPUSampler;
typedef struct WGPUBindGroupLayoutImpl* WGPUBindGroupLayout;
typedef struct WGPUBindGroupImpl* WGPUBindGroup;
typedef struct WGPUPipelineLayoutImpl* WGPUPipelineLayout;
typedef struct WGPUShaderModuleImpl* WGPUShaderModule;
typedef struct WGPURenderPipelineImpl* WGPURenderPipeline;
typedef struct WGPUCommandEncoderImpl* WGPUCommandEncoder;
typedef struct WGPUCommandBufferImpl* WGPUCommandBuffer;
typedef struct WGPURenderPassEncoderImpl* WGPURenderPassEncoder;
typedef struct WGPUSurfaceImpl* WGPUSurface;

typedef unsigned int WGPUBool;
typedef unsigned int WGPUInstanceFeatureName;
typedef unsigned int WGPUCallbackMode;
typedef unsigned int WGPURequestAdapterStatus;
typedef unsigned int WGPURequestDeviceStatus;
typedef unsigned int WGPUWaitStatus;
typedef unsigned int WGPUStatus;
typedef unsigned int WGPUMapAsyncStatus;
typedef unsigned long long WGPUBufferUsage;
typedef unsigned long long WGPUMapMode;
typedef unsigned long long WGPUTextureUsage;
typedef unsigned int WGPUTextureDimension;
typedef unsigned int WGPUTextureFormat;
typedef unsigned int WGPUAddressMode;
typedef unsigned int WGPUFilterMode;
typedef unsigned int WGPUMipmapFilterMode;
typedef unsigned int WGPUCompareFunction;
typedef unsigned long long WGPUShaderStage;
typedef unsigned int WGPUSamplerBindingType;
typedef unsigned int WGPUTextureSampleType;
typedef unsigned int WGPUTextureViewDimension;
typedef unsigned int WGPUSType;
typedef unsigned long long WGPUColorWriteMask;
typedef unsigned int WGPUPrimitiveTopology;
typedef unsigned int WGPUVertexFormat;
typedef unsigned int WGPUVertexStepMode;
typedef unsigned int WGPUTextureAspect;
typedef unsigned int WGPULoadOp;
typedef unsigned int WGPUStoreOp;
typedef unsigned int WGPUDeviceLostReason;
typedef unsigned int WGPUPresentMode;
typedef unsigned int WGPUCompositeAlphaMode;
typedef unsigned int WGPUSurfaceGetCurrentTextureStatus;

#define WGPU_FALSE 0
#define WGPU_TRUE 1
#define WGPU_STRLEN ((size_t)-1)
#define WGPUInstanceFeatureName_TimedWaitAny 1
#define WGPUCallbackMode_WaitAnyOnly 1
#define WGPUCallbackMode_AllowSpontaneous 2
#define WGPUDeviceLostReason_Unknown 1
#define WGPUDeviceLostReason_Destroyed 2
#define WGPUDeviceLostReason_CallbackCancelled 3
#define WGPUDeviceLostReason_FailedCreation 4
#define WGPURequestAdapterStatus_Success 1
#define WGPURequestDeviceStatus_Success 1
#define WGPUWaitStatus_Success 1
#define WGPUStatus_Success 1
#define WGPUStatus_Error 2
#define WGPUMapAsyncStatus_Success 1
#define WGPUMapAsyncStatus_Error 3
#define WGPUBufferUsage_None 0
#define WGPUBufferUsage_MapRead 1
#define WGPUBufferUsage_CopySrc 4
#define WGPUBufferUsage_CopyDst 8
#define WGPUBufferUsage_Index 16
#define WGPUBufferUsage_Vertex 32
#define WGPUMapMode_Read 1
#define WGPUTextureUsage_None 0
#define WGPUTextureUsage_CopySrc 1
#define WGPUTextureUsage_CopyDst 2
#define WGPUTextureUsage_TextureBinding 4
#define WGPUTextureUsage_RenderAttachment 16
#define WGPUTextureDimension_2D 2
#define WGPUTextureFormat_RGBA8Unorm 18
#define WGPUTextureFormat_BGRA8Unorm 23
#define WGPUAddressMode_ClampToEdge 1
#define WGPUFilterMode_Nearest 1
#define WGPUFilterMode_Linear 2
#define WGPUMipmapFilterMode_Nearest 1
#define WGPUShaderStage_Fragment 2
#define WGPUSamplerBindingType_Filtering 2
#define WGPUTextureSampleType_Float 2
#define WGPUTextureViewDimension_2D 2
#define WGPUSType_ShaderSourceWGSL 6
#define WGPUColorWriteMask_All 15
#define WGPUPrimitiveTopology_TriangleList 4
#define WGPUVertexStepMode_Vertex 1
#define WGPUVertexStepMode_Instance 2
#define WGPUVertexFormat_Undefined 0
#define WGPUVertexFormat_Uint32 12
#define WGPUVertexFormat_Uint32x2 13
#define WGPUVertexFormat_Uint32x3 14
#define WGPUVertexFormat_Uint32x4 15
#define WGPUVertexFormat_Sint32 16
#define WGPUVertexFormat_Sint32x2 17
#define WGPUVertexFormat_Sint32x3 18
#define WGPUVertexFormat_Sint32x4 19
#define WGPUVertexFormat_Float32 28
#define WGPUVertexFormat_Float32x2 29
#define WGPUVertexFormat_Float32x3 30
#define WGPUVertexFormat_Float32x4 31
#define WGPUTextureAspect_All 1
#define WGPULoadOp_Clear 2
#define WGPUStoreOp_Store 1
#define WGPUBackendType_D3D12 4
#define WGPUBackendType_Vulkan 6
#define WGPUSType_EmscriptenSurfaceSourceCanvasHTMLSelector 0x00050000
#define WGPUSType_SurfaceSourceWindowsHWND 0x00000005
#define WGPUSType_SurfaceSourceWaylandSurface 0x00000007
#define WGPUSType_SurfaceSourceXCBWindow 0x00000009
#define WGPUPresentMode_Fifo 1
#define WGPUPresentMode_Immediate 2
#define WGPUPresentMode_Mailbox 3
#define WGPUCompositeAlphaMode_Auto 1
#define WGPUSurfaceGetCurrentTextureStatus_SuccessOptimal 1
#define WGPUSurfaceGetCurrentTextureStatus_SuccessSuboptimal 2
#define WGPUSurfaceGetCurrentTextureStatus_Timeout 3
#define WGPUSurfaceGetCurrentTextureStatus_Outdated 4
#define WGPUSurfaceGetCurrentTextureStatus_Lost 5
#define WGPUSurfaceGetCurrentTextureStatus_Error 6

typedef struct WGPUStringView {
  const char* data;
  size_t length;
} WGPUStringView;

typedef struct WGPUChainedStruct {
  const struct WGPUChainedStruct* next;
  WGPUSType sType;
} WGPUChainedStruct;

typedef struct WGPUEmscriptenSurfaceSourceCanvasHTMLSelector {
  WGPUChainedStruct chain;
  WGPUStringView selector;
} WGPUEmscriptenSurfaceSourceCanvasHTMLSelector;

typedef struct WGPUSurfaceSourceWindowsHWND {
  WGPUChainedStruct chain;
  void* hinstance;
  void* hwnd;
} WGPUSurfaceSourceWindowsHWND;

typedef struct WGPUSurfaceSourceXCBWindow {
  WGPUChainedStruct chain;
  void* connection;
  uint32_t window;
} WGPUSurfaceSourceXCBWindow;

typedef struct WGPUSurfaceSourceWaylandSurface {
  WGPUChainedStruct chain;
  void* display;
  void* surface;
} WGPUSurfaceSourceWaylandSurface;

typedef struct WGPUSurfaceDescriptor {
  const WGPUChainedStruct* nextInChain;
  WGPUStringView label;
} WGPUSurfaceDescriptor;

typedef struct WGPUSurfaceCapabilities {
  void* nextInChain;
  WGPUTextureUsage usages;
  size_t formatCount;
  const WGPUTextureFormat* formats;
  size_t presentModeCount;
  const WGPUPresentMode* presentModes;
  size_t alphaModeCount;
  const WGPUCompositeAlphaMode* alphaModes;
} WGPUSurfaceCapabilities;
#define WGPU_SURFACE_CAPABILITIES_INIT                                                             \
  {                                                                                                \
  }

typedef struct WGPUSurfaceConfiguration {
  void* nextInChain;
  WGPUDevice device;
  WGPUTextureFormat format;
  WGPUTextureUsage usage;
  unsigned int width;
  unsigned int height;
  WGPUPresentMode presentMode;
  WGPUCompositeAlphaMode alphaMode;
  size_t viewFormatCount;
  const WGPUTextureFormat* viewFormats;
} WGPUSurfaceConfiguration;
#define WGPU_SURFACE_CONFIGURATION_INIT                                                            \
  {                                                                                                \
  }

typedef struct WGPUSurfaceTexture {
  WGPUTexture texture;
  WGPUSurfaceGetCurrentTextureStatus status;
} WGPUSurfaceTexture;

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

typedef struct WGPUBufferDescriptor {
  void* nextInChain;
  WGPUStringView label;
  WGPUBufferUsage usage;
  unsigned long long size;
  WGPUBool mappedAtCreation;
} WGPUBufferDescriptor;

#define WGPU_BUFFER_DESCRIPTOR_INIT                                                                \
  {                                                                                                \
  }

typedef struct WGPUExtent3D {
  unsigned int width;
  unsigned int height;
  unsigned int depthOrArrayLayers;
} WGPUExtent3D;

typedef struct WGPUTextureDescriptor {
  void* nextInChain;
  WGPUStringView label;
  WGPUTextureUsage usage;
  WGPUTextureDimension dimension;
  WGPUExtent3D size;
  WGPUTextureFormat format;
  unsigned int mipLevelCount;
  unsigned int sampleCount;
  size_t viewFormatCount;
  const WGPUTextureFormat* viewFormats;
} WGPUTextureDescriptor;

#define WGPU_TEXTURE_DESCRIPTOR_INIT                                                               \
  {                                                                                                \
  }

typedef struct WGPUTextureViewDescriptor WGPUTextureViewDescriptor;

typedef struct WGPUSamplerDescriptor {
  void* nextInChain;
  WGPUStringView label;
  WGPUAddressMode addressModeU;
  WGPUAddressMode addressModeV;
  WGPUAddressMode addressModeW;
  WGPUFilterMode magFilter;
  WGPUFilterMode minFilter;
  WGPUMipmapFilterMode mipmapFilter;
  float lodMinClamp;
  float lodMaxClamp;
  WGPUCompareFunction compare;
  unsigned short maxAnisotropy;
} WGPUSamplerDescriptor;

#define WGPU_SAMPLER_DESCRIPTOR_INIT                                                               \
  {                                                                                                \
  }

typedef struct WGPUBufferBindingLayout {
  unsigned int type;
  WGPUBool hasDynamicOffset;
  unsigned long long minBindingSize;
} WGPUBufferBindingLayout;
typedef struct WGPUSamplerBindingLayout {
  WGPUSamplerBindingType type;
} WGPUSamplerBindingLayout;
typedef struct WGPUTextureBindingLayout {
  WGPUTextureSampleType sampleType;
  WGPUTextureViewDimension viewDimension;
  WGPUBool multisampled;
} WGPUTextureBindingLayout;
typedef struct WGPUStorageTextureBindingLayout {
  unsigned int access;
  WGPUTextureFormat format;
  WGPUTextureViewDimension viewDimension;
} WGPUStorageTextureBindingLayout;
typedef struct WGPUBindGroupLayoutEntry {
  void* nextInChain;
  unsigned int binding;
  WGPUShaderStage visibility;
  unsigned int bindingArraySize;
  WGPUBufferBindingLayout buffer;
  WGPUSamplerBindingLayout sampler;
  WGPUTextureBindingLayout texture;
  WGPUStorageTextureBindingLayout storageTexture;
} WGPUBindGroupLayoutEntry;
#define WGPU_BIND_GROUP_LAYOUT_ENTRY_INIT                                                          \
  {                                                                                                \
  }
typedef struct WGPUBindGroupLayoutDescriptor {
  void* nextInChain;
  WGPUStringView label;
  size_t entryCount;
  const WGPUBindGroupLayoutEntry* entries;
} WGPUBindGroupLayoutDescriptor;
#define WGPU_BIND_GROUP_LAYOUT_DESCRIPTOR_INIT                                                     \
  {                                                                                                \
  }
typedef struct WGPUBindGroupEntry {
  void* nextInChain;
  unsigned int binding;
  WGPUBuffer buffer;
  unsigned long long offset;
  unsigned long long size;
  WGPUSampler sampler;
  WGPUTextureView textureView;
} WGPUBindGroupEntry;
#define WGPU_BIND_GROUP_ENTRY_INIT                                                                 \
  {                                                                                                \
  }
typedef struct WGPUBindGroupDescriptor {
  void* nextInChain;
  WGPUStringView label;
  WGPUBindGroupLayout layout;
  size_t entryCount;
  const WGPUBindGroupEntry* entries;
} WGPUBindGroupDescriptor;
#define WGPU_BIND_GROUP_DESCRIPTOR_INIT                                                            \
  {                                                                                                \
  }
typedef struct WGPUPipelineLayoutDescriptor {
  void* nextInChain;
  WGPUStringView label;
  size_t bindGroupLayoutCount;
  const WGPUBindGroupLayout* bindGroupLayouts;
} WGPUPipelineLayoutDescriptor;
#define WGPU_PIPELINE_LAYOUT_DESCRIPTOR_INIT                                                       \
  {                                                                                                \
  }
typedef struct WGPUShaderSourceWGSL {
  WGPUChainedStruct chain;
  WGPUStringView code;
} WGPUShaderSourceWGSL;
#define WGPU_SHADER_SOURCE_WGSL_INIT {{NULL, WGPUSType_ShaderSourceWGSL}, {NULL, 0}}
typedef struct WGPUShaderModuleDescriptor {
  const WGPUChainedStruct* nextInChain;
  WGPUStringView label;
} WGPUShaderModuleDescriptor;
#define WGPU_SHADER_MODULE_DESCRIPTOR_INIT                                                         \
  {                                                                                                \
  }
typedef struct WGPUColorTargetState {
  void* nextInChain;
  WGPUTextureFormat format;
  const void* blend;
  WGPUColorWriteMask writeMask;
} WGPUColorTargetState;
#define WGPU_COLOR_TARGET_STATE_INIT                                                               \
  {                                                                                                \
  }
typedef struct WGPUVertexAttribute {
  WGPUVertexFormat format;
  unsigned long long offset;
  unsigned int shaderLocation;
} WGPUVertexAttribute;
typedef struct WGPUVertexBufferLayout {
  unsigned long long arrayStride;
  WGPUVertexStepMode stepMode;
  size_t attributeCount;
  const WGPUVertexAttribute* attributes;
} WGPUVertexBufferLayout;
typedef struct WGPUVertexState {
  void* nextInChain;
  WGPUShaderModule module;
  WGPUStringView entryPoint;
  size_t constantCount;
  const void* constants;
  size_t bufferCount;
  const WGPUVertexBufferLayout* buffers;
} WGPUVertexState;
typedef struct WGPUFragmentState {
  void* nextInChain;
  WGPUShaderModule module;
  WGPUStringView entryPoint;
  size_t constantCount;
  const void* constants;
  size_t targetCount;
  const WGPUColorTargetState* targets;
} WGPUFragmentState;
#define WGPU_FRAGMENT_STATE_INIT                                                                   \
  {                                                                                                \
  }
typedef struct WGPUPrimitiveState {
  void* nextInChain;
  WGPUPrimitiveTopology topology;
  unsigned int stripIndexFormat;
  unsigned int frontFace;
  unsigned int cullMode;
  WGPUBool unclippedDepth;
} WGPUPrimitiveState;
typedef struct WGPUMultisampleState {
  void* nextInChain;
  unsigned int count;
  unsigned int mask;
  WGPUBool alphaToCoverageEnabled;
} WGPUMultisampleState;
typedef struct WGPURenderPipelineDescriptor {
  void* nextInChain;
  WGPUStringView label;
  WGPUPipelineLayout layout;
  WGPUVertexState vertex;
  WGPUPrimitiveState primitive;
  const void* depthStencil;
  WGPUMultisampleState multisample;
  const WGPUFragmentState* fragment;
} WGPURenderPipelineDescriptor;
#define WGPU_RENDER_PIPELINE_DESCRIPTOR_INIT                                                       \
  {                                                                                                \
  }

typedef struct WGPUCommandEncoderDescriptor {
  void* nextInChain;
  WGPUStringView label;
} WGPUCommandEncoderDescriptor;
#define WGPU_COMMAND_ENCODER_DESCRIPTOR_INIT                                                       \
  {                                                                                                \
  }
typedef struct WGPUTexelCopyBufferLayout {
  unsigned long long offset;
  unsigned int bytesPerRow;
  unsigned int rowsPerImage;
} WGPUTexelCopyBufferLayout;
typedef struct WGPUTexelCopyBufferInfo {
  void* nextInChain;
  WGPUTexelCopyBufferLayout layout;
  WGPUBuffer buffer;
} WGPUTexelCopyBufferInfo;
#define WGPU_TEXEL_COPY_BUFFER_INFO_INIT                                                           \
  {                                                                                                \
  }
typedef struct WGPUOrigin3D {
  unsigned int x;
  unsigned int y;
  unsigned int z;
} WGPUOrigin3D;
typedef struct WGPUTexelCopyTextureInfo {
  void* nextInChain;
  WGPUTexture texture;
  unsigned int mipLevel;
  WGPUOrigin3D origin;
  WGPUTextureAspect aspect;
} WGPUTexelCopyTextureInfo;
#define WGPU_TEXEL_COPY_TEXTURE_INFO_INIT                                                          \
  {                                                                                                \
  }
typedef struct WGPUColor {
  double r;
  double g;
  double b;
  double a;
} WGPUColor;
typedef struct WGPURenderPassColorAttachment {
  void* nextInChain;
  WGPUTextureView view;
  unsigned int depthSlice;
  WGPUTextureView resolveTarget;
  WGPULoadOp loadOp;
  WGPUStoreOp storeOp;
  WGPUColor clearValue;
} WGPURenderPassColorAttachment;
#define WGPU_RENDER_PASS_COLOR_ATTACHMENT_INIT                                                     \
  {                                                                                                \
  }
typedef struct WGPURenderPassDescriptor {
  void* nextInChain;
  WGPUStringView label;
  size_t colorAttachmentCount;
  const WGPURenderPassColorAttachment* colorAttachments;
  const void* depthStencilAttachment;
  const void* occlusionQuerySet;
  const void* timestampWrites;
} WGPURenderPassDescriptor;
#define WGPU_RENDER_PASS_DESCRIPTOR_INIT                                                           \
  {                                                                                                \
  }
typedef struct WGPUCommandBufferDescriptor {
  void* nextInChain;
  WGPUStringView label;
} WGPUCommandBufferDescriptor;
#define WGPU_COMMAND_BUFFER_DESCRIPTOR_INIT                                                        \
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
typedef void (*WGPUDeviceLostCallback)(const WGPUDevice*, WGPUDeviceLostReason, WGPUStringView,
                                       void*, void*);
typedef void (*WGPUBufferMapCallback)(WGPUMapAsyncStatus, WGPUStringView, void*, void*);

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

typedef struct WGPUDeviceLostCallbackInfo {
  void* nextInChain;
  WGPUCallbackMode mode;
  WGPUDeviceLostCallback callback;
  void* userdata1;
  void* userdata2;
} WGPUDeviceLostCallbackInfo;

typedef struct WGPUDeviceDescriptor {
  void* nextInChain;
  WGPUStringView label;
  size_t requiredFeatureCount;
  const void* requiredFeatures;
  const WGPULimits* requiredLimits;
  char defaultQueue[32];
  WGPUDeviceLostCallbackInfo deviceLostCallbackInfo;
  char uncapturedErrorCallbackInfo[40];
} WGPUDeviceDescriptor;
#define WGPU_DEVICE_DESCRIPTOR_INIT                                                                \
  {                                                                                                \
  }

typedef struct WGPUBufferMapCallbackInfo {
  void* nextInChain;
  WGPUCallbackMode mode;
  WGPUBufferMapCallback callback;
  void* userdata1;
  void* userdata2;
} WGPUBufferMapCallbackInfo;

WGPUInstance wgpuCreateInstance(const WGPUInstanceDescriptor* descriptor);
WGPUSurface wgpuInstanceCreateSurface(WGPUInstance instance,
                                      const WGPUSurfaceDescriptor* descriptor);
void wgpuSurfaceRelease(WGPUSurface surface);
WGPUStatus wgpuSurfaceGetCapabilities(WGPUSurface surface, WGPUAdapter adapter,
                                      WGPUSurfaceCapabilities* capabilities);
void wgpuSurfaceCapabilitiesFreeMembers(WGPUSurfaceCapabilities capabilities);
void wgpuSurfaceConfigure(WGPUSurface surface, const WGPUSurfaceConfiguration* configuration);
void wgpuSurfaceUnconfigure(WGPUSurface surface);
void wgpuSurfaceGetCurrentTexture(WGPUSurface surface, WGPUSurfaceTexture* surfaceTexture);
WGPUStatus wgpuSurfacePresent(WGPUSurface surface);
void wgpuInstanceRelease(WGPUInstance instance);
void wgpuInstanceProcessEvents(WGPUInstance instance);
WGPUFuture wgpuInstanceRequestAdapter(WGPUInstance instance,
                                      const WGPURequestAdapterOptions* options,
                                      WGPURequestAdapterCallbackInfo callbackInfo);
WGPUWaitStatus wgpuInstanceWaitAny(WGPUInstance instance, size_t futureCount,
                                   WGPUFutureWaitInfo* futures, unsigned long long timeoutNS);
WGPUFuture wgpuAdapterRequestDevice(WGPUAdapter adapter, const WGPUDeviceDescriptor* descriptor,
                                    WGPURequestDeviceCallbackInfo callbackInfo);
void wgpuAdapterRelease(WGPUAdapter adapter);
void wgpuDeviceRelease(WGPUDevice device);
void wgpuDeviceForceLoss(WGPUDevice device, WGPUDeviceLostReason reason, WGPUStringView message);
WGPUStatus wgpuDeviceGetLimits(WGPUDevice device, WGPULimits* limits);
WGPUQueue wgpuDeviceGetQueue(WGPUDevice device);
void wgpuQueueRelease(WGPUQueue queue);
WGPUBuffer wgpuDeviceCreateBuffer(WGPUDevice device, const WGPUBufferDescriptor* descriptor);
void wgpuBufferRelease(WGPUBuffer buffer);
void wgpuQueueWriteBuffer(WGPUQueue queue, WGPUBuffer buffer, unsigned long long offset,
                          const void* data, size_t size);
WGPUFuture wgpuBufferMapAsync(WGPUBuffer buffer, WGPUMapMode mode, size_t offset, size_t size,
                              WGPUBufferMapCallbackInfo callbackInfo);
const void* wgpuBufferGetConstMappedRange(WGPUBuffer buffer, size_t offset, size_t size);
void wgpuBufferUnmap(WGPUBuffer buffer);
WGPUTexture wgpuDeviceCreateTexture(WGPUDevice device, const WGPUTextureDescriptor* descriptor);
void wgpuTextureRelease(WGPUTexture texture);
WGPUTextureView wgpuTextureCreateView(WGPUTexture texture,
                                      const WGPUTextureViewDescriptor* descriptor);
void wgpuTextureViewRelease(WGPUTextureView view);
WGPUSampler wgpuDeviceCreateSampler(WGPUDevice device, const WGPUSamplerDescriptor* descriptor);
void wgpuSamplerRelease(WGPUSampler sampler);
WGPUBindGroupLayout
wgpuDeviceCreateBindGroupLayout(WGPUDevice device, const WGPUBindGroupLayoutDescriptor* descriptor);
void wgpuBindGroupLayoutRelease(WGPUBindGroupLayout layout);
WGPUBindGroup wgpuDeviceCreateBindGroup(WGPUDevice device,
                                        const WGPUBindGroupDescriptor* descriptor);
void wgpuBindGroupRelease(WGPUBindGroup bindGroup);
WGPUPipelineLayout wgpuDeviceCreatePipelineLayout(WGPUDevice device,
                                                  const WGPUPipelineLayoutDescriptor* descriptor);
void wgpuPipelineLayoutRelease(WGPUPipelineLayout layout);
WGPUShaderModule wgpuDeviceCreateShaderModule(WGPUDevice device,
                                              const WGPUShaderModuleDescriptor* descriptor);
void wgpuShaderModuleRelease(WGPUShaderModule shaderModule);
WGPURenderPipeline wgpuDeviceCreateRenderPipeline(WGPUDevice device,
                                                  const WGPURenderPipelineDescriptor* descriptor);
void wgpuRenderPipelineRelease(WGPURenderPipeline pipeline);
WGPUCommandEncoder wgpuDeviceCreateCommandEncoder(WGPUDevice device,
                                                  const WGPUCommandEncoderDescriptor* descriptor);
void wgpuCommandEncoderRelease(WGPUCommandEncoder encoder);
void wgpuCommandEncoderCopyBufferToTexture(WGPUCommandEncoder encoder,
                                           const WGPUTexelCopyBufferInfo* source,
                                           const WGPUTexelCopyTextureInfo* destination,
                                           const WGPUExtent3D* copySize);
void wgpuCommandEncoderCopyTextureToBuffer(WGPUCommandEncoder encoder,
                                           const WGPUTexelCopyTextureInfo* source,
                                           const WGPUTexelCopyBufferInfo* destination,
                                           const WGPUExtent3D* copySize);
WGPURenderPassEncoder wgpuCommandEncoderBeginRenderPass(WGPUCommandEncoder encoder,
                                                        const WGPURenderPassDescriptor* descriptor);
void wgpuRenderPassEncoderSetPipeline(WGPURenderPassEncoder pass, WGPURenderPipeline pipeline);
void wgpuRenderPassEncoderSetBindGroup(WGPURenderPassEncoder pass, unsigned int groupIndex,
                                       WGPUBindGroup group, size_t dynamicOffsetCount,
                                       const unsigned int* dynamicOffsets);
void wgpuRenderPassEncoderDraw(WGPURenderPassEncoder pass, unsigned int vertexCount,
                               unsigned int instanceCount, unsigned int firstVertex,
                               unsigned int firstInstance);
void wgpuRenderPassEncoderEnd(WGPURenderPassEncoder pass);
void wgpuRenderPassEncoderRelease(WGPURenderPassEncoder pass);
WGPUCommandBuffer wgpuCommandEncoderFinish(WGPUCommandEncoder encoder,
                                           const WGPUCommandBufferDescriptor* descriptor);
void wgpuCommandBufferRelease(WGPUCommandBuffer commandBuffer);
void wgpuQueueSubmit(WGPUQueue queue, size_t commandCount, const WGPUCommandBuffer* commands);

#ifdef __cplusplus
}
#endif

#endif
