// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_BACKEND_WEBGPU_PRESENTATION_ADAPTER_H_
#define GRANIT_BACKEND_WEBGPU_PRESENTATION_ADAPTER_H_

#include <memory>

#include <granit/core/result.h>

#include "backend/plugin_loader.h"
#include "backend/presentation.h"

namespace granit::detail {

struct webgpu_presentation_context;

/** 将 WebGPU 插件呈现 ABI 适配为 Renderer 使用的内部资源对象。 */
class webgpu_presentation_adapter {
public:
  webgpu_presentation_adapter(backend_plugin_loader& loader,
                              granit_backend_plugin_instance instance);

  [[nodiscard]] std::unique_ptr<backend_surface_resource> allocate_surface() const;
  [[nodiscard]] std::unique_ptr<backend_swapchain_resource> allocate_swapchain() const;

  [[nodiscard]] granit_result create_canvas_surface(backend_surface_resource& resource,
                                                    const char* selector,
                                                    std::uint32_t selector_length) const noexcept;
  [[nodiscard]] granit_result
  create_swapchain(backend_surface_resource& surface, const backend_swapchain_desc& desc,
                   backend_swapchain_resource& swapchain) const noexcept;
  [[nodiscard]] granit_result recreate_swapchain(backend_swapchain_resource& swapchain,
                                                 const backend_swapchain_desc& desc) const noexcept;
  [[nodiscard]] granit_result get_swapchain_info(backend_swapchain_resource& swapchain,
                                                 backend_swapchain_info& info) const noexcept;
  [[nodiscard]] granit_result
  acquire_swapchain(backend_swapchain_resource& swapchain,
                    backend_acquired_swapchain_frame& frame) const noexcept;
  [[nodiscard]] granit_result present_swapchain(backend_swapchain_resource& swapchain,
                                                bool& needs_recreate) const noexcept;
  [[nodiscard]] granit_result cancel_swapchain(backend_swapchain_resource& swapchain,
                                               bool& needs_recreate) const noexcept;
  [[nodiscard]] granit_backend_plugin_texture_view
  native_view(backend_texture_view_resource& view) const noexcept;

private:
  std::shared_ptr<webgpu_presentation_context> context_;
};

} // namespace granit::detail

#endif
