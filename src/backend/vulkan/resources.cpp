// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "backend/vulkan/resources.h"

#include <utility>

#include "renderer/renderer_state.h"

namespace granit::detail {

vulkan_buffer_resource::vulkan_buffer_resource(std::shared_ptr<renderer_state> renderer) noexcept
    : renderer_(std::move(renderer)) {}

vulkan_buffer_resource::~vulkan_buffer_resource() {
  if (renderer_) {
    renderer_->destroy_native_buffer(native_);
  }
}

vulkan_texture_resource::vulkan_texture_resource(std::shared_ptr<renderer_state> renderer,
                                                 bool owned) noexcept
    : renderer_(std::move(renderer)), owned_(owned) {}

vulkan_texture_resource::~vulkan_texture_resource() {
  if (renderer_ && owned_) {
    renderer_->destroy_native_texture(native_);
  }
}

vulkan_texture_view_resource::vulkan_texture_view_resource(
    std::shared_ptr<renderer_state> renderer) noexcept
    : renderer_(std::move(renderer)) {}

vulkan_texture_view_resource::~vulkan_texture_view_resource() {
  if (renderer_) {
    renderer_->destroy_native_texture_view(native_);
  }
}

vulkan_sampler_resource::vulkan_sampler_resource(std::shared_ptr<renderer_state> renderer) noexcept
    : renderer_(std::move(renderer)) {}

vulkan_sampler_resource::~vulkan_sampler_resource() {
  if (renderer_) {
    renderer_->destroy_native_sampler(native_);
  }
}

vulkan_shader_resource::vulkan_shader_resource(std::shared_ptr<renderer_state> renderer) noexcept
    : renderer_(std::move(renderer)) {}

vulkan_shader_resource::~vulkan_shader_resource() {
  if (renderer_) {
    renderer_->destroy_native_shader(native_);
  }
}

vulkan_bind_group_layout_resource::vulkan_bind_group_layout_resource(
    std::shared_ptr<renderer_state> renderer) noexcept
    : renderer_(std::move(renderer)) {}

vulkan_bind_group_layout_resource::~vulkan_bind_group_layout_resource() {
  if (renderer_) {
    renderer_->destroy_native_bind_group_layout(native_);
  }
}

vulkan_bind_group_resource::vulkan_bind_group_resource(
    std::shared_ptr<renderer_state> renderer) noexcept
    : renderer_(std::move(renderer)) {}

vulkan_bind_group_resource::~vulkan_bind_group_resource() {
  if (renderer_) {
    renderer_->destroy_native_bind_group(pool_);
  }
}

vulkan_pipeline_layout_resource::vulkan_pipeline_layout_resource(
    std::shared_ptr<renderer_state> renderer) noexcept
    : renderer_(std::move(renderer)) {}

vulkan_pipeline_layout_resource::~vulkan_pipeline_layout_resource() {
  if (renderer_) {
    renderer_->destroy_native_pipeline_layout(native_);
  }
}

vulkan_graphics_pipeline_resource::vulkan_graphics_pipeline_resource(
    std::shared_ptr<renderer_state> renderer) noexcept
    : renderer_(std::move(renderer)) {}

vulkan_graphics_pipeline_resource::~vulkan_graphics_pipeline_resource() {
  if (renderer_) {
    renderer_->destroy_native_graphics_pipeline(native_);
  }
}

vulkan_compute_pipeline_resource::vulkan_compute_pipeline_resource(
    std::shared_ptr<renderer_state> renderer) noexcept
    : renderer_(std::move(renderer)) {}

vulkan_compute_pipeline_resource::~vulkan_compute_pipeline_resource() {
  if (renderer_) {
    renderer_->destroy_native_compute_pipeline(native_);
  }
}

vulkan_command_recorder_resource::vulkan_command_recorder_resource(
    std::shared_ptr<renderer_state> renderer) noexcept
    : renderer_(std::move(renderer)) {}

vulkan_command_recorder_resource::~vulkan_command_recorder_resource() {
  if (renderer_) {
    renderer_->destroy_native_command_recorder(native_);
  }
}

vulkan_surface_resource::vulkan_surface_resource(std::shared_ptr<renderer_state> renderer) noexcept
    : renderer_(std::move(renderer)) {}

vulkan_surface_resource::~vulkan_surface_resource() {
  if (renderer_) {
    renderer_->destroy_native_surface(native_);
  }
}

vulkan_swapchain_resource::vulkan_swapchain_resource(
    std::shared_ptr<renderer_state> renderer) noexcept
    : renderer_(std::move(renderer)) {}

vulkan_swapchain_resource::~vulkan_swapchain_resource() {
  if (renderer_) {
    renderer_->destroy_native_swapchain(native_);
  }
}

vulkan_timestamp_query_pool_resource::vulkan_timestamp_query_pool_resource(
    std::shared_ptr<renderer_state> renderer) noexcept
    : renderer_(std::move(renderer)) {}

vulkan_timestamp_query_pool_resource::~vulkan_timestamp_query_pool_resource() {
  if (renderer_) {
    native_.destroy(renderer_->device());
  }
}

} // namespace granit::detail
