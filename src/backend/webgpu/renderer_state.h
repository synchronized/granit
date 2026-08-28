// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_BACKEND_WEBGPU_RENDERER_STATE_H_
#define GRANIT_BACKEND_WEBGPU_RENDERER_STATE_H_

#include <memory>

#include <granit/core/diagnostic.h>

#include "backend/capabilities.h"
#include "backend/lifecycle.h"
#include "backend/plugin_loader.h"
#include "backend/renderer.h"
#include "backend/webgpu/presentation_adapter.h"

namespace granit::detail {

/** 集中管理 WebGPU Provider、异步生命周期、能力快照和呈现适配器。 */
class webgpu_renderer_state final : public backend_renderer {
public:
  webgpu_renderer_state() = default;
  ~webgpu_renderer_state();

  webgpu_renderer_state(const webgpu_renderer_state&) = delete;
  webgpu_renderer_state& operator=(const webgpu_renderer_state&) = delete;

  [[nodiscard]] granit_result initialize_static(const granit_backend_plugin_api* api,
                                                granit_diagnostic_callback diagnostic_callback,
                                                void* diagnostic_user_data) noexcept;
  [[nodiscard]] granit_result process_backend_events() noexcept override;

  [[nodiscard]] backend_lifecycle_status lifecycle_status() const noexcept override;
  [[nodiscard]] const backend_capabilities& capabilities() const noexcept override {
    return capabilities_;
  }
  [[nodiscard]] webgpu_presentation_adapter* presentation() noexcept { return presentation_.get(); }

private:
  static void* allocate(std::uint64_t size, std::uint64_t alignment, void*) noexcept;
  static void deallocate(void* memory, std::uint64_t size, std::uint64_t alignment, void*) noexcept;
  static void diagnose(granit_diagnostic_severity severity, granit_diagnostic_category category,
                       const char* message, std::uint32_t message_length, void* user_data) noexcept;
  [[nodiscard]] granit_result refresh_state() noexcept;

  backend_plugin_loader loader_;
  granit_backend_plugin_instance instance_{};
  granit_diagnostic_callback diagnostic_callback_{};
  void* diagnostic_user_data_{};
  backend_lifecycle_status lifecycle_{};
  backend_capabilities capabilities_{};
  std::unique_ptr<webgpu_presentation_adapter> presentation_;
};

} // namespace granit::detail

#endif
