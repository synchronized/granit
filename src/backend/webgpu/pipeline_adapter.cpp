// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "backend/webgpu/pipeline_adapter.h"

#include <utility>

namespace granit::detail {

struct webgpu_pipeline_context {
  backend_plugin_loader* loader{};
  granit_backend_plugin_instance instance{};
};

namespace {

class webgpu_pipeline_layout_resource final : public backend_pipeline_layout_resource {
public:
  explicit webgpu_pipeline_layout_resource(std::shared_ptr<webgpu_pipeline_context> context)
      : context_(std::move(context)) {}
  ~webgpu_pipeline_layout_resource() override {
    if (handle_ != 0) {
      static_cast<void>(context_->loader->destroy_pipeline_layout(context_->instance, handle_));
    }
  }
  std::shared_ptr<webgpu_pipeline_context> context_;
  granit_backend_plugin_pipeline_layout handle_{};
};

class webgpu_graphics_pipeline_resource final : public backend_graphics_pipeline_resource {
public:
  explicit webgpu_graphics_pipeline_resource(std::shared_ptr<webgpu_pipeline_context> context)
      : context_(std::move(context)) {}
  ~webgpu_graphics_pipeline_resource() override {
    if (handle_ != 0) {
      static_cast<void>(context_->loader->destroy_render_pipeline(context_->instance, handle_));
    }
  }
  std::shared_ptr<webgpu_pipeline_context> context_;
  granit_backend_plugin_render_pipeline handle_{};
};

webgpu_pipeline_layout_resource* as_layout(backend_pipeline_layout_resource& resource) {
  return dynamic_cast<webgpu_pipeline_layout_resource*>(&resource);
}

webgpu_graphics_pipeline_resource* as_pipeline(backend_graphics_pipeline_resource& resource) {
  return dynamic_cast<webgpu_graphics_pipeline_resource*>(&resource);
}

std::uint32_t to_plugin_format(granit_texture_format format) noexcept {
  switch (format) {
  case GRANIT_TEXTURE_FORMAT_RGBA8_UNORM:
    return GRANIT_BACKEND_PLUGIN_TEXTURE_FORMAT_RGBA8_UNORM;
  case GRANIT_TEXTURE_FORMAT_BGRA8_UNORM:
    return GRANIT_BACKEND_PLUGIN_TEXTURE_FORMAT_BGRA8_UNORM;
  default:
    return 0;
  }
}

} // namespace

webgpu_pipeline_adapter::webgpu_pipeline_adapter(backend_plugin_loader& loader,
                                                 granit_backend_plugin_instance instance)
    : context_(
          std::make_shared<webgpu_pipeline_context>(webgpu_pipeline_context{&loader, instance})) {}

std::unique_ptr<backend_pipeline_layout_resource>
webgpu_pipeline_adapter::allocate_pipeline_layout() const {
  return std::make_unique<webgpu_pipeline_layout_resource>(context_);
}

std::unique_ptr<backend_graphics_pipeline_resource>
webgpu_pipeline_adapter::allocate_graphics_pipeline() const {
  return std::make_unique<webgpu_graphics_pipeline_resource>(context_);
}

granit_result webgpu_pipeline_adapter::create_pipeline_layout(
    backend_pipeline_layout_resource& resource) const noexcept {
  auto* layout = as_layout(resource);
  if (layout == nullptr || layout->handle_ != 0) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  return context_->loader->create_pipeline_layout(context_->instance, 0, &layout->handle_);
}

granit_result webgpu_pipeline_adapter::create_graphics_pipeline(
    backend_graphics_pipeline_resource& resource, backend_pipeline_layout_resource& layout_resource,
    granit_backend_plugin_shader vertex_shader, granit_backend_plugin_shader fragment_shader,
    granit_texture_format color_format) const noexcept {
  auto* pipeline = as_pipeline(resource);
  auto* layout = as_layout(layout_resource);
  const auto plugin_format = to_plugin_format(color_format);
  if (pipeline == nullptr || pipeline->handle_ != 0 || layout == nullptr || layout->handle_ == 0 ||
      vertex_shader == 0 || fragment_shader == 0 || plugin_format == 0) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  const granit_backend_plugin_render_pipeline_desc desc{
      sizeof(desc), 0, layout->handle_, vertex_shader, fragment_shader, plugin_format, 0};
  return context_->loader->create_render_pipeline(context_->instance, &desc, &pipeline->handle_);
}

granit_backend_plugin_render_pipeline webgpu_pipeline_adapter::native_handle(
    backend_graphics_pipeline_resource& resource) const noexcept {
  const auto* pipeline = as_pipeline(resource);
  return pipeline == nullptr ? 0 : pipeline->handle_;
}

} // namespace granit::detail
