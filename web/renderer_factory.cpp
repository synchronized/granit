// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "renderer/renderer_factory.h"

#include "backend/plugin_api.h"
#include "renderer/renderer_registry.h"

extern "C" const granit_backend_plugin_api*
granit_backend_plugin_query(uint32_t requested_abi) noexcept;

namespace granit::detail {

granit_result create_default_renderer(const granit_renderer_desc& desc, granit_renderer& renderer) {
  const auto surface_types =
      desc.struct_size >= GRANIT_RENDERER_DESC_VERSION_2_SIZE ? desc.surface_types : UINT32_C(0);
  if ((surface_types & ~GRANIT_SURFACE_TYPE_CANVAS_BIT) != 0) {
    return GRANIT_ERROR_UNSUPPORTED;
  }
  const auto diagnostic_callback =
      desc.struct_size >= GRANIT_RENDERER_DESC_VERSION_4_SIZE ? desc.diagnostic_callback : nullptr;
  auto* diagnostic_user_data =
      desc.struct_size >= GRANIT_RENDERER_DESC_VERSION_4_SIZE ? desc.diagnostic_user_data : nullptr;
  return renderer_registry::instance().create_webgpu_static(
      granit_backend_plugin_query(GRANIT_BACKEND_PLUGIN_ABI_VERSION), diagnostic_callback,
      diagnostic_user_data, renderer);
}

} // namespace granit::detail
