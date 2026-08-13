// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_PIPELINE_MESH_ACCESS_H_
#define GRANIT_PIPELINE_MESH_ACCESS_H_

#include <granit/pipeline/mesh.h>

#include <vector>

namespace granit::pipeline::detail {

struct mesh_vertex_layout {
  uint32_t stride = 0;
  granit_vertex_step_mode step_mode = GRANIT_VERTEX_STEP_MODE_VERTEX;
  std::vector<granit_vertex_attribute> attributes;
};

struct mesh_pipeline_state {
  granit_primitive_topology topology = GRANIT_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  std::vector<mesh_vertex_layout> vertex_buffers;
};

[[nodiscard]] granit_result validate_mesh_handle(granit_renderer renderer,
                                                 granit_mesh mesh) noexcept;
[[nodiscard]] granit_result copy_mesh_pipeline_state(granit_renderer renderer, granit_mesh mesh,
                                                     mesh_pipeline_state& state) noexcept;

/** 在进入渲染区域前校验并绑定 Mesh 借用的 Buffer。 */
[[nodiscard]] granit_result bind_mesh_buffers(granit_renderer renderer,
                                              granit_command_recorder recorder,
                                              granit_mesh mesh) noexcept;

/** 在已开始的渲染区域内录制 Mesh 的一次 Draw。 */
[[nodiscard]] granit_result draw_mesh(granit_renderer renderer, granit_command_recorder recorder,
                                      granit_mesh mesh) noexcept;

} // namespace granit::pipeline::detail

#endif
