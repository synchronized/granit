// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_BACKEND_RENDERER_H_
#define GRANIT_BACKEND_RENDERER_H_

#include <cstdint>
#include <string_view>

#include <granit/core/result.h>
#include <granit/renderer/renderer.h>

#include "backend/contracts/capabilities.h"
#include "backend/contracts/lifecycle.h"

namespace granit::detail {

/** Renderer 根状态的最小后端接口；资源与命令接口按后续子阶段继续分层。 */
class backend_renderer {
public:
  backend_renderer() = default;
  virtual ~backend_renderer() = default;

  backend_renderer(const backend_renderer&) = delete;
  backend_renderer& operator=(const backend_renderer&) = delete;

  [[nodiscard]] virtual backend_lifecycle_status lifecycle_status() const noexcept = 0;
  [[nodiscard]] virtual granit_result process_backend_events() noexcept = 0;
  [[nodiscard]] virtual const backend_capabilities& capabilities() const noexcept = 0;
  [[nodiscard]] virtual granit_renderer_backend backend() const noexcept = 0;
  [[nodiscard]] virtual std::string_view adapter_name() const noexcept = 0;
  [[nodiscard]] virtual std::uint32_t adapter_vendor_id() const noexcept = 0;
  [[nodiscard]] virtual std::uint32_t adapter_device_id() const noexcept = 0;
  [[nodiscard]] virtual std::uint32_t domain() const noexcept = 0;
  virtual void set_domain(std::uint32_t domain) noexcept = 0;
};

} // namespace granit::detail

#endif
