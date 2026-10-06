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

/**
 * 线程安全地初始化进程内 Vulkan Loader，并检查 Vulkan 1.3 支持。
 *
 * 默认按 system -> bundled 顺序尝试。可通过 GRANIT_VULKAN_RUNTIME 选择 system、bundled、
 * auto 或 none，并通过 GRANIT_VULKAN_LOADER_PATH 指定开发/诊断用 Loader 路径。
 */
[[nodiscard]] vulkan_loader_status initialize_vulkan_loader() noexcept;

} // namespace granit::detail

#endif
