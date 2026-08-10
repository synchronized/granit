// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_BACKEND_VULKAN_RESULT_H_
#define GRANIT_BACKEND_VULKAN_RESULT_H_

#include <granit/core/result.h>

#include <volk.h>

namespace granit::detail {

/** 将 Vulkan 错误转换为不暴露后端细节的 Granit 结果码。 */
[[nodiscard]] granit_result map_vulkan_result(VkResult result) noexcept;

} // namespace granit::detail

#endif
