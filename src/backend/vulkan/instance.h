// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_BACKEND_VULKAN_INSTANCE_H_
#define GRANIT_BACKEND_VULKAN_INSTANCE_H_

#include <cstdint>
#include <string_view>

#include <granit/core/result.h>

#include <volk.h>

namespace granit::detail {

class diagnostic_sink;

struct vulkan_instance_desc {
  std::string_view application_name;
  bool enable_validation{};
  std::uint32_t surface_types{};
  const diagnostic_sink* diagnostics{};
};

/** 拥有一个无窗口 Vulkan instance 及其独立函数表。 */
class vulkan_instance {
public:
  vulkan_instance() = default;
  ~vulkan_instance();

  vulkan_instance(const vulkan_instance&) = delete;
  vulkan_instance& operator=(const vulkan_instance&) = delete;
  vulkan_instance(vulkan_instance&& other) noexcept;
  vulkan_instance& operator=(vulkan_instance&& other) noexcept;

  [[nodiscard]] granit_result initialize(const vulkan_instance_desc& desc);
  void reset() noexcept;

  [[nodiscard]] bool valid() const noexcept { return instance_ != VK_NULL_HANDLE; }
  [[nodiscard]] VkInstance native_handle() const noexcept { return instance_; }
  [[nodiscard]] const volk::VolkInstanceTable& functions() const noexcept { return functions_; }

private:
  VkInstance instance_{VK_NULL_HANDLE};
  VkDebugUtilsMessengerEXT debug_messenger_{VK_NULL_HANDLE};
  volk::VolkInstanceTable functions_{};
};

} // namespace granit::detail

#endif
