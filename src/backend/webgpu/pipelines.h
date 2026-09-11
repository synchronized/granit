// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_BACKEND_WEBGPU_PIPELINES_H_
#define GRANIT_BACKEND_WEBGPU_PIPELINES_H_

#include "backend/webgpu/device.h"

namespace granit::detail {

/** 供 WebGPU Pipeline 资源析构共享的设备所有者引用。 */
struct webgpu_pipeline_owner {
  webgpu_device* device{};
};

} // namespace granit::detail

#endif
