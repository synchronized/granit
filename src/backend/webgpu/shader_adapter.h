// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_BACKEND_WEBGPU_SHADER_ADAPTER_H_
#define GRANIT_BACKEND_WEBGPU_SHADER_ADAPTER_H_

#include <memory>

#include "backend/contracts/resources.h"
#include "backend/webgpu/provider_dispatch.h"

namespace granit::detail {

struct webgpu_shader_context;

/** 将 WebGPU Provider Shader 句柄适配为内部后端资源对象。 */
class webgpu_shader_adapter {
public:
  webgpu_shader_adapter(webgpu_provider_dispatch& provider,
                        granit_webgpu_provider_instance instance);

  [[nodiscard]] std::unique_ptr<backend_shader_resource> allocate_shader() const;
  [[nodiscard]] granit_result create_shader(backend_shader_resource& resource, std::uint32_t stage,
                                            const char* wgsl, std::uint64_t wgsl_length,
                                            const char* entry_point,
                                            std::uint64_t entry_point_length) const noexcept;
  [[nodiscard]] granit_webgpu_provider_shader
  native_handle(backend_shader_resource& resource) const noexcept;

private:
  std::shared_ptr<webgpu_shader_context> context_;
};

} // namespace granit::detail

#endif
