// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_BACKEND_COMPUTE_H_
#define GRANIT_BACKEND_COMPUTE_H_

#include <cstdint>
#include <span>

#include <granit/core/result.h>

#include "backend/contracts/access.h"
#include "backend/contracts/resources.h"

namespace granit::detail {

/** 统一命令记录器中的计算调度能力。 */
class backend_compute_command_renderer {
public:
  backend_compute_command_renderer() = default;
  virtual ~backend_compute_command_renderer() = default;
  backend_compute_command_renderer(const backend_compute_command_renderer&) = delete;
  backend_compute_command_renderer& operator=(const backend_compute_command_renderer&) = delete;

  [[nodiscard]] virtual granit_result
  bind_compute_pipeline(backend_command_recorder_resource& recorder,
                        backend_compute_pipeline_resource& pipeline) noexcept = 0;
  [[nodiscard]] virtual granit_result
  bind_compute_groups(backend_command_recorder_resource& recorder,
                      backend_pipeline_layout_resource& layout, std::uint32_t first_group,
                      std::span<backend_bind_group_resource* const> bind_groups,
                      std::span<const std::uint32_t> dynamic_offsets,
                      std::span<const backend_buffer_access> buffer_accesses,
                      std::span<const backend_texture_access> texture_accesses) = 0;
  [[nodiscard]] virtual granit_result dispatch(backend_command_recorder_resource& recorder,
                                               std::uint32_t group_count_x,
                                               std::uint32_t group_count_y,
                                               std::uint32_t group_count_z) noexcept = 0;
};

} // namespace granit::detail

#endif
