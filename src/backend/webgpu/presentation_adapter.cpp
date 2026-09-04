// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "backend/webgpu/presentation_adapter.h"

#include <new>
#include <utility>

namespace granit::detail {
namespace {

granit_webgpu_provider_swapchain_desc to_provider_desc(const backend_swapchain_desc& desc) {
  return {sizeof(granit_webgpu_provider_swapchain_desc), desc.width, desc.height,
          desc.minimum_image_count, desc.present_mode};
}

granit_texture_format to_texture_format(granit_webgpu_provider_texture_format format) {
  switch (format) {
  case GRANIT_WEBGPU_PROVIDER_TEXTURE_FORMAT_RGBA8_UNORM:
    return GRANIT_TEXTURE_FORMAT_RGBA8_UNORM;
  case GRANIT_WEBGPU_PROVIDER_TEXTURE_FORMAT_BGRA8_UNORM:
    return GRANIT_TEXTURE_FORMAT_BGRA8_UNORM;
  default:
    return GRANIT_TEXTURE_FORMAT_UNDEFINED;
  }
}

} // namespace

struct webgpu_presentation_context {
  webgpu_provider_dispatch* provider{};
  granit_webgpu_provider_instance instance{};
};

namespace {

class webgpu_surface_resource final : public backend_surface_resource {
public:
  explicit webgpu_surface_resource(std::shared_ptr<webgpu_presentation_context> context)
      : context_(std::move(context)) {}

  ~webgpu_surface_resource() override {
    if (handle_ != 0) {
      static_cast<void>(context_->provider->destroy_surface(context_->instance, handle_));
    }
  }

  std::shared_ptr<webgpu_presentation_context> context_;
  granit_webgpu_provider_surface handle_{};
};

class webgpu_swapchain_resource final : public backend_swapchain_resource {
public:
  explicit webgpu_swapchain_resource(std::shared_ptr<webgpu_presentation_context> context)
      : context_(std::move(context)) {}

  ~webgpu_swapchain_resource() override {
    if (handle_ != 0) {
      static_cast<void>(context_->provider->destroy_swapchain(context_->instance, handle_));
    }
  }

  std::shared_ptr<webgpu_presentation_context> context_;
  granit_webgpu_provider_swapchain handle_{};
};

/** 借用资源由 Swapchain 在 Present、Cancel 或重建时统一失效。 */
class webgpu_borrowed_texture_resource final : public backend_texture_resource {
public:
  explicit webgpu_borrowed_texture_resource(granit_webgpu_provider_texture handle)
      : handle_(handle) {}

  granit_webgpu_provider_texture handle_{};
};

class webgpu_borrowed_texture_view_resource final : public backend_texture_view_resource {
public:
  explicit webgpu_borrowed_texture_view_resource(granit_webgpu_provider_texture_view handle)
      : handle_(handle) {}

  granit_webgpu_provider_texture_view handle_{};
};

webgpu_surface_resource* as_surface(backend_surface_resource& resource) {
  return dynamic_cast<webgpu_surface_resource*>(&resource);
}

webgpu_swapchain_resource* as_swapchain(backend_swapchain_resource& resource) {
  return dynamic_cast<webgpu_swapchain_resource*>(&resource);
}

} // namespace

webgpu_presentation_adapter::webgpu_presentation_adapter(webgpu_provider_dispatch& provider,
                                                         granit_webgpu_provider_instance instance)
    : context_(std::make_shared<webgpu_presentation_context>(
          webgpu_presentation_context{&provider, instance})) {}

std::unique_ptr<backend_surface_resource> webgpu_presentation_adapter::allocate_surface() const {
  return std::make_unique<webgpu_surface_resource>(context_);
}

std::unique_ptr<backend_swapchain_resource>
webgpu_presentation_adapter::allocate_swapchain() const {
  return std::make_unique<webgpu_swapchain_resource>(context_);
}

granit_result webgpu_presentation_adapter::create_win32_surface(backend_surface_resource& resource,
                                                                void* instance,
                                                                void* window) const noexcept {
  auto* surface = as_surface(resource);
  if (surface == nullptr || surface->handle_ != 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  granit_webgpu_provider_win32_surface_desc desc{sizeof(desc), 0, instance, window};
  return context_->provider->create_win32_surface(context_->instance, &desc, &surface->handle_);
}

granit_result webgpu_presentation_adapter::create_xcb_surface(backend_surface_resource& resource,
                                                              void* connection,
                                                              std::uint32_t window) const noexcept {
  auto* surface = as_surface(resource);
  if (surface == nullptr || surface->handle_ != 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  granit_webgpu_provider_xcb_surface_desc desc{sizeof(desc), 0, connection, window, 0};
  return context_->provider->create_xcb_surface(context_->instance, &desc, &surface->handle_);
}

granit_result webgpu_presentation_adapter::create_wayland_surface(
    backend_surface_resource& resource, void* display, void* native_surface) const noexcept {
  auto* surface = as_surface(resource);
  if (surface == nullptr || surface->handle_ != 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  granit_webgpu_provider_wayland_surface_desc desc{sizeof(desc), 0, display, native_surface};
  return context_->provider->create_wayland_surface(context_->instance, &desc, &surface->handle_);
}

granit_result
webgpu_presentation_adapter::create_canvas_surface(backend_surface_resource& resource,
                                                   const char* selector,
                                                   std::uint32_t selector_length) const noexcept {
  auto* surface = as_surface(resource);
  if (surface == nullptr || surface->handle_ != 0) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  granit_webgpu_provider_canvas_surface_desc desc{sizeof(desc), 0, selector, selector_length};
  return context_->provider->create_canvas_surface(context_->instance, &desc, &surface->handle_);
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
  const auto provider_desc = to_provider_desc(desc);
  return context_->provider->create_swapchain(context_->instance, surface->handle_, &provider_desc,
                                              &swapchain->handle_);
}

granit_result
webgpu_presentation_adapter::recreate_swapchain(backend_swapchain_resource& resource,
                                                const backend_swapchain_desc& desc) const noexcept {
  auto* swapchain = as_swapchain(resource);
  if (swapchain == nullptr || swapchain->handle_ == 0) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  const auto provider_desc = to_provider_desc(desc);
  return context_->provider->recreate_swapchain(context_->instance, swapchain->handle_,
                                                &provider_desc);
}

granit_result
webgpu_presentation_adapter::get_swapchain_info(backend_swapchain_resource& resource,
                                                backend_swapchain_info& info) const noexcept {
  auto* swapchain = as_swapchain(resource);
  if (swapchain == nullptr || swapchain->handle_ == 0) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  granit_webgpu_provider_swapchain_info provider_info{};
  provider_info.struct_size = sizeof(provider_info);
  const auto result = context_->provider->get_swapchain_info(context_->instance, swapchain->handle_,
                                                             &provider_info);
  if (result != GRANIT_SUCCESS) {
    return result;
  }
  const auto format = to_texture_format(provider_info.format);
  if (format == GRANIT_TEXTURE_FORMAT_UNDEFINED) {
    return GRANIT_ERROR_UNSUPPORTED;
  }
  info = {provider_info.width, provider_info.height, provider_info.image_count,
          provider_info.present_mode, format};
  return GRANIT_SUCCESS;
}

granit_result webgpu_presentation_adapter::acquire_swapchain(
    backend_swapchain_resource& resource, backend_acquired_swapchain_frame& frame) const noexcept {
  auto* swapchain = as_swapchain(resource);
  if (swapchain == nullptr || swapchain->handle_ == 0) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  granit_webgpu_provider_acquired_frame provider_frame{};
  provider_frame.struct_size = sizeof(provider_frame);
  const auto result = context_->provider->acquire_swapchain(context_->instance, swapchain->handle_,
                                                            &provider_frame);
  if (result != GRANIT_SUCCESS) {
    return result;
  }

  backend_swapchain_info info{};
  const auto info_result = get_swapchain_info(resource, info);
  if (info_result != GRANIT_SUCCESS) {
    std::uint32_t ignored{};
    static_cast<void>(
        context_->provider->cancel_swapchain(context_->instance, swapchain->handle_, &ignored));
    return info_result;
  }

  granit_texture_desc texture_desc = GRANIT_TEXTURE_DESC_INIT;
  texture_desc.format = info.format;
  texture_desc.usage = GRANIT_TEXTURE_USAGE_COLOR_ATTACHMENT_BIT;
  texture_desc.memory_location = GRANIT_MEMORY_LOCATION_DEVICE;
  texture_desc.width = info.width;
  texture_desc.height = info.height;

  auto texture = std::unique_ptr<backend_texture_resource>(
      new (std::nothrow) webgpu_borrowed_texture_resource(provider_frame.texture));
  auto view = std::unique_ptr<backend_texture_view_resource>(
      new (std::nothrow) webgpu_borrowed_texture_view_resource(provider_frame.view));
  if (texture == nullptr || view == nullptr) {
    std::uint32_t ignored{};
    static_cast<void>(
        context_->provider->cancel_swapchain(context_->instance, swapchain->handle_, &ignored));
    return GRANIT_ERROR_OUT_OF_MEMORY;
  }

  frame = {};
  frame.image_index = provider_frame.image_index;
  frame.needs_recreate = provider_frame.needs_recreate != 0;
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
  std::uint32_t provider_needs_recreate{};
  const auto result = context_->provider->present_swapchain(context_->instance, swapchain->handle_,
                                                            &provider_needs_recreate);
  needs_recreate = provider_needs_recreate != 0;
  return result;
}

granit_result webgpu_presentation_adapter::cancel_swapchain(backend_swapchain_resource& resource,
                                                            bool& needs_recreate) const noexcept {
  auto* swapchain = as_swapchain(resource);
  if (swapchain == nullptr || swapchain->handle_ == 0) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  std::uint32_t provider_needs_recreate{};
  const auto result = context_->provider->cancel_swapchain(context_->instance, swapchain->handle_,
                                                           &provider_needs_recreate);
  needs_recreate = provider_needs_recreate != 0;
  return result;
}

granit_webgpu_provider_texture_view
webgpu_presentation_adapter::native_view(backend_texture_view_resource& resource) const noexcept {
  const auto* view = dynamic_cast<webgpu_borrowed_texture_view_resource*>(&resource);
  return view == nullptr ? 0 : view->handle_;
}

} // namespace granit::detail
