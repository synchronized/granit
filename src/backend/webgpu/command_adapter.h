// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_BACKEND_WEBGPU_COMMAND_ADAPTER_H_
#define GRANIT_BACKEND_WEBGPU_COMMAND_ADAPTER_H_

#include <memory>

#include "backend/plugin_loader.h"
#include "backend/resources.h"

namespace granit::detail {

struct webgpu_command_context;

/** 适配 WebGPU MVP 的固定三角形命令记录、结束与提交。 */
class webgpu_command_adapter {
public:
  webgpu_command_adapter(backend_plugin_loader& loader, granit_backend_plugin_instance instance);

  [[nodiscard]] std::unique_ptr<backend_command_recorder_resource> allocate_recorder() const;
  [[nodiscard]] granit_result begin(backend_command_recorder_resource& resource) const noexcept;
  [[nodiscard]] granit_result draw(backend_command_recorder_resource& resource,
                                   granit_backend_plugin_texture_view target,
                                   granit_backend_plugin_render_pipeline pipeline) const noexcept;
  [[nodiscard]] granit_result end(backend_command_recorder_resource& resource) const noexcept;
  [[nodiscard]] granit_result submit(backend_command_recorder_resource& resource) const noexcept;
  [[nodiscard]] granit_result reset(backend_command_recorder_resource& resource) const noexcept;

private:
  std::shared_ptr<webgpu_command_context> context_;
};

} // namespace granit::detail

#endif
