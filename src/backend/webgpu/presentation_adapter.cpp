// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "backend/webgpu/presentation_adapter.h"

#include <new>
#include <utility>

namespace granit::detail {
namespace {

granit_backend_plugin_swapchain_desc to_plugin_desc(const backend_swapchain_desc& desc) {
  return {sizeof(granit_backend_plugin_swapchain_desc), desc.width, desc.height,
          desc.minimum_image_count, desc.present_mode};
}

granit_texture_format to_texture_format(granit_backend_plugin_texture_format format) {
  switch (format) {
  case GRANIT_BACKEND_PLUGIN_TEXTURE_FORMAT_RGBA8_UNORM:
    return GRANIT_TEXTURE_FORMAT_RGBA8_UNORM;
  case GRANIT_BACKEND_PLUGIN_TEXTURE_FORMAT_BGRA8_UNORM:
    return GRANIT_TEXTURE_FORMAT_BGRA8_UNORM;
  default:
    return GRANIT_TEXTURE_FORMAT_UNDEFINED;
  }
}

} // namespace

struct webgpu_presentation_context {
  backend_plugin_loader* loader{};
  granit_backend_plugin_instance instance{};
};

namespace {

class webgpu_surface_resource final : public backend_surface_resource {
public:
  explicit webgpu_surface_resource(std::shared_ptr<webgpu_presentation_context> context)
      : context_(std::move(context)) {}

  ~webgpu_surface_resource() override {
    if (handle_ != 0) {
      static_cast<void>(context_->loader->destroy_surface(context_->instance, handle_));
    }
  }

  std::shared_ptr<webgpu_presentation_context> context_;
  granit_backend_plugin_surface handle_{};
};

class webgpu_swapchain_resource final : public backend_swapchain_resource {
public:
  explicit webgpu_swapchain_resource(std::shared_ptr<webgpu_presentation_context> context)
      : context_(std::move(context)) {}

  ~webgpu_swapchain_resource() override {
    if (handle_ != 0) {
      static_cast<void>(context_->loader->destroy_swapchain(context_->instance, handle_));
    }
  }

  std::shared_ptr<webgpu_presentation_context> context_;
  granit_backend_plugin_swapchain handle_{};
};

/** 借用资源由 Swapchain 在 Present、Cancel 或重建时统一失效。 */
class webgpu_borrowed_texture_resource final : public backend_texture_resource {
public:
  explicit webgpu_borrowed_texture_resource(granit_backend_plugin_texture handle)
      : handle_(handle) {}

  granit_backend_plugin_texture handle_{};
};

class webgpu_borrowed_texture_view_resource final : public backend_texture_view_resource {
public:
  explicit webgpu_borrowed_texture_view_resource(granit_backend_plugin_texture_view handle)
      : handle_(handle) {}

  granit_backend_plugin_texture_view handle_{};
};

webgpu_surface_resource* as_surface(backend_surface_resource& resource) {
  return dynamic_cast<webgpu_surface_resource*>(&resource);
}

webgpu_swapchain_resource* as_swapchain(backend_swapchain_resource& resource) {
  return dynamic_cast<webgpu_swapchain_resource*>(&resource);
}

} // namespace

webgpu_presentation_adapter::webgpu_presentation_adapter(backend_plugin_loader& loader,
                                                         granit_backend_plugin_instance instance)
    : context_(std::make_shared<webgpu_presentation_context>(
          webgpu_presentation_context{&loader, instance})) {}

std::unique_ptr<backend_surface_resource> webgpu_presentation_adapter::allocate_surface() const {
  return std::make_unique<webgpu_surface_resource>(context_);
}

std::unique_ptr<backend_swapchain_resource>
webgpu_presentation_adapter::allocate_swapchain() const {
  return std::make_unique<webgpu_swapchain_resource>(context_);
}

granit_result
webgpu_presentation_adapter::create_canvas_surface(backend_surface_resource& resource,
                                                   const char* selector,
                                                   std::uint32_t selector_length) const noexcept {
  auto* surface = as_surface(resource);
  if (surface == nullptr || surface->handle_ != 0) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  granit_backend_plugin_canvas_surface_desc desc{sizeof(desc), 0, selector, selector_length};
  return context_->loader->create_canvas_surface(context_->instance, &desc, &surface->handle_);
}

granit_result webgpu_presentation_adapter::create_swapchain(
    backend_surface_resource& surface_resource, const backend_swapchain_desc& desc,
    backend_swapchain_resource& swapchain_resource) const noexcept {
  auto* surface = as_surface(surface_resource);
  auto* swapchain = as_swapchain(swapchain_resource);
  if (surface == nullptr || surface->handle_ == 0 || swapchain == nullptr ||
      swapchain->handle_ != 0) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  const auto plugin_desc = to_plugin_desc(desc);
  return context_->loader->create_swapchain(context_->instance, surface->handle_, &plugin_desc,
                                            &swapchain->handle_);
}

granit_result
webgpu_presentation_adapter::recreate_swapchain(backend_swapchain_resource& resource,
                                                const backend_swapchain_desc& desc) const noexcept {
  auto* swapchain = as_swapchain(resource);
  if (swapchain == nullptr || swapchain->handle_ == 0) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  const auto plugin_desc = to_plugin_desc(desc);
  return context_->loader->recreate_swapchain(context_->instance, swapchain->handle_, &plugin_desc);
}

granit_result
webgpu_presentation_adapter::get_swapchain_info(backend_swapchain_resource& resource,
                                                backend_swapchain_info& info) const noexcept {
  auto* swapchain = as_swapchain(resource);
  if (swapchain == nullptr || swapchain->handle_ == 0) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  granit_backend_plugin_swapchain_info plugin_info{};
  plugin_info.struct_size = sizeof(plugin_info);
  const auto result =
      context_->loader->get_swapchain_info(context_->instance, swapchain->handle_, &plugin_info);
  if (result != GRANIT_SUCCESS) {
    return result;
  }
  const auto format = to_texture_format(plugin_info.format);
  if (format == GRANIT_TEXTURE_FORMAT_UNDEFINED) {
    return GRANIT_ERROR_UNSUPPORTED;
  }
  info = {plugin_info.width, plugin_info.height, plugin_info.image_count, plugin_info.present_mode,
          format};
  return GRANIT_SUCCESS;
}

granit_result webgpu_presentation_adapter::acquire_swapchain(
    backend_swapchain_resource& resource, backend_acquired_swapchain_frame& frame) const noexcept {
  auto* swapchain = as_swapchain(resource);
  if (swapchain == nullptr || swapchain->handle_ == 0) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  granit_backend_plugin_acquired_frame plugin_frame{};
  plugin_frame.struct_size = sizeof(plugin_frame);
  const auto result =
      context_->loader->acquire_swapchain(context_->instance, swapchain->handle_, &plugin_frame);
  if (result != GRANIT_SUCCESS) {
    return result;
  }

  backend_swapchain_info info{};
  const auto info_result = get_swapchain_info(resource, info);
  if (info_result != GRANIT_SUCCESS) {
    std::uint32_t ignored{};
    static_cast<void>(
        context_->loader->cancel_swapchain(context_->instance, swapchain->handle_, &ignored));
    return info_result;
  }

  granit_texture_desc texture_desc = GRANIT_TEXTURE_DESC_INIT;
  texture_desc.format = info.format;
  texture_desc.usage = GRANIT_TEXTURE_USAGE_COLOR_ATTACHMENT_BIT;
  texture_desc.memory_location = GRANIT_MEMORY_LOCATION_DEVICE;
  texture_desc.width = info.width;
  texture_desc.height = info.height;

  auto texture = std::unique_ptr<backend_texture_resource>(
      new (std::nothrow) webgpu_borrowed_texture_resource(plugin_frame.texture));
  auto view = std::unique_ptr<backend_texture_view_resource>(
      new (std::nothrow) webgpu_borrowed_texture_view_resource(plugin_frame.view));
  if (texture == nullptr || view == nullptr) {
    std::uint32_t ignored{};
    static_cast<void>(
        context_->loader->cancel_swapchain(context_->instance, swapchain->handle_, &ignored));
    return GRANIT_ERROR_OUT_OF_MEMORY;
  }

  frame = {};
  frame.image_index = plugin_frame.image_index;
  frame.needs_recreate = plugin_frame.needs_recreate != 0;
  frame.dynamic_backbuffer.texture = std::move(texture);
  frame.dynamic_backbuffer.view = std::move(view);
  frame.dynamic_backbuffer.desc = texture_desc;
  return GRANIT_SUCCESS;
}

granit_result webgpu_presentation_adapter::present_swapchain(backend_swapchain_resource& resource,
                                                             bool& needs_recreate) const noexcept {
  auto* swapchain = as_swapchain(resource);
  if (swapchain == nullptr || swapchain->handle_ == 0) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  std::uint32_t plugin_needs_recreate{};
  const auto result = context_->loader->present_swapchain(context_->instance, swapchain->handle_,
                                                          &plugin_needs_recreate);
  needs_recreate = plugin_needs_recreate != 0;
  return result;
}

granit_result webgpu_presentation_adapter::cancel_swapchain(backend_swapchain_resource& resource,
                                                            bool& needs_recreate) const noexcept {
  auto* swapchain = as_swapchain(resource);
  if (swapchain == nullptr || swapchain->handle_ == 0) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  std::uint32_t plugin_needs_recreate{};
  const auto result = context_->loader->cancel_swapchain(context_->instance, swapchain->handle_,
                                                         &plugin_needs_recreate);
  needs_recreate = plugin_needs_recreate != 0;
  return result;
}

granit_backend_plugin_texture_view
webgpu_presentation_adapter::native_view(backend_texture_view_resource& resource) const noexcept {
  const auto* view = dynamic_cast<webgpu_borrowed_texture_view_resource*>(&resource);
  return view == nullptr ? 0 : view->handle_;
}

} // namespace granit::detail
