// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "backend/webgpu/renderer_state.h"

#include <utility>

namespace granit::detail {
namespace {

class webgpu_shader_resource final : public backend_shader_resource {
public:
  explicit webgpu_shader_resource(std::shared_ptr<webgpu_renderer_state> renderer) noexcept
      : renderer_(std::move(renderer)) {}

  ~webgpu_shader_resource() override {
    if (handle_ != 0)
      static_cast<void>(renderer_->native_device().destroy_shader(handle_));
  }

  std::shared_ptr<webgpu_renderer_state> renderer_;
  webgpu_shader handle_{};
};

webgpu_shader_resource* as_shader(backend_shader_resource& resource) noexcept {
  return dynamic_cast<webgpu_shader_resource*>(&resource);
}

} // namespace

std::unique_ptr<backend_shader_resource> webgpu_renderer_state::allocate_shader_resource() {
  if (lifecycle_.state != backend_lifecycle_state::ready)
    return nullptr;
  return std::make_unique<webgpu_shader_resource>(shared_from_this());
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
  return device_.create_shader(&desc, &resource->handle_);
}

webgpu_shader
webgpu_renderer_state::native_shader(backend_shader_resource& resource) const noexcept {
  const auto* shader = as_shader(resource);
  return shader == nullptr ? webgpu_shader{} : shader->handle_;
}

} // namespace granit::detail
