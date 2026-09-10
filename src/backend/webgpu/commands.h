// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_BACKEND_WEBGPU_COMMANDS_H_
#define GRANIT_BACKEND_WEBGPU_COMMANDS_H_

#include "backend/webgpu/device.h"

namespace granit::detail {

/** 供 WebGPU 命令记录器析构共享的设备所有者引用。 */
struct webgpu_command_owner {
  webgpu_device* context{};
  webgpu_instance_handle instance{};
};

} // namespace granit::detail

#endif
