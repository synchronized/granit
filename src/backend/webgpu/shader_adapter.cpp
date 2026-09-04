// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "backend/webgpu/shader_adapter.h"

#include <utility>

namespace granit::detail {

struct webgpu_shader_context {
  webgpu_provider_dispatch* provider{};
  granit_webgpu_provider_instance instance{};
};

namespace {

class webgpu_shader_resource final : public backend_shader_resource {
public:
  explicit webgpu_shader_resource(std::shared_ptr<webgpu_shader_context> context)
      : context_(std::move(context)) {}

  ~webgpu_shader_resource() override {
    if (handle_ != 0) {
      static_cast<void>(context_->provider->destroy_shader(context_->instance, handle_));
    }
  }

  std::shared_ptr<webgpu_shader_context> context_;
  granit_webgpu_provider_shader handle_{};
};

webgpu_shader_resource* as_shader(backend_shader_resource& resource) {
  return dynamic_cast<webgpu_shader_resource*>(&resource);
}

} // namespace

webgpu_shader_adapter::webgpu_shader_adapter(webgpu_provider_dispatch& provider,
                                             granit_webgpu_provider_instance instance)
    : context_(
          std::make_shared<webgpu_shader_context>(webgpu_shader_context{&provider, instance})) {}

std::unique_ptr<backend_shader_resource> webgpu_shader_adapter::allocate_shader() const {
  return std::make_unique<webgpu_shader_resource>(context_);
}

granit_result
webgpu_shader_adapter::create_shader(backend_shader_resource& resource, std::uint32_t stage,
                                     const char* wgsl, std::uint64_t wgsl_length,
                                     const char* entry_point,
                                     std::uint64_t entry_point_length) const noexcept {
  auto* shader = as_shader(resource);
  if (shader == nullptr || shader->handle_ != 0) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  const granit_webgpu_provider_shader_desc desc{sizeof(desc), stage,       wgsl,
                                                wgsl_length,  entry_point, entry_point_length};
  return context_->provider->create_shader(context_->instance, &desc, &shader->handle_);
}

granit_webgpu_provider_shader
webgpu_shader_adapter::native_handle(backend_shader_resource& resource) const noexcept {
  const auto* shader = as_shader(resource);
  return shader == nullptr ? 0 : shader->handle_;
}

} // namespace granit::detail
