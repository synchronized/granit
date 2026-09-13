// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "shader_cli/commands.h"
#include <granit/tools/shader_tools.hpp>

#include <iostream>

namespace granit::shader_cli {
namespace {
const char* object_backend_name(uint32_t backend) {
  return backend == GRANIT_SHADER_BACKEND_VULKAN_BIT ? "vulkan" : "webgpu";
}

} // namespace

int print_target_capabilities(granit::shader_backend backend) {
  const auto [status, capabilities] = granit::shader_tools::target_capabilities(backend);
  if (status.failed()) {
    std::cerr << "不支持请求的 Shader 目标档位\n";
    return 1;
  }
  std::cout << "target=" << object_backend_name(capabilities.backend) << "-portable\n"
            << "backend=" << object_backend_name(capabilities.backend) << '\n'
            << "profile=portable\n"
            << "features=none\n";
  return 0;
}

} // namespace granit::shader_cli
