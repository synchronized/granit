// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_BACKEND_RESOURCE_MANAGEMENT_H_
#define GRANIT_BACKEND_RESOURCE_MANAGEMENT_H_

#include <cstdint>
#include <memory>
#include <span>

#include <granit/core/result.h>
#include <granit/renderer/buffer.h>
#include <granit/renderer/pipeline.h>
#include <granit/renderer/sampler.h>
#include <granit/renderer/texture.h>

#include "backend/binding.h"
#include "backend/resources.h"
#include "backend/upload.h"

namespace granit::detail {

/** 统一基础资源、绑定资源和上传批次的后端管理能力。 */
class backend_resource_renderer {
public:
  backend_resource_renderer() = default;
  virtual ~backend_resource_renderer() = default;
  backend_resource_renderer(const backend_resource_renderer&) = delete;
  backend_resource_renderer& operator=(const backend_resource_renderer&) = delete;

  [[nodiscard]] virtual std::unique_ptr<backend_buffer_resource> allocate_buffer_resource() = 0;
  [[nodiscard]] virtual granit_result create_buffer(const granit_buffer_desc& desc,
                                                    backend_buffer_resource& buffer) noexcept = 0;
  [[nodiscard]] virtual void* mapped_buffer_data(backend_buffer_resource& buffer) noexcept = 0;
  [[nodiscard]] virtual granit_result flush_buffer(backend_buffer_resource& buffer,
                                                   std::uint64_t offset,
                                                   std::uint64_t size) noexcept = 0;
  [[nodiscard]] virtual granit_result invalidate_buffer(backend_buffer_resource& buffer,
                                                        std::uint64_t offset,
                                                        std::uint64_t size) noexcept = 0;
  [[nodiscard]] virtual granit_result upload_buffer(backend_buffer_resource& buffer,
                                                    std::uint64_t offset, const void* data,
                                                    std::uint64_t size) noexcept = 0;
  [[nodiscard]] virtual granit_result
  upload_batch(std::span<const backend_upload_operation> uploads) noexcept = 0;
  [[nodiscard]] virtual std::unique_ptr<backend_texture_resource> allocate_texture_resource() = 0;
  [[nodiscard]] virtual granit_result
  create_texture(const granit_texture_desc& desc, backend_texture_resource& texture) noexcept = 0;
  [[nodiscard]] virtual granit_result
  upload_texture(backend_texture_resource& texture, granit_texture_format format, const void* data,
                 std::uint64_t size, const granit_texture_data_layout& layout,
                 const granit_texture_write_region& region) noexcept = 0;
  [[nodiscard]] virtual std::unique_ptr<backend_texture_view_resource>
  allocate_texture_view_resource() = 0;
  [[nodiscard]] virtual granit_result
  create_texture_view(backend_texture_resource& texture, const granit_texture_desc& texture_desc,
                      const granit_texture_view_desc& view_desc,
                      backend_texture_view_resource& view) noexcept = 0;
  [[nodiscard]] virtual std::unique_ptr<backend_sampler_resource> allocate_sampler_resource() = 0;
  [[nodiscard]] virtual granit_result
  create_sampler(const granit_sampler_desc& desc, backend_sampler_resource& sampler) noexcept = 0;
  [[nodiscard]] virtual std::unique_ptr<backend_bind_group_layout_resource>
  allocate_bind_group_layout_resource() = 0;
  [[nodiscard]] virtual granit_result
  create_bind_group_layout(std::span<const granit_bind_group_layout_entry> entries,
                           backend_bind_group_layout_resource& layout) noexcept = 0;
  [[nodiscard]] virtual std::unique_ptr<backend_bind_group_resource>
  allocate_bind_group_resource() = 0;
  [[nodiscard]] virtual granit_result
  create_bind_group(backend_bind_group_layout_resource& layout,
                    std::span<const backend_bind_group_write> writes,
                    backend_bind_group_resource& bind_group) noexcept = 0;
  [[nodiscard]] virtual std::unique_ptr<backend_compute_pipeline_resource>
  allocate_compute_pipeline_resource() = 0;
  [[nodiscard]] virtual granit_result
  create_compute_pipeline(backend_pipeline_layout_resource& layout,
                          backend_shader_resource& compute_shader, const char* compute_entry,
                          backend_compute_pipeline_resource& pipeline) noexcept = 0;
};

} // namespace granit::detail

#endif
