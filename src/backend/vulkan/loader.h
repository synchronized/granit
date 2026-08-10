// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_BACKEND_VULKAN_LOADER_H_
#define GRANIT_BACKEND_VULKAN_LOADER_H_

#include <cstdint>

#include <granit/core/result.h>

namespace granit::detail {

struct vulkan_loader_status {
  granit_result result;
  std::uint32_t api_version;
};

/** 线程安全地初始化进程内 Vulkan loader，并检查 Vulkan 1.3 支持。 */
[[nodiscard]] vulkan_loader_status initialize_vulkan_loader() noexcept;

} // namespace granit::detail

#endif
