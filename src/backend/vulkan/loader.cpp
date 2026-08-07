// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "backend/vulkan/loader.h"

#include <volk.h>

namespace granit::detail {
namespace {

vulkan_loader_status create_loader_status() noexcept {
  vulkan_loader_status status{GRANIT_ERROR_BACKEND_UNAVAILABLE, 0};
  if (volk::volkInitialize() != VK_SUCCESS) {
    return status;
  }

  status.api_version = volk::volkGetInstanceVersion();
  if (status.api_version < VK_API_VERSION_1_3) {
    status.result = GRANIT_ERROR_INCOMPATIBLE_DRIVER;
    return status;
  }
  status.result = GRANIT_SUCCESS;
  return status;
}

} // namespace

vulkan_loader_status initialize_vulkan_loader() noexcept {
  static const vulkan_loader_status status = create_loader_status();
  return status;
}

} // namespace granit::detail
