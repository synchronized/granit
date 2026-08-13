// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_PIPELINE_MESH_ACCESS_H_
#define GRANIT_PIPELINE_MESH_ACCESS_H_

#include <granit/pipeline/mesh.h>

namespace granit::pipeline::detail {

[[nodiscard]] granit_result validate_mesh_handle(granit_renderer renderer,
                                                 granit_mesh mesh) noexcept;

/**
 * 将 Mesh 的 Buffer 绑定与一次 Draw 录制到已开始的 Recorder。
 *
 * 调用时会重新校验 Mesh 借用的 Buffer；Pipeline、Viewport、Scissor 和渲染区域
 * 仍由上层阶段负责准备。
 */
[[nodiscard]] granit_result record_mesh_draw(granit_renderer renderer,
                                             granit_command_recorder recorder,
                                             granit_mesh mesh) noexcept;

} // namespace granit::pipeline::detail

#endif
