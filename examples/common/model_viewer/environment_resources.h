// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_EXAMPLES_COMMON_MODEL_VIEWER_ENVIRONMENT_RESOURCES_H_
#define GRANIT_EXAMPLES_COMMON_MODEL_VIEWER_ENVIRONMENT_RESOURCES_H_

#include "model_viewer/environment_package.h"

#include <granit/pipeline/render_pipeline.h>
#include <granit/renderer/texture.hpp>

namespace granit::example::model_viewer {

/** 拥有由 GRENV 包上传的环境纹理，并提供 Render Pipeline 借用描述。 */
class environment_resources {
public:
  [[nodiscard]] granit::result initialize(granit_renderer renderer,
                                          const environment_package& package) noexcept;
  /** 创建无需外部资产的低分辨率中性摄影棚环境。 */
  [[nodiscard]] granit::result initialize_builtin_studio(granit_renderer renderer) noexcept;
  void reset() noexcept;

  [[nodiscard]] bool valid() const noexcept {
    return environment_.irradiance != GRANIT_NULL_HANDLE;
  }
  [[nodiscard]] const granit_render_pipeline_environment& environment() const noexcept {
    return environment_;
  }
  [[nodiscard]] granit_render_pipeline_environment& environment() noexcept { return environment_; }

private:
  granit::texture irradiance_texture_;
  granit::texture_view irradiance_view_;
  granit::texture prefiltered_texture_;
  granit::texture_view prefiltered_view_;
  granit::texture brdf_texture_;
  granit::texture_view brdf_view_;
  granit_render_pipeline_environment environment_ = GRANIT_RENDER_PIPELINE_ENVIRONMENT_INIT;
};

} // namespace granit::example::model_viewer

#endif
