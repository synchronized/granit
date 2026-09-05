// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "backend/webgpu/provider_dispatch.h"

#include <algorithm>
#include <cstddef>
#include <new>

namespace granit::detail {
namespace {

bool is_compatible(const granit_webgpu_provider_api* api,
                   granit_webgpu_provider_kind expected_kind) noexcept {
  constexpr std::size_t minimum_size = offsetof(granit_webgpu_provider_api, instance_api) +
                                       sizeof(const granit_webgpu_provider_instance_api*);
  constexpr std::size_t minimum_instance_api_size =
      offsetof(granit_webgpu_provider_instance_api, recorder_generate_mipmaps) +
      sizeof(granit_webgpu_provider_recorder_generate_mipmaps_fn);
  return api != nullptr && api->struct_size >= minimum_size &&
         api->abi_version == GRANIT_WEBGPU_PROVIDER_ABI_VERSION && api->kind == expected_kind &&
         api->reserved == 0 && api->name != nullptr && api->name_length != 0 &&
         api->create != nullptr && api->destroy != nullptr && api->instance_api != nullptr &&
         api->instance_api->struct_size >= minimum_instance_api_size &&
         api->instance_api->reserved == 0 && api->instance_api->get_capabilities != nullptr &&
         api->instance_api->create_buffer != nullptr &&
         api->instance_api->destroy_buffer != nullptr &&
         api->instance_api->write_buffer != nullptr && api->instance_api->read_buffer != nullptr &&
         api->instance_api->write_texture != nullptr &&
         api->instance_api->create_texture != nullptr &&
         api->instance_api->destroy_texture != nullptr &&
         api->instance_api->create_texture_view != nullptr &&
         api->instance_api->destroy_texture_view != nullptr &&
         api->instance_api->create_sampler != nullptr &&
         api->instance_api->destroy_sampler != nullptr &&
         api->instance_api->create_bind_group_layout != nullptr &&
         api->instance_api->destroy_bind_group_layout != nullptr &&
         api->instance_api->create_bind_group != nullptr &&
         api->instance_api->destroy_bind_group != nullptr &&
         api->instance_api->create_shader != nullptr &&
         api->instance_api->destroy_shader != nullptr &&
         api->instance_api->create_pipeline_layout != nullptr &&
         api->instance_api->destroy_pipeline_layout != nullptr &&
         api->instance_api->create_render_pipeline != nullptr &&
         api->instance_api->destroy_render_pipeline != nullptr &&
         api->instance_api->create_command_recorder != nullptr &&
         api->instance_api->destroy_command_recorder != nullptr &&
         api->instance_api->recorder_copy_buffer_to_texture != nullptr &&
         api->instance_api->finish_command_recorder != nullptr &&
         api->instance_api->destroy_command_buffer != nullptr &&
         api->instance_api->submit_command_buffer != nullptr &&
         api->instance_api->recorder_copy_texture_to_buffer != nullptr &&
         api->instance_api->get_instance_status != nullptr &&
         api->instance_api->process_events != nullptr &&
         api->instance_api->create_win32_surface != nullptr &&
         api->instance_api->create_xcb_surface != nullptr &&
         api->instance_api->create_wayland_surface != nullptr &&
         api->instance_api->create_canvas_surface != nullptr &&
         api->instance_api->destroy_surface != nullptr &&
         api->instance_api->create_swapchain != nullptr &&
         api->instance_api->recreate_swapchain != nullptr &&
         api->instance_api->get_swapchain_info != nullptr &&
         api->instance_api->acquire_swapchain != nullptr &&
         api->instance_api->present_swapchain != nullptr &&
         api->instance_api->cancel_swapchain != nullptr &&
         api->instance_api->destroy_swapchain != nullptr &&
         api->instance_api->recorder_begin_rendering != nullptr &&
         api->instance_api->recorder_bind_pipeline != nullptr &&
         api->instance_api->recorder_bind_graphics_groups != nullptr &&
         api->instance_api->recorder_bind_vertex_buffers != nullptr &&
         api->instance_api->recorder_bind_index_buffer != nullptr &&
         api->instance_api->recorder_draw_vertices != nullptr &&
         api->instance_api->recorder_draw_indices != nullptr &&
         api->instance_api->recorder_end_rendering != nullptr &&
         api->instance_api->write_upload_batch != nullptr &&
         api->instance_api->create_compute_pipeline != nullptr &&
         api->instance_api->destroy_compute_pipeline != nullptr &&
         api->instance_api->recorder_begin_compute != nullptr &&
         api->instance_api->recorder_bind_compute_pipeline != nullptr &&
         api->instance_api->recorder_bind_compute_groups != nullptr &&
         api->instance_api->recorder_dispatch != nullptr &&
         api->instance_api->recorder_end_compute != nullptr &&
         api->instance_api->recorder_set_viewports != nullptr &&
         api->instance_api->recorder_set_scissors != nullptr &&
         api->instance_api->recorder_copy_buffer != nullptr &&
         api->instance_api->recorder_copy_buffer_to_texture_v2 != nullptr &&
         api->instance_api->recorder_copy_texture_to_buffer_v2 != nullptr &&
         api->instance_api->recorder_copy_texture != nullptr &&
         api->instance_api->recorder_fill_buffer != nullptr &&
         api->instance_api->recorder_generate_mipmaps != nullptr;
}

bool is_valid_host(const granit_webgpu_provider_host_api* host) noexcept {
  constexpr std::size_t minimum_size =
      offsetof(granit_webgpu_provider_host_api, allocator_user_data) + sizeof(void*);
  return host != nullptr && host->struct_size >= minimum_size && host->reserved == 0 &&
         host->allocate != nullptr && host->deallocate != nullptr;
}

} // namespace

webgpu_provider_dispatch::~webgpu_provider_dispatch() { close(); }

granit_result webgpu_provider_dispatch::connect(const granit_webgpu_provider_api* api) noexcept {
  close();
  if (!is_compatible(api, GRANIT_WEBGPU_PROVIDER_KIND_WEBGPU)) {
    return GRANIT_ERROR_INCOMPATIBLE_DRIVER;
  }
  api_ = api;
  return GRANIT_SUCCESS;
}

granit_result
webgpu_provider_dispatch::create_instance(const granit_webgpu_provider_host_api* host,
                                          granit_webgpu_provider_instance* out_instance) noexcept {
  if (api_ == nullptr || out_instance == nullptr || !is_valid_host(host)) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  *out_instance = 0;

  granit_webgpu_provider_instance instance = 0;
  granit_result result = GRANIT_ERROR_INTERNAL;
  try {
    result = api_->create(host, &instance);
  } catch (...) {
    if (instance != 0) {
      try {
        api_->destroy(instance);
      } catch (...) {
      }
    }
    return GRANIT_ERROR_INTERNAL;
  }
  if (result != GRANIT_SUCCESS) {
    if (instance != 0) {
      try {
        api_->destroy(instance);
      } catch (...) {
      }
    }
    return result;
  }
  if (instance == 0) {
    return GRANIT_ERROR_INITIALIZATION_FAILED;
  }

  try {
    instances_.push_back(instance);
  } catch (const std::bad_alloc&) {
    try {
      api_->destroy(instance);
    } catch (...) {
    }
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    try {
      api_->destroy(instance);
    } catch (...) {
    }
    return GRANIT_ERROR_INTERNAL;
  }
  *out_instance = instance;
  return GRANIT_SUCCESS;
}

granit_result
webgpu_provider_dispatch::destroy_instance(granit_webgpu_provider_instance instance) noexcept {
  if (api_ == nullptr || instance == 0) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  const auto found = std::find(instances_.begin(), instances_.end(), instance);
  if (found == instances_.end()) {
    return GRANIT_ERROR_INVALID_HANDLE;
  }
  try {
    api_->destroy(instance);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
  instances_.erase(found);
  return GRANIT_SUCCESS;
}

granit_result webgpu_provider_dispatch::get_capabilities(
    granit_webgpu_provider_instance instance,
    granit_webgpu_provider_capabilities* capabilities) noexcept {
  constexpr std::size_t minimum_size =
      offsetof(granit_webgpu_provider_capabilities, reserved_2) + sizeof(std::uint32_t);
  if (api_ == nullptr || instance == 0 || capabilities == nullptr ||
      capabilities->struct_size < minimum_size || capabilities->reserved != 0 ||
      capabilities->reserved_2 != 0) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  if (std::find(instances_.begin(), instances_.end(), instance) == instances_.end()) {
    return GRANIT_ERROR_INVALID_HANDLE;
  }
  try {
    return api_->instance_api->get_capabilities(instance, capabilities);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result webgpu_provider_dispatch::get_instance_status(
    granit_webgpu_provider_instance instance,
    granit_webgpu_provider_instance_status* status) noexcept {
  if (api_ == nullptr || instance == 0 || status == nullptr ||
      status->struct_size < sizeof(granit_webgpu_provider_instance_status) ||
      status->reserved != 0) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  if (std::find(instances_.begin(), instances_.end(), instance) == instances_.end()) {
    return GRANIT_ERROR_INVALID_HANDLE;
  }
  try {
    return api_->instance_api->get_instance_status(instance, status);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result
webgpu_provider_dispatch::process_events(granit_webgpu_provider_instance instance) noexcept {
  if (api_ == nullptr || instance == 0) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  if (std::find(instances_.begin(), instances_.end(), instance) == instances_.end()) {
    return GRANIT_ERROR_INVALID_HANDLE;
  }
  try {
    return api_->instance_api->process_events(instance);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result webgpu_provider_dispatch::create_win32_surface(
    granit_webgpu_provider_instance instance, const granit_webgpu_provider_win32_surface_desc* desc,
    granit_webgpu_provider_surface* surface) noexcept {
  if (api_ == nullptr || instance == 0 || desc == nullptr || surface == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  if (std::find(instances_.begin(), instances_.end(), instance) == instances_.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  try {
    return api_->instance_api->create_win32_surface(instance, desc, surface);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result
webgpu_provider_dispatch::create_xcb_surface(granit_webgpu_provider_instance instance,
                                             const granit_webgpu_provider_xcb_surface_desc* desc,
                                             granit_webgpu_provider_surface* surface) noexcept {
  if (api_ == nullptr || instance == 0 || desc == nullptr || surface == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  if (std::find(instances_.begin(), instances_.end(), instance) == instances_.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  try {
    return api_->instance_api->create_xcb_surface(instance, desc, surface);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result webgpu_provider_dispatch::create_wayland_surface(
    granit_webgpu_provider_instance instance,
    const granit_webgpu_provider_wayland_surface_desc* desc,
    granit_webgpu_provider_surface* surface) noexcept {
  if (api_ == nullptr || instance == 0 || desc == nullptr || surface == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  if (std::find(instances_.begin(), instances_.end(), instance) == instances_.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  try {
    return api_->instance_api->create_wayland_surface(instance, desc, surface);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result webgpu_provider_dispatch::create_canvas_surface(
    granit_webgpu_provider_instance instance,
    const granit_webgpu_provider_canvas_surface_desc* desc,
    granit_webgpu_provider_surface* surface) noexcept {
  if (api_ == nullptr || instance == 0 || desc == nullptr || surface == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  if (std::find(instances_.begin(), instances_.end(), instance) == instances_.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  try {
    return api_->instance_api->create_canvas_surface(instance, desc, surface);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result
webgpu_provider_dispatch::destroy_surface(granit_webgpu_provider_instance instance,
                                          granit_webgpu_provider_surface surface) noexcept {
  if (api_ == nullptr || instance == 0 || surface == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  if (std::find(instances_.begin(), instances_.end(), instance) == instances_.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  try {
    return api_->instance_api->destroy_surface(instance, surface);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result
webgpu_provider_dispatch::create_swapchain(granit_webgpu_provider_instance instance,
                                           granit_webgpu_provider_surface surface,
                                           const granit_webgpu_provider_swapchain_desc* desc,
                                           granit_webgpu_provider_swapchain* swapchain) noexcept {
  if (api_ == nullptr || instance == 0 || surface == 0 || desc == nullptr || swapchain == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  if (std::find(instances_.begin(), instances_.end(), instance) == instances_.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  try {
    return api_->instance_api->create_swapchain(instance, surface, desc, swapchain);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result webgpu_provider_dispatch::recreate_swapchain(
    granit_webgpu_provider_instance instance, granit_webgpu_provider_swapchain swapchain,
    const granit_webgpu_provider_swapchain_desc* desc) noexcept {
  if (api_ == nullptr || instance == 0 || swapchain == 0 || desc == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  if (std::find(instances_.begin(), instances_.end(), instance) == instances_.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  try {
    return api_->instance_api->recreate_swapchain(instance, swapchain, desc);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result
webgpu_provider_dispatch::get_swapchain_info(granit_webgpu_provider_instance instance,
                                             granit_webgpu_provider_swapchain swapchain,
                                             granit_webgpu_provider_swapchain_info* info) noexcept {
  if (api_ == nullptr || instance == 0 || swapchain == 0 || info == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  if (std::find(instances_.begin(), instances_.end(), instance) == instances_.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  try {
    return api_->instance_api->get_swapchain_info(instance, swapchain, info);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result
webgpu_provider_dispatch::acquire_swapchain(granit_webgpu_provider_instance instance,
                                            granit_webgpu_provider_swapchain swapchain,
                                            granit_webgpu_provider_acquired_frame* frame) noexcept {
  if (api_ == nullptr || instance == 0 || swapchain == 0 || frame == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  if (std::find(instances_.begin(), instances_.end(), instance) == instances_.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  try {
    return api_->instance_api->acquire_swapchain(instance, swapchain, frame);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result
webgpu_provider_dispatch::present_swapchain(granit_webgpu_provider_instance instance,
                                            granit_webgpu_provider_swapchain swapchain,
                                            std::uint32_t* needs_recreate) noexcept {
  if (api_ == nullptr || instance == 0 || swapchain == 0 || needs_recreate == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  if (std::find(instances_.begin(), instances_.end(), instance) == instances_.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  try {
    return api_->instance_api->present_swapchain(instance, swapchain, needs_recreate);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result webgpu_provider_dispatch::cancel_swapchain(granit_webgpu_provider_instance instance,
                                                         granit_webgpu_provider_swapchain swapchain,
                                                         std::uint32_t* needs_recreate) noexcept {
  if (api_ == nullptr || instance == 0 || swapchain == 0 || needs_recreate == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  if (std::find(instances_.begin(), instances_.end(), instance) == instances_.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  try {
    return api_->instance_api->cancel_swapchain(instance, swapchain, needs_recreate);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result
webgpu_provider_dispatch::destroy_swapchain(granit_webgpu_provider_instance instance,
                                            granit_webgpu_provider_swapchain swapchain) noexcept {
  if (api_ == nullptr || instance == 0 || swapchain == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  if (std::find(instances_.begin(), instances_.end(), instance) == instances_.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  try {
    return api_->instance_api->destroy_swapchain(instance, swapchain);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result
webgpu_provider_dispatch::create_buffer(granit_webgpu_provider_instance instance,
                                        const granit_webgpu_provider_buffer_desc* desc,
                                        granit_webgpu_provider_buffer* buffer) noexcept {
  if (api_ == nullptr || instance == 0 || desc == nullptr || buffer == nullptr) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  if (std::find(instances_.begin(), instances_.end(), instance) == instances_.end()) {
    return GRANIT_ERROR_INVALID_HANDLE;
  }
  try {
    return api_->instance_api->create_buffer(instance, desc, buffer);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result
webgpu_provider_dispatch::destroy_buffer(granit_webgpu_provider_instance instance,
                                         granit_webgpu_provider_buffer buffer) noexcept {
  if (api_ == nullptr || instance == 0 || buffer == 0) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  if (std::find(instances_.begin(), instances_.end(), instance) == instances_.end()) {
    return GRANIT_ERROR_INVALID_HANDLE;
  }
  try {
    return api_->instance_api->destroy_buffer(instance, buffer);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result webgpu_provider_dispatch::write_buffer(granit_webgpu_provider_instance instance,
                                                     granit_webgpu_provider_buffer buffer,
                                                     std::uint64_t offset, const void* data,
                                                     std::uint64_t size) noexcept {
  if (api_ == nullptr || instance == 0 || buffer == 0 || data == nullptr || size == 0) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  if (std::find(instances_.begin(), instances_.end(), instance) == instances_.end()) {
    return GRANIT_ERROR_INVALID_HANDLE;
  }
  try {
    return api_->instance_api->write_buffer(instance, buffer, offset, data, size);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result webgpu_provider_dispatch::read_buffer(granit_webgpu_provider_instance instance,
                                                    granit_webgpu_provider_buffer buffer,
                                                    std::uint64_t offset, void* data,
                                                    std::uint64_t size) noexcept {
  if (api_ == nullptr || instance == 0 || buffer == 0 || data == nullptr || size == 0) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  if (std::find(instances_.begin(), instances_.end(), instance) == instances_.end()) {
    return GRANIT_ERROR_INVALID_HANDLE;
  }
  try {
    return api_->instance_api->read_buffer(instance, buffer, offset, data, size);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result
webgpu_provider_dispatch::create_texture(granit_webgpu_provider_instance instance,
                                         const granit_webgpu_provider_texture_desc* desc,
                                         granit_webgpu_provider_texture* texture) noexcept {
  if (api_ == nullptr || instance == 0 || desc == nullptr || texture == nullptr) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  if (std::find(instances_.begin(), instances_.end(), instance) == instances_.end()) {
    return GRANIT_ERROR_INVALID_HANDLE;
  }
  try {
    return api_->instance_api->create_texture(instance, desc, texture);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result
webgpu_provider_dispatch::destroy_texture(granit_webgpu_provider_instance instance,
                                          granit_webgpu_provider_texture texture) noexcept {
  if (api_ == nullptr || instance == 0 || texture == 0) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  if (std::find(instances_.begin(), instances_.end(), instance) == instances_.end()) {
    return GRANIT_ERROR_INVALID_HANDLE;
  }
  try {
    return api_->instance_api->destroy_texture(instance, texture);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result
webgpu_provider_dispatch::write_texture(granit_webgpu_provider_instance instance,
                                        granit_webgpu_provider_texture texture,
                                        const granit_webgpu_provider_texture_write_desc* desc,
                                        const void* data, std::uint64_t size) noexcept {
  if (api_ == nullptr || instance == 0 || texture == 0 || desc == nullptr || data == nullptr ||
      size == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  if (std::find(instances_.begin(), instances_.end(), instance) == instances_.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  try {
    return api_->instance_api->write_texture(instance, texture, desc, data, size);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result webgpu_provider_dispatch::write_upload_batch(
    granit_webgpu_provider_instance instance,
    std::span<const granit_webgpu_provider_upload_operation> operations) noexcept {
  if (api_ == nullptr || instance == 0 || operations.empty() || operations.size() > UINT32_MAX)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  if (std::find(instances_.begin(), instances_.end(), instance) == instances_.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  try {
    return api_->instance_api->write_upload_batch(instance, operations.data(),
                                                  static_cast<std::uint32_t>(operations.size()));
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result
webgpu_provider_dispatch::create_texture_view(granit_webgpu_provider_instance instance,
                                              granit_webgpu_provider_texture texture,
                                              const granit_webgpu_provider_texture_view_desc* desc,
                                              granit_webgpu_provider_texture_view* view) noexcept {
  if (api_ == nullptr || instance == 0 || texture == 0 || desc == nullptr || view == nullptr) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  if (std::find(instances_.begin(), instances_.end(), instance) == instances_.end()) {
    return GRANIT_ERROR_INVALID_HANDLE;
  }
  try {
    return api_->instance_api->create_texture_view(instance, texture, desc, view);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result
webgpu_provider_dispatch::destroy_texture_view(granit_webgpu_provider_instance instance,
                                               granit_webgpu_provider_texture_view view) noexcept {
  if (api_ == nullptr || instance == 0 || view == 0) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  if (std::find(instances_.begin(), instances_.end(), instance) == instances_.end()) {
    return GRANIT_ERROR_INVALID_HANDLE;
  }
  try {
    return api_->instance_api->destroy_texture_view(instance, view);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result
webgpu_provider_dispatch::create_sampler(granit_webgpu_provider_instance instance,
                                         const granit_webgpu_provider_sampler_desc* desc,
                                         granit_webgpu_provider_sampler* sampler) noexcept {
  if (api_ == nullptr || instance == 0 || desc == nullptr || sampler == nullptr) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  if (std::find(instances_.begin(), instances_.end(), instance) == instances_.end()) {
    return GRANIT_ERROR_INVALID_HANDLE;
  }
  try {
    return api_->instance_api->create_sampler(instance, desc, sampler);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result
webgpu_provider_dispatch::destroy_sampler(granit_webgpu_provider_instance instance,
                                          granit_webgpu_provider_sampler sampler) noexcept {
  if (api_ == nullptr || instance == 0 || sampler == 0) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  if (std::find(instances_.begin(), instances_.end(), instance) == instances_.end()) {
    return GRANIT_ERROR_INVALID_HANDLE;
  }
  try {
    return api_->instance_api->destroy_sampler(instance, sampler);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

#define GRANIT_PROVIDER_DISPATCH_CREATE_METHOD(method, function, input_type, output_type)          \
  granit_result webgpu_provider_dispatch::method(granit_webgpu_provider_instance instance,         \
                                                 input_type input, output_type* output) noexcept { \
    if (api_ == nullptr || instance == 0 || input == 0 || output == nullptr) {                     \
      return GRANIT_ERROR_INVALID_ARGUMENT;                                                        \
    }                                                                                              \
    if (std::find(instances_.begin(), instances_.end(), instance) == instances_.end()) {           \
      return GRANIT_ERROR_INVALID_HANDLE;                                                          \
    }                                                                                              \
    try {                                                                                          \
      return api_->instance_api->function(instance, input, output);                                \
    } catch (...) {                                                                                \
      return GRANIT_ERROR_INTERNAL;                                                                \
    }                                                                                              \
  }

#define GRANIT_PROVIDER_DISPATCH_DESTROY_METHOD(method, function, handle_type)                     \
  granit_result webgpu_provider_dispatch::method(granit_webgpu_provider_instance instance,         \
                                                 handle_type handle) noexcept {                    \
    if (api_ == nullptr || instance == 0 || handle == 0) {                                         \
      return GRANIT_ERROR_INVALID_ARGUMENT;                                                        \
    }                                                                                              \
    if (std::find(instances_.begin(), instances_.end(), instance) == instances_.end()) {           \
      return GRANIT_ERROR_INVALID_HANDLE;                                                          \
    }                                                                                              \
    try {                                                                                          \
      return api_->instance_api->function(instance, handle);                                       \
    } catch (...) {                                                                                \
      return GRANIT_ERROR_INTERNAL;                                                                \
    }                                                                                              \
  }

granit_result webgpu_provider_dispatch::create_bind_group_layout(
    granit_webgpu_provider_instance instance,
    const granit_webgpu_provider_bind_group_layout_desc* desc,
    granit_webgpu_provider_bind_group_layout* layout) noexcept {
  if (api_ == nullptr || instance == 0 || desc == nullptr || layout == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  if (std::find(instances_.begin(), instances_.end(), instance) == instances_.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  try {
    return api_->instance_api->create_bind_group_layout(instance, desc, layout);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

GRANIT_PROVIDER_DISPATCH_DESTROY_METHOD(destroy_bind_group_layout, destroy_bind_group_layout,
                                        granit_webgpu_provider_bind_group_layout)

granit_result webgpu_provider_dispatch::create_bind_group(
    granit_webgpu_provider_instance instance, const granit_webgpu_provider_bind_group_desc* desc,
    granit_webgpu_provider_bind_group* bind_group) noexcept {
  if (api_ == nullptr || instance == 0 || desc == nullptr || bind_group == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  if (std::find(instances_.begin(), instances_.end(), instance) == instances_.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  try {
    return api_->instance_api->create_bind_group(instance, desc, bind_group);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

GRANIT_PROVIDER_DISPATCH_DESTROY_METHOD(destroy_bind_group, destroy_bind_group,
                                        granit_webgpu_provider_bind_group)
GRANIT_PROVIDER_DISPATCH_CREATE_METHOD(create_shader, create_shader,
                                       const granit_webgpu_provider_shader_desc*,
                                       granit_webgpu_provider_shader)
GRANIT_PROVIDER_DISPATCH_DESTROY_METHOD(destroy_shader, destroy_shader,
                                        granit_webgpu_provider_shader)
granit_result webgpu_provider_dispatch::create_pipeline_layout(
    granit_webgpu_provider_instance instance,
    const granit_webgpu_provider_pipeline_layout_desc* desc,
    granit_webgpu_provider_pipeline_layout* pipeline_layout) noexcept {
  if (api_ == nullptr || instance == 0 || desc == nullptr || pipeline_layout == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  if (std::find(instances_.begin(), instances_.end(), instance) == instances_.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  try {
    return api_->instance_api->create_pipeline_layout(instance, desc, pipeline_layout);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}
GRANIT_PROVIDER_DISPATCH_DESTROY_METHOD(destroy_pipeline_layout, destroy_pipeline_layout,
                                        granit_webgpu_provider_pipeline_layout)
GRANIT_PROVIDER_DISPATCH_CREATE_METHOD(create_compute_pipeline, create_compute_pipeline,
                                       const granit_webgpu_provider_compute_pipeline_desc*,
                                       granit_webgpu_provider_compute_pipeline)
GRANIT_PROVIDER_DISPATCH_DESTROY_METHOD(destroy_compute_pipeline, destroy_compute_pipeline,
                                        granit_webgpu_provider_compute_pipeline)

granit_result webgpu_provider_dispatch::recorder_begin_compute(
    granit_webgpu_provider_instance instance,
    granit_webgpu_provider_command_recorder recorder) noexcept {
  if (api_ == nullptr || instance == 0 || recorder == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    return api_->instance_api->recorder_begin_compute(instance, recorder);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result webgpu_provider_dispatch::recorder_bind_compute_pipeline(
    granit_webgpu_provider_instance instance, granit_webgpu_provider_command_recorder recorder,
    granit_webgpu_provider_compute_pipeline pipeline) noexcept {
  if (api_ == nullptr || instance == 0 || recorder == 0 || pipeline == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    return api_->instance_api->recorder_bind_compute_pipeline(instance, recorder, pipeline);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result webgpu_provider_dispatch::recorder_bind_compute_groups(
    granit_webgpu_provider_instance instance, granit_webgpu_provider_command_recorder recorder,
    granit_webgpu_provider_pipeline_layout layout, std::uint32_t first_group,
    std::span<const granit_webgpu_provider_bind_group> groups,
    std::span<const std::uint32_t> dynamic_offsets) noexcept {
  if (api_ == nullptr || instance == 0 || recorder == 0 || layout == 0 || groups.empty() ||
      groups.size() > UINT32_MAX || dynamic_offsets.size() > UINT32_MAX)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    return api_->instance_api->recorder_bind_compute_groups(
        instance, recorder, layout, first_group, groups.data(),
        static_cast<std::uint32_t>(groups.size()), dynamic_offsets.data(),
        static_cast<std::uint32_t>(dynamic_offsets.size()));
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result webgpu_provider_dispatch::recorder_dispatch(
    granit_webgpu_provider_instance instance, granit_webgpu_provider_command_recorder recorder,
    std::uint32_t x, std::uint32_t y, std::uint32_t z) noexcept {
  if (api_ == nullptr || instance == 0 || recorder == 0 || x == 0 || y == 0 || z == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    return api_->instance_api->recorder_dispatch(instance, recorder, x, y, z);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result webgpu_provider_dispatch::recorder_end_compute(
    granit_webgpu_provider_instance instance,
    granit_webgpu_provider_command_recorder recorder) noexcept {
  if (api_ == nullptr || instance == 0 || recorder == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    return api_->instance_api->recorder_end_compute(instance, recorder);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}
GRANIT_PROVIDER_DISPATCH_CREATE_METHOD(create_render_pipeline, create_render_pipeline,
                                       const granit_webgpu_provider_render_pipeline_desc*,
                                       granit_webgpu_provider_render_pipeline)
GRANIT_PROVIDER_DISPATCH_DESTROY_METHOD(destroy_render_pipeline, destroy_render_pipeline,
                                        granit_webgpu_provider_render_pipeline)

granit_result webgpu_provider_dispatch::create_command_recorder(
    granit_webgpu_provider_instance instance,
    granit_webgpu_provider_command_recorder* recorder) noexcept {
  if (api_ == nullptr || instance == 0 || recorder == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  if (std::find(instances_.begin(), instances_.end(), instance) == instances_.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  try {
    return api_->instance_api->create_command_recorder(instance, recorder);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

GRANIT_PROVIDER_DISPATCH_DESTROY_METHOD(destroy_command_recorder, destroy_command_recorder,
                                        granit_webgpu_provider_command_recorder)

granit_result webgpu_provider_dispatch::recorder_copy_buffer_to_texture(
    granit_webgpu_provider_instance instance, granit_webgpu_provider_command_recorder recorder,
    granit_webgpu_provider_buffer buffer, granit_webgpu_provider_texture texture,
    std::uint32_t width, std::uint32_t height, std::uint32_t bytes_per_row) noexcept {
  if (api_ == nullptr || instance == 0 || recorder == 0 || buffer == 0 || texture == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  if (std::find(instances_.begin(), instances_.end(), instance) == instances_.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  try {
    return api_->instance_api->recorder_copy_buffer_to_texture(instance, recorder, buffer, texture,
                                                               width, height, bytes_per_row);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result webgpu_provider_dispatch::recorder_begin_rendering(
    granit_webgpu_provider_instance instance, granit_webgpu_provider_command_recorder recorder,
    granit_webgpu_provider_texture_view target, granit_webgpu_provider_load_operation load,
    granit_webgpu_provider_store_operation store, const float clear[4],
    granit_webgpu_provider_texture_view resolve_target,
    granit_webgpu_provider_texture_view depth_target,
    granit_webgpu_provider_load_operation depth_load,
    granit_webgpu_provider_store_operation depth_store, float clear_depth) noexcept {
  if (api_ == nullptr || instance == 0 || recorder == 0 || (target == 0 && depth_target == 0) ||
      clear == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    return api_->instance_api->recorder_begin_rendering(
        instance, recorder, target, resolve_target, load, store, clear[0], clear[1], clear[2],
        clear[3], depth_target, depth_load, depth_store, clear_depth);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result webgpu_provider_dispatch::recorder_bind_pipeline(
    granit_webgpu_provider_instance instance, granit_webgpu_provider_command_recorder recorder,
    granit_webgpu_provider_render_pipeline pipeline) noexcept {
  if (api_ == nullptr || instance == 0 || recorder == 0 || pipeline == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    return api_->instance_api->recorder_bind_pipeline(instance, recorder, pipeline);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result webgpu_provider_dispatch::recorder_bind_graphics_groups(
    granit_webgpu_provider_instance instance, granit_webgpu_provider_command_recorder recorder,
    granit_webgpu_provider_pipeline_layout layout, std::uint32_t first_group,
    std::span<const granit_webgpu_provider_bind_group> groups,
    std::span<const std::uint32_t> dynamic_offsets) noexcept {
  if (api_ == nullptr || instance == 0 || recorder == 0 || layout == 0 || groups.empty() ||
      groups.size() > UINT32_MAX || dynamic_offsets.size() > UINT32_MAX)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    return api_->instance_api->recorder_bind_graphics_groups(
        instance, recorder, layout, first_group, groups.data(),
        static_cast<std::uint32_t>(groups.size()), dynamic_offsets.data(),
        static_cast<std::uint32_t>(dynamic_offsets.size()));
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result webgpu_provider_dispatch::recorder_bind_vertex_buffers(
    granit_webgpu_provider_instance instance, granit_webgpu_provider_command_recorder recorder,
    std::uint32_t first,
    std::span<const granit_webgpu_provider_vertex_buffer_binding> bindings) noexcept {
  if (api_ == nullptr || instance == 0 || recorder == 0 || bindings.empty())
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    return api_->instance_api->recorder_bind_vertex_buffers(
        instance, recorder, first, bindings.data(), static_cast<std::uint32_t>(bindings.size()));
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result webgpu_provider_dispatch::recorder_bind_index_buffer(
    granit_webgpu_provider_instance instance, granit_webgpu_provider_command_recorder recorder,
    granit_webgpu_provider_buffer buffer, std::uint64_t offset,
    granit_webgpu_provider_index_format format) noexcept {
  if (api_ == nullptr || instance == 0 || recorder == 0 || buffer == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    return api_->instance_api->recorder_bind_index_buffer(instance, recorder, buffer, offset,
                                                          format);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result webgpu_provider_dispatch::recorder_set_viewports(
    granit_webgpu_provider_instance instance, granit_webgpu_provider_command_recorder recorder,
    std::uint32_t first, std::span<const granit_webgpu_provider_viewport> viewports) noexcept {
  if (api_ == nullptr || instance == 0 || recorder == 0 || viewports.empty() ||
      viewports.size() > UINT32_MAX)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    return api_->instance_api->recorder_set_viewports(instance, recorder, first, viewports.data(),
                                                      static_cast<std::uint32_t>(viewports.size()));
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result webgpu_provider_dispatch::recorder_set_scissors(
    granit_webgpu_provider_instance instance, granit_webgpu_provider_command_recorder recorder,
    std::uint32_t first, std::span<const granit_webgpu_provider_scissor> scissors) noexcept {
  if (api_ == nullptr || instance == 0 || recorder == 0 || scissors.empty() ||
      scissors.size() > UINT32_MAX)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    return api_->instance_api->recorder_set_scissors(instance, recorder, first, scissors.data(),
                                                     static_cast<std::uint32_t>(scissors.size()));
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result webgpu_provider_dispatch::recorder_draw_vertices(
    granit_webgpu_provider_instance instance, granit_webgpu_provider_command_recorder recorder,
    std::uint32_t vertex_count, std::uint32_t instance_count, std::uint32_t first_vertex,
    std::uint32_t first_instance) noexcept {
  if (api_ == nullptr || instance == 0 || recorder == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    return api_->instance_api->recorder_draw_vertices(instance, recorder, vertex_count,
                                                      instance_count, first_vertex, first_instance);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result webgpu_provider_dispatch::recorder_draw_indices(
    granit_webgpu_provider_instance instance, granit_webgpu_provider_command_recorder recorder,
    std::uint32_t index_count, std::uint32_t instance_count, std::uint32_t first_index,
    std::int32_t vertex_offset, std::uint32_t first_instance) noexcept {
  if (api_ == nullptr || instance == 0 || recorder == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    return api_->instance_api->recorder_draw_indices(instance, recorder, index_count,
                                                     instance_count, first_index, vertex_offset,
                                                     first_instance);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result webgpu_provider_dispatch::recorder_end_rendering(
    granit_webgpu_provider_instance instance,
    granit_webgpu_provider_command_recorder recorder) noexcept {
  if (api_ == nullptr || instance == 0 || recorder == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    return api_->instance_api->recorder_end_rendering(instance, recorder);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result webgpu_provider_dispatch::finish_command_recorder(
    granit_webgpu_provider_instance instance, granit_webgpu_provider_command_recorder recorder,
    granit_webgpu_provider_command_buffer* command_buffer) noexcept {
  if (api_ == nullptr || instance == 0 || recorder == 0 || command_buffer == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  if (std::find(instances_.begin(), instances_.end(), instance) == instances_.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  try {
    return api_->instance_api->finish_command_recorder(instance, recorder, command_buffer);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

GRANIT_PROVIDER_DISPATCH_DESTROY_METHOD(destroy_command_buffer, destroy_command_buffer,
                                        granit_webgpu_provider_command_buffer)
GRANIT_PROVIDER_DISPATCH_DESTROY_METHOD(submit_command_buffer, submit_command_buffer,
                                        granit_webgpu_provider_command_buffer)

granit_result webgpu_provider_dispatch::recorder_copy_texture_to_buffer(
    granit_webgpu_provider_instance instance, granit_webgpu_provider_command_recorder recorder,
    granit_webgpu_provider_texture texture, granit_webgpu_provider_buffer buffer,
    std::uint32_t width, std::uint32_t height, std::uint32_t bytes_per_row) noexcept {
  if (api_ == nullptr || instance == 0 || recorder == 0 || texture == 0 || buffer == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  if (std::find(instances_.begin(), instances_.end(), instance) == instances_.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  try {
    return api_->instance_api->recorder_copy_texture_to_buffer(instance, recorder, texture, buffer,
                                                               width, height, bytes_per_row);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result webgpu_provider_dispatch::recorder_copy_buffer(
    granit_webgpu_provider_instance instance, granit_webgpu_provider_command_recorder recorder,
    granit_webgpu_provider_buffer source, granit_webgpu_provider_buffer destination,
    std::span<const granit_webgpu_provider_buffer_copy_region> regions) noexcept {
  if (api_ == nullptr || instance == 0 || recorder == 0 || source == 0 || destination == 0 ||
      regions.empty() || regions.size() > UINT32_MAX)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  if (std::find(instances_.begin(), instances_.end(), instance) == instances_.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  try {
    return api_->instance_api->recorder_copy_buffer(instance, recorder, source, destination,
                                                    regions.data(),
                                                    static_cast<std::uint32_t>(regions.size()));
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result webgpu_provider_dispatch::recorder_copy_buffer_to_texture_v2(
    granit_webgpu_provider_instance instance, granit_webgpu_provider_command_recorder recorder,
    granit_webgpu_provider_buffer source, granit_webgpu_provider_texture destination,
    const granit_webgpu_provider_texture_buffer_copy& region) noexcept {
  if (api_ == nullptr || instance == 0 || recorder == 0 || source == 0 || destination == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    return api_->instance_api->recorder_copy_buffer_to_texture_v2(instance, recorder, source,
                                                                  destination, &region);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result webgpu_provider_dispatch::recorder_copy_texture_to_buffer_v2(
    granit_webgpu_provider_instance instance, granit_webgpu_provider_command_recorder recorder,
    granit_webgpu_provider_texture source, granit_webgpu_provider_buffer destination,
    const granit_webgpu_provider_texture_buffer_copy& region) noexcept {
  if (api_ == nullptr || instance == 0 || recorder == 0 || source == 0 || destination == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    return api_->instance_api->recorder_copy_texture_to_buffer_v2(instance, recorder, source,
                                                                  destination, &region);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result webgpu_provider_dispatch::recorder_copy_texture(
    granit_webgpu_provider_instance instance, granit_webgpu_provider_command_recorder recorder,
    granit_webgpu_provider_texture source, granit_webgpu_provider_texture destination,
    const granit_webgpu_provider_texture_copy_region& region) noexcept {
  if (api_ == nullptr || instance == 0 || recorder == 0 || source == 0 || destination == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    return api_->instance_api->recorder_copy_texture(instance, recorder, source, destination,
                                                     &region);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result webgpu_provider_dispatch::recorder_fill_buffer(
    granit_webgpu_provider_instance instance, granit_webgpu_provider_command_recorder recorder,
    granit_webgpu_provider_buffer buffer, std::uint64_t offset, std::uint64_t size,
    std::uint32_t value) noexcept {
  if (api_ == nullptr || instance == 0 || recorder == 0 || buffer == 0 || size == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    return api_->instance_api->recorder_fill_buffer(instance, recorder, buffer, offset, size,
                                                    value);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result webgpu_provider_dispatch::recorder_generate_mipmaps(
    granit_webgpu_provider_instance instance, granit_webgpu_provider_command_recorder recorder,
    granit_webgpu_provider_texture texture,
    const granit_webgpu_provider_texture_mipmap_range& range) noexcept {
  if (api_ == nullptr || instance == 0 || recorder == 0 || texture == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    return api_->instance_api->recorder_generate_mipmaps(instance, recorder, texture, &range);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

#undef GRANIT_PROVIDER_DISPATCH_CREATE_METHOD
#undef GRANIT_PROVIDER_DISPATCH_DESTROY_METHOD

void webgpu_provider_dispatch::close() noexcept {
  if (api_ != nullptr) {
    for (auto iterator = instances_.rbegin(); iterator != instances_.rend(); ++iterator) {
      try {
        api_->destroy(*iterator);
      } catch (...) {
      }
    }
  }
  instances_.clear();
  api_ = nullptr;
}

} // namespace granit::detail
