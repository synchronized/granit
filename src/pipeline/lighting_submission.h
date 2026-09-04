// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_PIPELINE_LIGHTING_SUBMISSION_H_
#define GRANIT_PIPELINE_LIGHTING_SUBMISSION_H_

#include "lighting/ibl_resources.h"
#include "lighting/light_data.h"
#include "scene/multi_view_submission.h"

#include <granit/pipeline/render_pipeline.h>

namespace granit::pipeline::detail {

/** 当前 View 使用的光源 GPU 数据与 IBL 输入。 */
struct lighting_submission {
  lighting::packed_view_lights lights;
  lighting::light_requirements requirements{};
  lighting::ibl_texture_views ibl_views{};
  lighting::ibl_sampling_constants ibl_constants{.intensity = 0.0F};
};

/** 准备当前 View 的光源与环境输入，失败时不修改 output。 */
[[nodiscard]] granit_result build_lighting_submission(
    const scene::multi_view_snapshot& snapshot, std::uint32_t view_index,
    const lighting::light_limits& limits, lighting::ibl_texture_views default_ibl,
    const granit_render_pipeline_environment* environment, lighting_submission& output) noexcept;

} // namespace granit::pipeline::detail

#endif
