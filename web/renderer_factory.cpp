// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "renderer/renderer_factory.h"

#include "backend/webgpu/provider_api.h"
#include "backend/webgpu/renderer_factory.h"

extern "C" const granit_webgpu_provider_api*
granit_webgpu_provider_query(uint32_t requested_abi) noexcept;

namespace granit::detail {

granit_result create_default_renderer(const granit_renderer_desc& desc, granit_renderer& renderer) {
  const auto surface_types = desc.surface_types;
  if ((surface_types & ~GRANIT_SURFACE_TYPE_CANVAS_BIT) != 0) {
    return GRANIT_ERROR_UNSUPPORTED;
  }
  const auto backend = desc.backend;
  if (backend == GRANIT_RENDERER_BACKEND_VULKAN)
    return GRANIT_ERROR_BACKEND_UNAVAILABLE;
  const auto diagnostic_callback = desc.diagnostic_callback;
  auto* diagnostic_user_data = desc.diagnostic_user_data;
  return create_webgpu_renderer_static(
      granit_webgpu_provider_query(GRANIT_WEBGPU_PROVIDER_ABI_VERSION), surface_types,
      diagnostic_callback, diagnostic_user_data, renderer);
}

} // namespace granit::detail
