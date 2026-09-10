// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "backend/webgpu/renderer_state.h"

#include "core/texture_format.h"

#include <new>
#include <string>
#include <vector>
#include <utility>

namespace granit::detail {

namespace {

class webgpu_shader_resource final : public backend_shader_resource {
public:
  webgpu_shader_resource(webgpu_context& context, webgpu_instance_handle instance) noexcept
      : context_(&context), instance_(instance) {}

  ~webgpu_shader_resource() override {
    if (handle_ != 0)
      static_cast<void>(context_->destroy_shader(instance_, handle_));
  }

  webgpu_context* context_{};
  webgpu_instance_handle instance_{};
  webgpu_shader handle_{};
};

webgpu_shader_resource* as_shader(backend_shader_resource& resource) noexcept {
  return dynamic_cast<webgpu_shader_resource*>(&resource);
}

} // namespace

std::unique_ptr<backend_buffer_resource> webgpu_renderer_state::allocate_buffer_resource() {
  return resources_ ? resources_->allocate_buffer() : nullptr;
}

granit_result webgpu_renderer_state::create_buffer(const granit_buffer_desc& desc,
                                                   backend_buffer_resource& buffer) noexcept {
  return resources_ ? resources_->create_buffer(desc, buffer) : GRANIT_ERROR_NOT_READY;
}

void* webgpu_renderer_state::mapped_buffer_data(backend_buffer_resource& buffer) noexcept {
  return resources_ ? resources_->mapped_data(buffer) : nullptr;
}

granit_result webgpu_renderer_state::flush_buffer(backend_buffer_resource& buffer,
                                                  std::uint64_t offset,
                                                  std::uint64_t size) noexcept {
  return resources_ ? resources_->flush(buffer, offset, size) : GRANIT_ERROR_NOT_READY;
}

granit_result webgpu_renderer_state::invalidate_buffer(backend_buffer_resource& buffer,
                                                       std::uint64_t offset,
                                                       std::uint64_t size) noexcept {
  return resources_ ? resources_->invalidate(buffer, offset, size) : GRANIT_ERROR_NOT_READY;
}

std::unique_ptr<backend_texture_resource> webgpu_renderer_state::allocate_texture_resource() {
  return resources_ ? resources_->allocate_texture() : nullptr;
}

granit_result webgpu_renderer_state::create_texture(const granit_texture_desc& desc,
                                                    backend_texture_resource& texture) noexcept {
  return resources_ ? resources_->create_texture(desc, texture) : GRANIT_ERROR_NOT_READY;
}

granit_result webgpu_renderer_state::upload_texture(
    backend_texture_resource& texture, granit_texture_format, const void* data, std::uint64_t size,
    const granit_texture_data_layout& layout, const granit_texture_write_region& region) noexcept {
  return resources_ ? resources_->upload_texture(texture, data, size, layout, region)
                    : GRANIT_ERROR_NOT_READY;
}

std::unique_ptr<backend_texture_view_resource>
webgpu_renderer_state::allocate_texture_view_resource() {
  return resources_ ? resources_->allocate_texture_view() : nullptr;
}

granit_result webgpu_renderer_state::create_texture_view(
    backend_texture_resource& texture, const granit_texture_desc& texture_desc,
    const granit_texture_view_desc& desc, backend_texture_view_resource& view) noexcept {
  return resources_ ? resources_->create_texture_view(texture, texture_desc, desc, view)
                    : GRANIT_ERROR_NOT_READY;
}

std::unique_ptr<backend_sampler_resource> webgpu_renderer_state::allocate_sampler_resource() {
  return resources_ ? resources_->allocate_sampler() : nullptr;
}

granit_result webgpu_renderer_state::create_sampler(const granit_sampler_desc& desc,
                                                    backend_sampler_resource& sampler) noexcept {
  return resources_ ? resources_->create_sampler(desc, sampler) : GRANIT_ERROR_NOT_READY;
}

std::unique_ptr<backend_bind_group_layout_resource>
webgpu_renderer_state::allocate_bind_group_layout_resource() {
  return resources_ ? resources_->allocate_bind_group_layout() : nullptr;
}

granit_result webgpu_renderer_state::create_bind_group_layout(
    std::span<const granit_bind_group_layout_entry> entries,
    backend_bind_group_layout_resource& layout) noexcept {
  return resources_ ? resources_->create_bind_group_layout(entries, layout)
                    : GRANIT_ERROR_NOT_READY;
}

std::unique_ptr<backend_bind_group_resource> webgpu_renderer_state::allocate_bind_group_resource() {
  return resources_ ? resources_->allocate_bind_group() : nullptr;
}

granit_result
webgpu_renderer_state::create_bind_group(backend_bind_group_layout_resource& layout,
                                         std::span<const backend_bind_group_write> writes,
                                         backend_bind_group_resource& group) noexcept {
  return resources_ ? resources_->create_bind_group(layout, writes, group) : GRANIT_ERROR_NOT_READY;
}

std::unique_ptr<backend_shader_resource> webgpu_renderer_state::allocate_shader_resource() {
  if (lifecycle_.state != backend_lifecycle_state::ready)
    return nullptr;
  return std::make_unique<webgpu_shader_resource>(context_, instance_);
}

granit_result webgpu_renderer_state::create_shader(backend_shader_resource& shader,
                                                   granit_shader_stage stage,
                                                   granit_shader_code_format code_format,
                                                   std::span<const std::byte> code,
                                                   std::string_view entry_point) noexcept {
  if (code_format != GRANIT_SHADER_CODE_FORMAT_WGSL)
    return GRANIT_ERROR_UNSUPPORTED;
  auto* resource = as_shader(shader);
  if (resource == nullptr || resource->handle_ != 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const webgpu_shader_desc desc{sizeof(desc),
                                stage,
                                reinterpret_cast<const char*>(code.data()),
                                code.size(),
                                entry_point.data(),
                                entry_point.size()};
  return context_.create_shader(instance_, &desc, &resource->handle_);
}

webgpu_shader
webgpu_renderer_state::native_shader(backend_shader_resource& resource) const noexcept {
  const auto* shader = as_shader(resource);
  return shader == nullptr ? webgpu_shader{} : shader->handle_;
}

} // namespace granit::detail
