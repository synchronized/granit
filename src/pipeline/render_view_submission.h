// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_PIPELINE_RENDER_VIEW_SUBMISSION_H_
#define GRANIT_PIPELINE_RENDER_VIEW_SUBMISSION_H_

#include "material/pbr_render_graph_adapter.h"
#include "scene/multi_view_submission.h"

#include <granit/pipeline/render_pipeline.h>

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace granit::pipeline::detail {

/** 当前 View 的可见对象及其公共回调表示；只保存一次渲染调用所需的数据。 */
struct render_view_submission {
  granit_scene_view view{};
  std::vector<std::uint64_t> payloads;
  std::vector<granit_render_pipeline_draw_binding> draw_bindings;
  std::vector<granit_scene_renderable> renderables;
  std::vector<material::pbr_object_input> pbr_objects;
};

/** 将内部可见性结果与 payload 绑定转换为一次 View 提交，失败时不修改 output。 */
[[nodiscard]] granit_result build_render_view_submission(
    const scene::multi_view_snapshot& snapshot, std::uint32_t view_index,
    const std::unordered_map<std::uint64_t, granit_render_pipeline_draw_binding>& bindings,
    render_view_submission& output) noexcept;

} // namespace granit::pipeline::detail

#endif
