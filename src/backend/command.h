// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_BACKEND_COMMAND_H_
#define GRANIT_BACKEND_COMMAND_H_

#include <cstdint>
#include <memory>

#include <granit/core/result.h>

#include "backend/resources.h"

namespace granit::detail {

/** 统一命令记录器的分配、生命周期与状态查询。 */
class backend_command_renderer {
public:
  backend_command_renderer() = default;
  virtual ~backend_command_renderer() = default;
  backend_command_renderer(const backend_command_renderer&) = delete;
  backend_command_renderer& operator=(const backend_command_renderer&) = delete;

  [[nodiscard]] virtual std::unique_ptr<backend_command_recorder_resource>
  allocate_command_recorder_resource() = 0;
  [[nodiscard]] virtual granit_result
  create_command_recorder(backend_command_recorder_resource& recorder) noexcept = 0;
  [[nodiscard]] virtual granit_result
  begin_command_recorder(backend_command_recorder_resource& recorder) noexcept = 0;
  [[nodiscard]] virtual granit_result
  end_command_recorder(backend_command_recorder_resource& recorder) noexcept = 0;
  [[nodiscard]] virtual granit_result
  reset_command_recorder(backend_command_recorder_resource& recorder) noexcept = 0;
  /** 丢弃正在记录或待提交的原生状态，使资源可重新创建。 */
  [[nodiscard]] virtual granit_result
  discard_command_recorder(backend_command_recorder_resource& recorder) noexcept = 0;
  [[nodiscard]] virtual bool
  command_recorder_is_recording(backend_command_recorder_resource& recorder) noexcept = 0;
  [[nodiscard]] virtual granit_result draw(backend_command_recorder_resource& recorder,
                                           backend_texture_view_resource* target,
                                           backend_graphics_pipeline_resource* pipeline,
                                           std::uint32_t vertex_count, std::uint32_t instance_count,
                                           std::uint32_t first_vertex,
                                           std::uint32_t first_instance) noexcept = 0;
};

} // namespace granit::detail

#endif
