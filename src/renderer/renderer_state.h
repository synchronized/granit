// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_RENDERER_RENDERER_STATE_H_
#define GRANIT_RENDERER_RENDERER_STATE_H_

#include <cstdint>
#include <string_view>

#include <granit/result.h>

#include "backend/vulkan/device.h"
#include "backend/vulkan/instance.h"

namespace granit::detail {

class renderer_state {
public:
  renderer_state() = default;

  renderer_state(const renderer_state&) = delete;
  renderer_state& operator=(const renderer_state&) = delete;
  renderer_state(renderer_state&&) = delete;
  renderer_state& operator=(renderer_state&&) = delete;

  [[nodiscard]] granit_result initialize(std::string_view application_name, bool enable_validation);

  void set_domain(std::uint32_t domain) noexcept { domain_ = domain; }
  [[nodiscard]] std::uint32_t domain() const noexcept { return domain_; }
  [[nodiscard]] const vulkan_instance& instance() const noexcept { return instance_; }
  [[nodiscard]] const vulkan_device& device() const noexcept { return device_; }

private:
  std::uint32_t domain_{};
  vulkan_instance instance_;
  vulkan_device device_;
};

} // namespace granit::detail

#endif
