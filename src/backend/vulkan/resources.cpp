// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "backend/vulkan/resources.h"

#include <utility>

#include "renderer/renderer_state.h"

namespace granit::detail {

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

} // namespace granit::detail
