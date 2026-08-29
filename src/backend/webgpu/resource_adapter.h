// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_BACKEND_WEBGPU_RESOURCE_ADAPTER_H_
#define GRANIT_BACKEND_WEBGPU_RESOURCE_ADAPTER_H_

#include <memory>

#include "backend/plugin_loader.h"
#include "backend/resource_management.h"

namespace granit::detail {

struct webgpu_resource_context;

/** 将公共 Buffer 资源契约适配到 WebGPU Provider。 */
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

private:
  std::shared_ptr<webgpu_resource_context> context_;
};

} // namespace granit::detail

#endif
