// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_BACKEND_WEBGPU_RESOURCE_ADAPTER_H_
#define GRANIT_BACKEND_WEBGPU_RESOURCE_ADAPTER_H_

#include <memory>

#include "backend/plugin/plugin_loader.h"
#include "backend/resource_management.h"

namespace granit::detail {

struct webgpu_resource_context;

/** 将公共基础资源契约适配到 WebGPU Provider。 */
class webgpu_resource_adapter {
public:
  webgpu_resource_adapter(backend_plugin_loader& loader, granit_backend_plugin_instance instance);

  [[nodiscard]] std::unique_ptr<backend_buffer_resource> allocate_buffer() const;
  [[nodiscard]] granit_result create_buffer(const granit_buffer_desc& desc,
                                            backend_buffer_resource& resource) const noexcept;
  [[nodiscard]] void* mapped_data(backend_buffer_resource& resource) const noexcept;
  [[nodiscard]] granit_result flush(backend_buffer_resource& resource, std::uint64_t offset,
                                    std::uint64_t size) const noexcept;
  [[nodiscard]] granit_result invalidate(backend_buffer_resource& resource, std::uint64_t offset,
                                         std::uint64_t size) const noexcept;
  [[nodiscard]] granit_result upload(backend_buffer_resource& resource, std::uint64_t offset,
                                     const void* data, std::uint64_t size) const noexcept;
  [[nodiscard]] granit_result
  upload_batch(std::span<const backend_upload_operation> uploads) const noexcept;
  [[nodiscard]] granit_backend_plugin_buffer
  native_buffer(backend_buffer_resource& resource) const noexcept;
  [[nodiscard]] std::unique_ptr<backend_texture_resource> allocate_texture() const;
  [[nodiscard]] granit_result create_texture(const granit_texture_desc& desc,
                                             backend_texture_resource& resource) const noexcept;
  [[nodiscard]] granit_backend_plugin_texture
  native_texture(backend_texture_resource& resource) const noexcept;
  [[nodiscard]] granit_result
  upload_texture(backend_texture_resource& resource, const void* data, std::uint64_t size,
                 const granit_texture_data_layout& layout,
                 const granit_texture_write_region& region) const noexcept;
  [[nodiscard]] std::unique_ptr<backend_texture_view_resource> allocate_texture_view() const;
  [[nodiscard]] granit_result
  create_texture_view(backend_texture_resource& texture, const granit_texture_desc& texture_desc,
                      const granit_texture_view_desc& desc,
                      backend_texture_view_resource& resource) const noexcept;
  [[nodiscard]] granit_backend_plugin_texture_view
  native_texture_view(backend_texture_view_resource& resource) const noexcept;
  [[nodiscard]] std::unique_ptr<backend_sampler_resource> allocate_sampler() const;
  [[nodiscard]] granit_result create_sampler(const granit_sampler_desc& desc,
                                             backend_sampler_resource& resource) const noexcept;
  [[nodiscard]] std::unique_ptr<backend_bind_group_layout_resource>
  allocate_bind_group_layout() const;
  [[nodiscard]] granit_result
  create_bind_group_layout(std::span<const granit_bind_group_layout_entry> entries,
                           backend_bind_group_layout_resource& resource) const noexcept;
  [[nodiscard]] std::unique_ptr<backend_bind_group_resource> allocate_bind_group() const;
  [[nodiscard]] granit_result
  create_bind_group(backend_bind_group_layout_resource& layout,
                    std::span<const backend_bind_group_write> writes,
                    backend_bind_group_resource& resource) const noexcept;
  [[nodiscard]] granit_backend_plugin_bind_group_layout
  native_bind_group_layout(backend_bind_group_layout_resource& resource) const noexcept;
  [[nodiscard]] granit_backend_plugin_bind_group
  native_bind_group(backend_bind_group_resource& resource) const noexcept;

private:
  std::shared_ptr<webgpu_resource_context> context_;
};

} // namespace granit::detail

#endif
