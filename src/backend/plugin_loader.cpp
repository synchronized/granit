// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "backend/plugin_loader.h"

#include <algorithm>
#include <cstddef>
#include <new>

namespace granit::detail {
namespace {

bool is_compatible(const granit_backend_plugin_api* api,
                   granit_backend_plugin_kind expected_kind) noexcept {
  constexpr std::size_t minimum_size = offsetof(granit_backend_plugin_api, instance_api) +
                                       sizeof(const granit_backend_plugin_instance_api*);
  constexpr std::size_t minimum_instance_api_size =
      offsetof(granit_backend_plugin_instance_api, destroy_swapchain) +
      sizeof(granit_backend_plugin_destroy_swapchain_fn);
  return api != nullptr && api->struct_size >= minimum_size &&
         api->abi_version == GRANIT_BACKEND_PLUGIN_ABI_VERSION && api->kind == expected_kind &&
         api->reserved == 0 && api->name != nullptr && api->name_length != 0 &&
         api->create != nullptr && api->destroy != nullptr && api->instance_api != nullptr &&
         api->instance_api->struct_size >= minimum_instance_api_size &&
         api->instance_api->reserved == 0 && api->instance_api->get_capabilities != nullptr &&
         api->instance_api->create_buffer != nullptr &&
         api->instance_api->destroy_buffer != nullptr &&
         api->instance_api->write_buffer != nullptr && api->instance_api->read_buffer != nullptr &&
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
         api->instance_api->recorder_draw != nullptr &&
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
         api->instance_api->destroy_swapchain != nullptr;
}

bool is_valid_host(const granit_backend_plugin_host_api* host) noexcept {
  constexpr std::size_t minimum_size =
      offsetof(granit_backend_plugin_host_api, allocator_user_data) + sizeof(void*);
  return host != nullptr && host->struct_size >= minimum_size && host->reserved == 0 &&
         host->allocate != nullptr && host->deallocate != nullptr;
}

} // namespace

backend_plugin_loader::~backend_plugin_loader() { close(); }

granit_result backend_plugin_loader::open(const char* library_path,
                                          granit_backend_plugin_kind expected_kind) noexcept {
  close();
  if (library_path == nullptr || library_path[0] == '\0') {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }

  if (!library_.open(library_path)) {
    return GRANIT_ERROR_BACKEND_UNAVAILABLE;
  }

  const auto query = reinterpret_cast<granit_backend_plugin_query_fn>(
      library_.symbol(GRANIT_BACKEND_PLUGIN_QUERY_SYMBOL));
  if (query == nullptr) {
    close();
    return GRANIT_ERROR_INCOMPATIBLE_DRIVER;
  }

  api_ = query(GRANIT_BACKEND_PLUGIN_ABI_VERSION);
  if (!is_compatible(api_, expected_kind)) {
    close();
    return GRANIT_ERROR_INCOMPATIBLE_DRIVER;
  }
  return GRANIT_SUCCESS;
}

granit_result
backend_plugin_loader::open_static(const granit_backend_plugin_api* api,
                                   granit_backend_plugin_kind expected_kind) noexcept {
  close();
  if (!is_compatible(api, expected_kind)) {
    return GRANIT_ERROR_INCOMPATIBLE_DRIVER;
  }
  api_ = api;
  return GRANIT_SUCCESS;
}

granit_result
backend_plugin_loader::create_instance(const granit_backend_plugin_host_api* host,
                                       granit_backend_plugin_instance* out_instance) noexcept {
  if (api_ == nullptr || out_instance == nullptr || !is_valid_host(host)) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  *out_instance = 0;

  granit_backend_plugin_instance instance = 0;
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
backend_plugin_loader::destroy_instance(granit_backend_plugin_instance instance) noexcept {
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

granit_result
backend_plugin_loader::get_capabilities(granit_backend_plugin_instance instance,
                                        granit_backend_plugin_capabilities* capabilities) noexcept {
  constexpr std::size_t minimum_size =
      offsetof(granit_backend_plugin_capabilities, reserved_2) + sizeof(std::uint32_t);
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

granit_result
backend_plugin_loader::get_instance_status(granit_backend_plugin_instance instance,
                                           granit_backend_plugin_instance_status* status) noexcept {
  if (api_ == nullptr || instance == 0 || status == nullptr ||
      status->struct_size < sizeof(granit_backend_plugin_instance_status) ||
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
backend_plugin_loader::process_events(granit_backend_plugin_instance instance) noexcept {
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

granit_result
backend_plugin_loader::create_win32_surface(granit_backend_plugin_instance instance,
                                            const granit_backend_plugin_win32_surface_desc* desc,
                                            granit_backend_plugin_surface* surface) noexcept {
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
backend_plugin_loader::create_xcb_surface(granit_backend_plugin_instance instance,
                                          const granit_backend_plugin_xcb_surface_desc* desc,
                                          granit_backend_plugin_surface* surface) noexcept {
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

granit_result backend_plugin_loader::create_wayland_surface(
    granit_backend_plugin_instance instance, const granit_backend_plugin_wayland_surface_desc* desc,
    granit_backend_plugin_surface* surface) noexcept {
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

granit_result
backend_plugin_loader::create_canvas_surface(granit_backend_plugin_instance instance,
                                             const granit_backend_plugin_canvas_surface_desc* desc,
                                             granit_backend_plugin_surface* surface) noexcept {
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
backend_plugin_loader::destroy_surface(granit_backend_plugin_instance instance,
                                       granit_backend_plugin_surface surface) noexcept {
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
backend_plugin_loader::create_swapchain(granit_backend_plugin_instance instance,
                                        granit_backend_plugin_surface surface,
                                        const granit_backend_plugin_swapchain_desc* desc,
                                        granit_backend_plugin_swapchain* swapchain) noexcept {
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

granit_result backend_plugin_loader::recreate_swapchain(
    granit_backend_plugin_instance instance, granit_backend_plugin_swapchain swapchain,
    const granit_backend_plugin_swapchain_desc* desc) noexcept {
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
backend_plugin_loader::get_swapchain_info(granit_backend_plugin_instance instance,
                                          granit_backend_plugin_swapchain swapchain,
                                          granit_backend_plugin_swapchain_info* info) noexcept {
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
backend_plugin_loader::acquire_swapchain(granit_backend_plugin_instance instance,
                                         granit_backend_plugin_swapchain swapchain,
                                         granit_backend_plugin_acquired_frame* frame) noexcept {
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

granit_result backend_plugin_loader::present_swapchain(granit_backend_plugin_instance instance,
                                                       granit_backend_plugin_swapchain swapchain,
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

granit_result backend_plugin_loader::cancel_swapchain(granit_backend_plugin_instance instance,
                                                      granit_backend_plugin_swapchain swapchain,
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
backend_plugin_loader::destroy_swapchain(granit_backend_plugin_instance instance,
                                         granit_backend_plugin_swapchain swapchain) noexcept {
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

granit_result backend_plugin_loader::create_buffer(granit_backend_plugin_instance instance,
                                                   const granit_backend_plugin_buffer_desc* desc,
                                                   granit_backend_plugin_buffer* buffer) noexcept {
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

granit_result backend_plugin_loader::destroy_buffer(granit_backend_plugin_instance instance,
                                                    granit_backend_plugin_buffer buffer) noexcept {
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

granit_result backend_plugin_loader::write_buffer(granit_backend_plugin_instance instance,
                                                  granit_backend_plugin_buffer buffer,
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

granit_result backend_plugin_loader::read_buffer(granit_backend_plugin_instance instance,
                                                 granit_backend_plugin_buffer buffer,
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
backend_plugin_loader::create_texture(granit_backend_plugin_instance instance,
                                      const granit_backend_plugin_texture_desc* desc,
                                      granit_backend_plugin_texture* texture) noexcept {
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
backend_plugin_loader::destroy_texture(granit_backend_plugin_instance instance,
                                       granit_backend_plugin_texture texture) noexcept {
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
backend_plugin_loader::create_texture_view(granit_backend_plugin_instance instance,
                                           granit_backend_plugin_texture texture,
                                           granit_backend_plugin_texture_view* view) noexcept {
  if (api_ == nullptr || instance == 0 || texture == 0 || view == nullptr) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  if (std::find(instances_.begin(), instances_.end(), instance) == instances_.end()) {
    return GRANIT_ERROR_INVALID_HANDLE;
  }
  try {
    return api_->instance_api->create_texture_view(instance, texture, view);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result
backend_plugin_loader::destroy_texture_view(granit_backend_plugin_instance instance,
                                            granit_backend_plugin_texture_view view) noexcept {
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
backend_plugin_loader::create_sampler(granit_backend_plugin_instance instance,
                                      const granit_backend_plugin_sampler_desc* desc,
                                      granit_backend_plugin_sampler* sampler) noexcept {
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
backend_plugin_loader::destroy_sampler(granit_backend_plugin_instance instance,
                                       granit_backend_plugin_sampler sampler) noexcept {
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

#define GRANIT_LOADER_CREATE_METHOD(method, function, input_type, output_type)                     \
  granit_result backend_plugin_loader::method(granit_backend_plugin_instance instance,             \
                                              input_type input, output_type* output) noexcept {    \
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

#define GRANIT_LOADER_DESTROY_METHOD(method, function, handle_type)                                \
  granit_result backend_plugin_loader::method(granit_backend_plugin_instance instance,             \
                                              handle_type handle) noexcept {                       \
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

granit_result backend_plugin_loader::create_bind_group_layout(
    granit_backend_plugin_instance instance,
    granit_backend_plugin_bind_group_layout* layout) noexcept {
  if (api_ == nullptr || instance == 0 || layout == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  if (std::find(instances_.begin(), instances_.end(), instance) == instances_.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  try {
    return api_->instance_api->create_bind_group_layout(instance, layout);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

GRANIT_LOADER_DESTROY_METHOD(destroy_bind_group_layout, destroy_bind_group_layout,
                             granit_backend_plugin_bind_group_layout)

granit_result
backend_plugin_loader::create_bind_group(granit_backend_plugin_instance instance,
                                         const granit_backend_plugin_bind_group_desc* desc,
                                         granit_backend_plugin_bind_group* bind_group) noexcept {
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

GRANIT_LOADER_DESTROY_METHOD(destroy_bind_group, destroy_bind_group,
                             granit_backend_plugin_bind_group)
GRANIT_LOADER_CREATE_METHOD(create_shader, create_shader, const granit_backend_plugin_shader_desc*,
                            granit_backend_plugin_shader)
GRANIT_LOADER_DESTROY_METHOD(destroy_shader, destroy_shader, granit_backend_plugin_shader)
granit_result backend_plugin_loader::create_pipeline_layout(
    granit_backend_plugin_instance instance,
    granit_backend_plugin_bind_group_layout bind_group_layout,
    granit_backend_plugin_pipeline_layout* pipeline_layout) noexcept {
  if (api_ == nullptr || instance == 0 || pipeline_layout == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  if (std::find(instances_.begin(), instances_.end(), instance) == instances_.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  try {
    return api_->instance_api->create_pipeline_layout(instance, bind_group_layout, pipeline_layout);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}
GRANIT_LOADER_DESTROY_METHOD(destroy_pipeline_layout, destroy_pipeline_layout,
                             granit_backend_plugin_pipeline_layout)
GRANIT_LOADER_CREATE_METHOD(create_render_pipeline, create_render_pipeline,
                            const granit_backend_plugin_render_pipeline_desc*,
                            granit_backend_plugin_render_pipeline)
GRANIT_LOADER_DESTROY_METHOD(destroy_render_pipeline, destroy_render_pipeline,
                             granit_backend_plugin_render_pipeline)

granit_result backend_plugin_loader::create_command_recorder(
    granit_backend_plugin_instance instance,
    granit_backend_plugin_command_recorder* recorder) noexcept {
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

GRANIT_LOADER_DESTROY_METHOD(destroy_command_recorder, destroy_command_recorder,
                             granit_backend_plugin_command_recorder)

granit_result backend_plugin_loader::recorder_copy_buffer_to_texture(
    granit_backend_plugin_instance instance, granit_backend_plugin_command_recorder recorder,
    granit_backend_plugin_buffer buffer, granit_backend_plugin_texture texture, std::uint32_t width,
    std::uint32_t height, std::uint32_t bytes_per_row) noexcept {
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

granit_result backend_plugin_loader::recorder_draw(
    granit_backend_plugin_instance instance, granit_backend_plugin_command_recorder recorder,
    granit_backend_plugin_texture_view target, granit_backend_plugin_render_pipeline pipeline,
    granit_backend_plugin_bind_group bind_group, std::uint32_t first_vertex_buffer,
    std::span<const granit_backend_plugin_vertex_buffer_binding> vertex_buffers, bool indexed,
    granit_backend_plugin_buffer index_buffer, std::uint64_t index_buffer_offset,
    granit_backend_plugin_index_format index_format, std::uint32_t element_count,
    std::uint32_t instance_count, std::uint32_t first_element, std::int32_t vertex_offset,
    std::uint32_t first_instance) noexcept {
  if (api_ == nullptr || instance == 0 || recorder == 0 || target == 0 || pipeline == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  if (std::find(instances_.begin(), instances_.end(), instance) == instances_.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  try {
    return api_->instance_api->recorder_draw(
        instance, recorder, target, pipeline, bind_group, first_vertex_buffer,
        vertex_buffers.data(), static_cast<std::uint32_t>(vertex_buffers.size()), indexed ? 1U : 0U,
        index_buffer, index_buffer_offset, index_format, element_count, instance_count,
        first_element, vertex_offset, first_instance);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result backend_plugin_loader::finish_command_recorder(
    granit_backend_plugin_instance instance, granit_backend_plugin_command_recorder recorder,
    granit_backend_plugin_command_buffer* command_buffer) noexcept {
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

GRANIT_LOADER_DESTROY_METHOD(destroy_command_buffer, destroy_command_buffer,
                             granit_backend_plugin_command_buffer)
GRANIT_LOADER_DESTROY_METHOD(submit_command_buffer, submit_command_buffer,
                             granit_backend_plugin_command_buffer)

granit_result backend_plugin_loader::recorder_copy_texture_to_buffer(
    granit_backend_plugin_instance instance, granit_backend_plugin_command_recorder recorder,
    granit_backend_plugin_texture texture, granit_backend_plugin_buffer buffer, std::uint32_t width,
    std::uint32_t height, std::uint32_t bytes_per_row) noexcept {
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

#undef GRANIT_LOADER_CREATE_METHOD
#undef GRANIT_LOADER_DESTROY_METHOD

void backend_plugin_loader::close() noexcept {
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
  library_.close();
}

} // namespace granit::detail
