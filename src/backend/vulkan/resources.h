// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_BACKEND_VULKAN_RESOURCES_H_
#define GRANIT_BACKEND_VULKAN_RESOURCES_H_

#include <memory>
#include <volk.h>

#include "backend/resources.h"
#include "backend/vulkan/memory_allocator.h"

namespace granit::detail {

class renderer_state;

class vulkan_buffer_resource final : public backend_buffer_resource {
public:
  explicit vulkan_buffer_resource(std::shared_ptr<renderer_state> renderer) noexcept;
  ~vulkan_buffer_resource() override;

  [[nodiscard]] vulkan_buffer_allocation& native() noexcept { return native_; }
  [[nodiscard]] const vulkan_buffer_allocation& native() const noexcept { return native_; }

private:
  std::shared_ptr<renderer_state> renderer_;
  vulkan_buffer_allocation native_{};
};

class vulkan_texture_resource final : public backend_texture_resource {
public:
  explicit vulkan_texture_resource(std::shared_ptr<renderer_state> renderer,
                                   bool owned = true) noexcept;
  ~vulkan_texture_resource() override;

  [[nodiscard]] vulkan_image_allocation& native() noexcept { return native_; }
  [[nodiscard]] const vulkan_image_allocation& native() const noexcept { return native_; }

private:
  std::shared_ptr<renderer_state> renderer_;
  vulkan_image_allocation native_{};
  bool owned_{true};
};

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

class vulkan_shader_resource final : public backend_shader_resource {
public:
  explicit vulkan_shader_resource(std::shared_ptr<renderer_state> renderer) noexcept;
  ~vulkan_shader_resource() override;

  [[nodiscard]] VkShaderModule& native() noexcept { return native_; }
  [[nodiscard]] VkShaderModule native() const noexcept { return native_; }

private:
  std::shared_ptr<renderer_state> renderer_;
  VkShaderModule native_{VK_NULL_HANDLE};
};

class vulkan_bind_group_layout_resource final : public backend_bind_group_layout_resource {
public:
  explicit vulkan_bind_group_layout_resource(std::shared_ptr<renderer_state> renderer) noexcept;
  ~vulkan_bind_group_layout_resource() override;

  [[nodiscard]] VkDescriptorSetLayout& native() noexcept { return native_; }
  [[nodiscard]] VkDescriptorSetLayout native() const noexcept { return native_; }

private:
  std::shared_ptr<renderer_state> renderer_;
  VkDescriptorSetLayout native_{VK_NULL_HANDLE};
};

class vulkan_bind_group_resource final : public backend_bind_group_resource {
public:
  explicit vulkan_bind_group_resource(std::shared_ptr<renderer_state> renderer) noexcept;
  ~vulkan_bind_group_resource() override;

  [[nodiscard]] VkDescriptorPool& pool() noexcept { return pool_; }
  [[nodiscard]] VkDescriptorSet& set() noexcept { return set_; }
  [[nodiscard]] VkDescriptorSet set() const noexcept { return set_; }

private:
  std::shared_ptr<renderer_state> renderer_;
  VkDescriptorPool pool_{VK_NULL_HANDLE};
  VkDescriptorSet set_{VK_NULL_HANDLE};
};

class vulkan_pipeline_layout_resource final : public backend_pipeline_layout_resource {
public:
  explicit vulkan_pipeline_layout_resource(std::shared_ptr<renderer_state> renderer) noexcept;
  ~vulkan_pipeline_layout_resource() override;

  [[nodiscard]] VkPipelineLayout& native() noexcept { return native_; }
  [[nodiscard]] VkPipelineLayout native() const noexcept { return native_; }

private:
  std::shared_ptr<renderer_state> renderer_;
  VkPipelineLayout native_{VK_NULL_HANDLE};
};

class vulkan_graphics_pipeline_resource final : public backend_graphics_pipeline_resource {
public:
  explicit vulkan_graphics_pipeline_resource(std::shared_ptr<renderer_state> renderer) noexcept;
  ~vulkan_graphics_pipeline_resource() override;

  [[nodiscard]] VkPipeline& native() noexcept { return native_; }
  [[nodiscard]] VkPipeline native() const noexcept { return native_; }

private:
  std::shared_ptr<renderer_state> renderer_;
  VkPipeline native_{VK_NULL_HANDLE};
};

class vulkan_compute_pipeline_resource final : public backend_compute_pipeline_resource {
public:
  explicit vulkan_compute_pipeline_resource(std::shared_ptr<renderer_state> renderer) noexcept;
  ~vulkan_compute_pipeline_resource() override;

  [[nodiscard]] VkPipeline& native() noexcept { return native_; }
  [[nodiscard]] VkPipeline native() const noexcept { return native_; }

private:
  std::shared_ptr<renderer_state> renderer_;
  VkPipeline native_{VK_NULL_HANDLE};
};

} // namespace granit::detail

#endif
