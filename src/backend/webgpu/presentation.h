// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_BACKEND_WEBGPU_PRESENTATION_H_
#define GRANIT_BACKEND_WEBGPU_PRESENTATION_H_

#include "backend/contracts/presentation.h"
#include "backend/webgpu/context.h"

namespace granit::detail {

/** 供 WebGPU Surface 和 Swapchain 资源析构共享的设备所有者引用。 */
struct webgpu_presentation_owner {
  webgpu_context* context{};
  webgpu_instance_handle instance{};
};

} // namespace granit::detail

#endif
