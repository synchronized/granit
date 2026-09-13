// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "backend/webgpu/device.h"
#include "backend/webgpu/device_state.h"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <mutex>
#include <new>
#include <string>
#include <utility>

#include <webgpu/webgpu.h>

namespace {

using granit::detail::webgpu_device_state;
using namespace granit::detail::webgpu_native;

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

} // namespace

namespace granit::detail {

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
} // namespace granit::detail
