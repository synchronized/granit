// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_PIPELINE_PBR_DRAW_BINDINGS_H_
#define GRANIT_PIPELINE_PBR_DRAW_BINDINGS_H_

#include "material/pbr_draw_inputs.h"
#include "pipeline/material_access.h"

#include <granit/renderer/buffer.hpp>
#include <granit/renderer/pipeline.hpp>

namespace granit::pipeline::detail {

/** 拥有一次 PBR Draw 的 Group 0 Frame 和 Group 2 Object GPU 绑定。 */
class pbr_draw_bindings {
public:
  [[nodiscard]] granit_result initialize(granit_renderer renderer,
                                         const material_draw_state& material,
                                         const granit::material::pbr_frame_constants& frame,
                                         const granit::material::pbr_object_constants& object) noexcept;
  [[nodiscard]] granit_result reset() noexcept;
  [[nodiscard]] bool initialized() const noexcept {
    return frame_group_.valid() && object_group_.valid();
  }
  [[nodiscard]] granit_bind_group frame_group() const noexcept {
    return frame_group_.native_handle();
  }
  [[nodiscard]] granit_bind_group object_group() const noexcept {
    return object_group_.native_handle();
  }

private:
  granit::buffer frame_buffer_;
  granit::buffer object_buffer_;
  granit::bind_group frame_group_;
  granit::bind_group object_group_;
};

} // namespace granit::pipeline::detail

#endif
