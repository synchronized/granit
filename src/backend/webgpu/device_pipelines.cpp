// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "backend/webgpu/device.h"
#include "backend/webgpu/device_state.h"
#include "backend/webgpu/device_utils.h"

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <memory>
#include <mutex>
#include <new>
#include <string>
#include <utility>
#include <vector>

#include <webgpu/webgpu.h>

namespace {

using granit::detail::webgpu_device_state;
using namespace granit::detail::webgpu_native;

struct pipeline_warmup_request {
  std::shared_ptr<webgpu_device_state::pipeline_warmup_record> warmup;
  webgpu_host_api host{};
  WGPUInstance instance{};
};

void emit(const webgpu_host_api& host, granit_diagnostic_severity severity, const char* message,
          std::uint32_t message_length) noexcept {
  if (host.diagnostic_callback == nullptr)
    return;
  try {
    host.diagnostic_callback(severity, GRANIT_DIAGNOSTIC_CATEGORY_DEVICE, message, message_length,
                             host.diagnostic_user_data);
  } catch (...) {
  }
}

void emit_dawn_message(const webgpu_host_api* host, WGPUStringView message) noexcept {
  if (host == nullptr || message.data == nullptr)
    return;
  const auto length = message.length == WGPU_STRLEN ? std::strlen(message.data) : message.length;
  const auto bounded_length = static_cast<std::uint32_t>(
      (std::min)(length, static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max())));
  emit(*host, GRANIT_DIAGNOSTIC_SEVERITY_ERROR, message.data, bounded_length);
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

} // namespace

namespace granit::detail {

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

} // namespace granit::detail
