// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "backend/vulkan/resources.h"

#include <utility>
#include <algorithm>

#include "backend/vulkan/renderer_state.h"

namespace granit::detail {

vulkan_buffer_resource::vulkan_buffer_resource(
    std::shared_ptr<vulkan_renderer_state> renderer) noexcept
    : renderer_(std::move(renderer)) {}

vulkan_buffer_resource::~vulkan_buffer_resource() {
  if (renderer_) {
    renderer_->destroy_native_buffer(native_);
  }
}

vulkan_texture_resource::vulkan_texture_resource(std::shared_ptr<vulkan_renderer_state> renderer,
                                                 bool owned) noexcept
    : renderer_(std::move(renderer)), owned_(owned) {}

vulkan_texture_resource::~vulkan_texture_resource() {
  if (renderer_ && owned_) {
    renderer_->destroy_native_texture(native_);
  }
}

vulkan_texture_view_resource::vulkan_texture_view_resource(
    std::shared_ptr<vulkan_renderer_state> renderer) noexcept
    : renderer_(std::move(renderer)) {}

vulkan_texture_view_resource::~vulkan_texture_view_resource() {
  if (renderer_) {
    renderer_->destroy_native_texture_view(native_);
  }
}

vulkan_sampler_resource::vulkan_sampler_resource(
    std::shared_ptr<vulkan_renderer_state> renderer) noexcept
    : renderer_(std::move(renderer)) {}

vulkan_sampler_resource::~vulkan_sampler_resource() {
  if (renderer_) {
    renderer_->destroy_native_sampler(native_);
  }
}

vulkan_resource_table_resource::vulkan_resource_table_resource(
    std::shared_ptr<vulkan_renderer_state> renderer) noexcept
    : renderer_(std::move(renderer)), registry_(renderer_->domain()) {}

vulkan_resource_table_resource::~vulkan_resource_table_resource() {
  if (renderer_) registry_.destroy(renderer_->device());
}

granit_result vulkan_resource_table_resource::initialize(
    const granit_resource_table_desc& desc) noexcept {
  if (!renderer_ || !renderer_->device().bindless_descriptor_indexing_supported())
    return GRANIT_ERROR_UNSUPPORTED;
  return registry_.initialize(renderer_->device(),
                              std::max(desc.sampled_texture_view_capacity, UINT32_C(1)),
                              std::max(desc.sampler_capacity, UINT32_C(1)),
                              std::max(desc.storage_buffer_capacity, UINT32_C(1)));
}

granit_result vulkan_resource_table_resource::update(
    const granit_resource_table_update_desc& update, backend_resource* resource) noexcept {
  if (resource == nullptr) return GRANIT_SUCCESS;
  switch (update.type) {
  case GRANIT_RESOURCE_TABLE_RESOURCE_SAMPLED_TEXTURE_VIEW: {
    const auto* view = dynamic_cast<vulkan_texture_view_resource*>(resource);
    if (view == nullptr) return GRANIT_ERROR_INVALID_ARGUMENT;
    return registry_.update_sampled_texture_at(renderer_->device(), update.slot, view->native());
  }
  case GRANIT_RESOURCE_TABLE_RESOURCE_SAMPLER: {
    const auto* sampler = dynamic_cast<vulkan_sampler_resource*>(resource);
    if (sampler == nullptr) return GRANIT_ERROR_INVALID_ARGUMENT;
    return registry_.update_sampler_at(renderer_->device(), update.slot, sampler->native());
  }
  case GRANIT_RESOURCE_TABLE_RESOURCE_STORAGE_BUFFER: {
    const auto* buffer = dynamic_cast<vulkan_buffer_resource*>(resource);
    if (buffer == nullptr) return GRANIT_ERROR_INVALID_ARGUMENT;
    const auto size = update.size == GRANIT_WHOLE_SIZE ? VK_WHOLE_SIZE : update.size;
    return registry_.update_storage_buffer_at(renderer_->device(), update.slot,
                                              buffer->native().buffer, update.offset, size);
  }
  default:
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
}

vulkan_shader_resource::vulkan_shader_resource(
    std::shared_ptr<vulkan_renderer_state> renderer) noexcept
    : renderer_(std::move(renderer)) {}

vulkan_shader_resource::~vulkan_shader_resource() {
  if (renderer_) {
    renderer_->destroy_native_shader(native_);
  }
}

vulkan_bind_group_layout_resource::vulkan_bind_group_layout_resource(
    std::shared_ptr<vulkan_renderer_state> renderer) noexcept
    : renderer_(std::move(renderer)) {}

vulkan_bind_group_layout_resource::~vulkan_bind_group_layout_resource() {
  if (renderer_) {
    renderer_->destroy_native_bind_group_layout(native_);
  }
}

vulkan_bind_group_resource::vulkan_bind_group_resource(
    std::shared_ptr<vulkan_renderer_state> renderer) noexcept
    : renderer_(std::move(renderer)) {}

vulkan_bind_group_resource::~vulkan_bind_group_resource() {
  if (renderer_) {
    renderer_->destroy_native_bind_group(pool_);
  }
}

vulkan_pipeline_layout_resource::vulkan_pipeline_layout_resource(
    std::shared_ptr<vulkan_renderer_state> renderer) noexcept
    : renderer_(std::move(renderer)) {}

vulkan_pipeline_layout_resource::~vulkan_pipeline_layout_resource() {
  if (renderer_) {
    renderer_->destroy_native_pipeline_layout(native_);
  }
}

vulkan_graphics_pipeline_resource::vulkan_graphics_pipeline_resource(
    std::shared_ptr<vulkan_renderer_state> renderer) noexcept
    : renderer_(std::move(renderer)) {}

vulkan_graphics_pipeline_resource::~vulkan_graphics_pipeline_resource() {
  if (renderer_) {
    renderer_->destroy_native_graphics_pipeline(native_);
  }
}

vulkan_compute_pipeline_resource::vulkan_compute_pipeline_resource(
    std::shared_ptr<vulkan_renderer_state> renderer) noexcept
    : renderer_(std::move(renderer)) {}

vulkan_compute_pipeline_resource::~vulkan_compute_pipeline_resource() {
  if (renderer_) {
    renderer_->destroy_native_compute_pipeline(native_);
  }
}

vulkan_command_recorder_resource::vulkan_command_recorder_resource(
    std::shared_ptr<vulkan_renderer_state> renderer) noexcept
    : renderer_(std::move(renderer)) {}

vulkan_command_recorder_resource::~vulkan_command_recorder_resource() {
  if (renderer_) {
    renderer_->destroy_native_command_recorder(*this);
  }
}

vulkan_surface_resource::vulkan_surface_resource(
    std::shared_ptr<vulkan_renderer_state> renderer) noexcept
    : renderer_(std::move(renderer)) {}

vulkan_surface_resource::~vulkan_surface_resource() {
  if (renderer_) {
    renderer_->destroy_native_surface(native_);
  }
}

vulkan_swapchain_resource::vulkan_swapchain_resource(
    std::shared_ptr<vulkan_renderer_state> renderer) noexcept
    : renderer_(std::move(renderer)) {}

vulkan_swapchain_resource::~vulkan_swapchain_resource() {
  if (renderer_) {
    renderer_->destroy_native_swapchain(native_);
  }
}

vulkan_timestamp_query_pool_resource::vulkan_timestamp_query_pool_resource(
    std::shared_ptr<vulkan_renderer_state> renderer) noexcept
    : renderer_(std::move(renderer)) {}

vulkan_timestamp_query_pool_resource::~vulkan_timestamp_query_pool_resource() {
  if (renderer_) {
    native_.destroy(renderer_->device());
  }
}

} // namespace granit::detail
