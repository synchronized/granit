// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_EXAMPLES_SAMPLES_MODEL_VIEWER_WEB_PIPELINE_WARMUP_H_
#define GRANIT_EXAMPLES_SAMPLES_MODEL_VIEWER_WEB_PIPELINE_WARMUP_H_

#include <granit/renderer/pipeline_warmup.hpp>

#include <cstdint>
#include <vector>

namespace granit::example::model_viewer {
class gpu_scene;
}

namespace granit::example::model_viewer::web {

/** 异步预热当前场景材质所需的 WebGPU Pipeline。 */
class pipeline_warmup final {
public:
  [[nodiscard]] granit::result begin(granit::renderer_ref renderer, gpu_scene& scene,
                                     granit::texture_format color_format,
                                     granit::sample_count samples);
  [[nodiscard]] granit::result poll();
  void reset() noexcept;

  [[nodiscard]] bool started() const noexcept { return phase_ != phase::idle; }

private:
  enum class phase { idle, running, complete };

  granit::pipeline_warmup_batch batch_;
  granit::async_operation operation_;
  std::vector<std::uint32_t> material_indices_;
  phase phase_{phase::idle};
};

} // namespace granit::example::model_viewer::web

#endif // GRANIT_EXAMPLES_SAMPLES_MODEL_VIEWER_WEB_PIPELINE_WARMUP_H_
