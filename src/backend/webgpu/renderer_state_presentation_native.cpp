// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "backend/webgpu/renderer_state.h"

#include <new>
#include <utility>

namespace granit::detail {
namespace {

webgpu_swapchain_desc to_presentation_owner_desc(const backend_swapchain_desc& desc) {
  return {sizeof(webgpu_swapchain_desc), desc.width, desc.height, desc.minimum_image_count,
          desc.present_mode};
}

granit_texture_format to_texture_format(webgpu_texture_format format) {
  switch (format) {
  case GRANIT_WEBGPU_TEXTURE_FORMAT_RGBA8_UNORM:
    return GRANIT_TEXTURE_FORMAT_RGBA8_UNORM;
  case GRANIT_WEBGPU_TEXTURE_FORMAT_BGRA8_UNORM:
    return GRANIT_TEXTURE_FORMAT_BGRA8_UNORM;
  default:
    return GRANIT_TEXTURE_FORMAT_UNDEFINED;
  }
}

} // namespace

namespace {

class webgpu_surface_resource final : public backend_surface_resource {
public:
  explicit webgpu_surface_resource(std::shared_ptr<webgpu_presentation_owner> context)
      : presentation_owner_(std::move(context)) {}

  ~webgpu_surface_resource() override {
    if (handle_ != 0) {
      static_cast<void>(presentation_owner_->device->destroy_surface(handle_));
    }
  }

  std::shared_ptr<webgpu_presentation_owner> presentation_owner_;
  webgpu_surface handle_{};
};

class webgpu_swapchain_resource final : public backend_swapchain_resource {
public:
  explicit webgpu_swapchain_resource(std::shared_ptr<webgpu_presentation_owner> context)
      : presentation_owner_(std::move(context)) {}

  ~webgpu_swapchain_resource() override {
    if (handle_ != 0) {
      static_cast<void>(presentation_owner_->device->destroy_swapchain(handle_));
    }
  }

  std::shared_ptr<webgpu_presentation_owner> presentation_owner_;
  webgpu_swapchain handle_{};
};

/** 借用资源由 Swapchain 在 Present、Cancel 或重建时统一失效。 */
class webgpu_borrowed_texture_resource final : public backend_texture_resource {
public:
  explicit webgpu_borrowed_texture_resource(webgpu_texture handle) : handle_(handle) {}

  webgpu_texture handle_{};
};

class webgpu_borrowed_texture_view_resource final : public backend_texture_view_resource {
public:
  explicit webgpu_borrowed_texture_view_resource(webgpu_texture_view handle) : handle_(handle) {}

  webgpu_texture_view handle_{};
};

webgpu_surface_resource* as_surface(backend_surface_resource& resource) {
  return dynamic_cast<webgpu_surface_resource*>(&resource);
}

webgpu_swapchain_resource* as_swapchain(backend_swapchain_resource& resource) {
  return dynamic_cast<webgpu_swapchain_resource*>(&resource);
}

} // namespace

std::unique_ptr<backend_surface_resource> webgpu_renderer_state::presentation_allocate_surface() {
  return std::make_unique<webgpu_surface_resource>(presentation_owner_);
}

std::unique_ptr<backend_swapchain_resource>
webgpu_renderer_state::presentation_allocate_swapchain() {
  return std::make_unique<webgpu_swapchain_resource>(presentation_owner_);
}

granit_result
webgpu_renderer_state::presentation_create_win32_surface(backend_surface_resource& resource,
                                                         void* instance, void* window) noexcept {
  auto* surface = as_surface(resource);
  if (surface == nullptr || surface->handle_ != 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  webgpu_win32_surface_desc desc{sizeof(desc), 0, instance, window};
  return presentation_owner_->device->create_win32_surface(&desc, &surface->handle_);
}

granit_result webgpu_renderer_state::presentation_create_xcb_surface(
    backend_surface_resource& resource, void* connection, std::uint32_t window) noexcept {
  auto* surface = as_surface(resource);
  if (surface == nullptr || surface->handle_ != 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  webgpu_xcb_surface_desc desc{sizeof(desc), 0, connection, window, 0};
  return presentation_owner_->device->create_xcb_surface(&desc, &surface->handle_);
}

granit_result webgpu_renderer_state::presentation_create_wayland_surface(
    backend_surface_resource& resource, void* display, void* native_surface) noexcept {
  auto* surface = as_surface(resource);
  if (surface == nullptr || surface->handle_ != 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  webgpu_wayland_surface_desc desc{sizeof(desc), 0, display, native_surface};
  return presentation_owner_->device->create_wayland_surface(&desc, &surface->handle_);
}

granit_result
webgpu_renderer_state::presentation_create_canvas_surface(backend_surface_resource& resource,
                                                          const char* selector,
                                                          std::uint32_t selector_length) noexcept {
  auto* surface = as_surface(resource);
  if (surface == nullptr || surface->handle_ != 0) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  webgpu_canvas_surface_desc desc{sizeof(desc), 0, selector, selector_length};
  return presentation_owner_->device->create_canvas_surface(&desc, &surface->handle_);
}

granit_result webgpu_renderer_state::presentation_create_swapchain(
    backend_surface_resource& surface_resource, const backend_swapchain_desc& desc,
    backend_swapchain_resource& swapchain_resource) noexcept {
  auto* surface = as_surface(surface_resource);
  auto* swapchain = as_swapchain(swapchain_resource);
  if (surface == nullptr || surface->handle_ == 0 || swapchain == nullptr ||
      swapchain->handle_ != 0) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  const auto presentation_owner_desc = to_presentation_owner_desc(desc);
  return presentation_owner_->device->create_swapchain(surface->handle_, &presentation_owner_desc,
                                                       &swapchain->handle_);
}

granit_result webgpu_renderer_state::presentation_recreate_swapchain(
    backend_swapchain_resource& resource, const backend_swapchain_desc& desc) noexcept {
  auto* swapchain = as_swapchain(resource);
  if (swapchain == nullptr || swapchain->handle_ == 0) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  const auto presentation_owner_desc = to_presentation_owner_desc(desc);
  return presentation_owner_->device->recreate_swapchain(swapchain->handle_,
                                                         &presentation_owner_desc);
}

granit_result
webgpu_renderer_state::presentation_get_swapchain_info(backend_swapchain_resource& resource,
                                                       backend_swapchain_info& info) noexcept {
  auto* swapchain = as_swapchain(resource);
  if (swapchain == nullptr || swapchain->handle_ == 0) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  webgpu_swapchain_info presentation_owner_info{};
  presentation_owner_info.struct_size = sizeof(presentation_owner_info);
  const auto result =
      presentation_owner_->device->get_swapchain_info(swapchain->handle_, &presentation_owner_info);
  if (result != GRANIT_SUCCESS) {
    return result;
  }
  const auto format = to_texture_format(presentation_owner_info.format);
  if (format == GRANIT_TEXTURE_FORMAT_UNDEFINED) {
    return GRANIT_ERROR_UNSUPPORTED;
  }
  info = {presentation_owner_info.width, presentation_owner_info.height,
          presentation_owner_info.image_count, presentation_owner_info.present_mode, format};
  return GRANIT_SUCCESS;
}

granit_result webgpu_renderer_state::presentation_acquire_swapchain(
    backend_swapchain_resource& resource, backend_acquired_swapchain_frame& frame) noexcept {
  auto* swapchain = as_swapchain(resource);
  if (swapchain == nullptr || swapchain->handle_ == 0) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  webgpu_acquired_frame presentation_owner_frame{};
  presentation_owner_frame.struct_size = sizeof(presentation_owner_frame);
  const auto result =
      presentation_owner_->device->acquire_swapchain(swapchain->handle_, &presentation_owner_frame);
  if (result != GRANIT_SUCCESS) {
    return result;
  }

  backend_swapchain_info info{};
  const auto info_result = presentation_get_swapchain_info(resource, info);
  if (info_result != GRANIT_SUCCESS) {
    std::uint32_t ignored{};
    static_cast<void>(presentation_owner_->device->cancel_swapchain(swapchain->handle_, &ignored));
    return info_result;
  }

  granit_texture_desc texture_desc = GRANIT_TEXTURE_DESC_INIT;
  texture_desc.format = info.format;
  texture_desc.usage = GRANIT_TEXTURE_USAGE_COLOR_ATTACHMENT_BIT;
  texture_desc.memory_location = GRANIT_MEMORY_LOCATION_DEVICE;
  texture_desc.width = info.width;
  texture_desc.height = info.height;

  auto texture = std::unique_ptr<backend_texture_resource>(
      new (std::nothrow) webgpu_borrowed_texture_resource(presentation_owner_frame.texture));
  auto view = std::unique_ptr<backend_texture_view_resource>(
      new (std::nothrow) webgpu_borrowed_texture_view_resource(presentation_owner_frame.view));
  if (texture == nullptr || view == nullptr) {
    std::uint32_t ignored{};
    static_cast<void>(presentation_owner_->device->cancel_swapchain(swapchain->handle_, &ignored));
    return GRANIT_ERROR_OUT_OF_MEMORY;
  }

  frame = {};
  frame.image_index = presentation_owner_frame.image_index;
  frame.needs_recreate = presentation_owner_frame.needs_recreate != 0;
  frame.dynamic_backbuffer.texture = std::move(texture);
  frame.dynamic_backbuffer.view = std::move(view);
  frame.dynamic_backbuffer.desc = texture_desc;
  return GRANIT_SUCCESS;
}

granit_result
webgpu_renderer_state::presentation_present_swapchain(backend_swapchain_resource& resource,
                                                      bool& needs_recreate) noexcept {
  auto* swapchain = as_swapchain(resource);
  if (swapchain == nullptr || swapchain->handle_ == 0) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  std::uint32_t presentation_owner_needs_recreate{};
  const auto result = presentation_owner_->device->present_swapchain(
      swapchain->handle_, &presentation_owner_needs_recreate);
  needs_recreate = presentation_owner_needs_recreate != 0;
  return result;
}

granit_result
webgpu_renderer_state::presentation_cancel_swapchain(backend_swapchain_resource& resource,
                                                     bool& needs_recreate) noexcept {
  auto* swapchain = as_swapchain(resource);
  if (swapchain == nullptr || swapchain->handle_ == 0) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  std::uint32_t presentation_owner_needs_recreate{};
  const auto result = presentation_owner_->device->cancel_swapchain(
      swapchain->handle_, &presentation_owner_needs_recreate);
  needs_recreate = presentation_owner_needs_recreate != 0;
  return result;
}

webgpu_texture_view
webgpu_renderer_state::presentation_native_view(backend_texture_view_resource& resource) noexcept {
  const auto* view = dynamic_cast<webgpu_borrowed_texture_view_resource*>(&resource);
  return view == nullptr ? 0 : view->handle_;
}

} // namespace granit::detail
