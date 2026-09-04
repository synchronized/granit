// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_BACKEND_INTERFACES_H_
#define GRANIT_BACKEND_INTERFACES_H_

#include <memory>

#include "backend/command.h"
#include "backend/compute.h"
#include "backend/diagnostics.h"
#include "backend/pipeline.h"
#include "backend/presentation.h"
#include "backend/queue.h"
#include "backend/renderer.h"
#include "backend/rendering.h"
#include "backend/resource_management.h"
#include "backend/retirement.h"
#include "backend/shader.h"
#include "backend/timestamp.h"
#include "backend/transfer.h"

namespace granit::detail {

/** Renderer 注册时形成的不可变 HAL 能力快照。 */
struct backend_interfaces {
  std::shared_ptr<backend_renderer> renderer;
  std::shared_ptr<backend_diagnostic_renderer> diagnostics;
  std::shared_ptr<backend_resource_renderer> resources;
  std::shared_ptr<backend_presentation_renderer> presentation;
  std::shared_ptr<backend_queue> queue;
  std::shared_ptr<backend_command_renderer> commands;
  std::shared_ptr<backend_graphics_command_renderer> graphics;
  std::shared_ptr<backend_compute_command_renderer> compute;
  std::shared_ptr<backend_transfer_command_renderer> transfer;
  std::shared_ptr<backend_pipeline_layout_renderer> pipeline_layouts;
  std::shared_ptr<backend_pipeline_renderer> pipelines;
  std::shared_ptr<backend_pipeline_cache_renderer> pipeline_cache;
  std::shared_ptr<backend_shader_renderer> shaders;
  std::shared_ptr<backend_spirv_shader_renderer> spirv_shaders;
  std::shared_ptr<backend_retirement_renderer> retirement;
  std::shared_ptr<backend_timestamp_renderer> timestamps;

  /** 返回两个正式后端都必须实现的最小能力集合是否完整。 */
  [[nodiscard]] bool has_required_capabilities() const noexcept {
    return renderer && resources && presentation && queue && commands && graphics && compute &&
           transfer && pipeline_layouts && pipelines && (shaders || spirv_shaders) && retirement;
  }
};

/** 集中执行一次 RTTI 能力发现，后续调用只读取快照。 */
[[nodiscard]] inline backend_interfaces
discover_backend_interfaces(const std::shared_ptr<backend_renderer>& renderer) {
  backend_interfaces interfaces{};
  interfaces.renderer = renderer;
  interfaces.diagnostics = std::dynamic_pointer_cast<backend_diagnostic_renderer>(renderer);
  interfaces.resources = std::dynamic_pointer_cast<backend_resource_renderer>(renderer);
  interfaces.presentation = std::dynamic_pointer_cast<backend_presentation_renderer>(renderer);
  interfaces.queue = std::dynamic_pointer_cast<backend_queue>(renderer);
  interfaces.commands = std::dynamic_pointer_cast<backend_command_renderer>(renderer);
  interfaces.graphics = std::dynamic_pointer_cast<backend_graphics_command_renderer>(renderer);
  interfaces.compute = std::dynamic_pointer_cast<backend_compute_command_renderer>(renderer);
  interfaces.transfer = std::dynamic_pointer_cast<backend_transfer_command_renderer>(renderer);
  interfaces.pipeline_layouts =
      std::dynamic_pointer_cast<backend_pipeline_layout_renderer>(renderer);
  interfaces.pipelines = std::dynamic_pointer_cast<backend_pipeline_renderer>(renderer);
  interfaces.pipeline_cache = std::dynamic_pointer_cast<backend_pipeline_cache_renderer>(renderer);
  interfaces.shaders = std::dynamic_pointer_cast<backend_shader_renderer>(renderer);
  interfaces.spirv_shaders = std::dynamic_pointer_cast<backend_spirv_shader_renderer>(renderer);
  interfaces.retirement = std::dynamic_pointer_cast<backend_retirement_renderer>(renderer);
  interfaces.timestamps = std::dynamic_pointer_cast<backend_timestamp_renderer>(renderer);
  return interfaces;
}

} // namespace granit::detail

#endif
