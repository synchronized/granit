// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_BACKEND_VULKAN_RESOURCES_H_
#define GRANIT_BACKEND_VULKAN_RESOURCES_H_

#include <memory>
#include <volk.h>

#include "backend/resources.h"

namespace granit::detail {

class renderer_state;

class vulkan_texture_view_resource final : public backend_texture_view_resource {
public:
  explicit vulkan_texture_view_resource(std::shared_ptr<renderer_state> renderer) noexcept;
  ~vulkan_texture_view_resource() override;

  [[nodiscard]] VkImageView& native() noexcept { return native_; }
  [[nodiscard]] VkImageView native() const noexcept { return native_; }

private:
  std::shared_ptr<renderer_state> renderer_;
  VkImageView native_{VK_NULL_HANDLE};
};

class vulkan_sampler_resource final : public backend_sampler_resource {
public:
  explicit vulkan_sampler_resource(std::shared_ptr<renderer_state> renderer) noexcept;
  ~vulkan_sampler_resource() override;

  [[nodiscard]] VkSampler& native() noexcept { return native_; }
  [[nodiscard]] VkSampler native() const noexcept { return native_; }

private:
  std::shared_ptr<renderer_state> renderer_;
  VkSampler native_{VK_NULL_HANDLE};
};

} // namespace granit::detail

#endif
