// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_EXAMPLES_SAMPLES_MODEL_VIEWER_WEB_PIPELINE_VALIDATION_H_
#define GRANIT_EXAMPLES_SAMPLES_MODEL_VIEWER_WEB_PIPELINE_VALIDATION_H_

#include <granit/renderer/pipeline.h>
#include <granit/renderer/pipeline_warmup.h>
#include <granit/renderer/renderer.h>
#include <granit/renderer/shader.h>

#include <cstdint>
namespace granit::example::model_viewer::web {

/** 验收 Web 后端异步 Pipeline 预热和公共 C API 资源生命周期。 */
class pipeline_validation final {
public:
  pipeline_validation() = default;
  ~pipeline_validation() = default;
  pipeline_validation(const pipeline_validation&) = delete;
  pipeline_validation& operator=(const pipeline_validation&) = delete;

  [[nodiscard]] granit_result begin(granit_renderer renderer);
  [[nodiscard]] granit_result poll();

  /** 必须在所属 Renderer 销毁前调用；允许对空闲或已完成对象重复调用。 */
  void reset() noexcept;

  [[nodiscard]] bool started() const noexcept { return phase_ != phase::idle; }

private:
  enum class phase { idle, running, complete };

  granit_renderer renderer_{};
  granit_shader vertex_{};
  granit_shader fragment_{};
  granit_shader compute_{};
  granit_pipeline_layout layout_{};
  granit_pipeline_warmup_batch batch_{};
  granit_async_operation operation_{};
  std::uint32_t graphics_index_{};
  std::uint32_t compute_index_{};
  phase phase_{phase::idle};
};

} // namespace granit::example::model_viewer::web

#endif // GRANIT_EXAMPLES_SAMPLES_MODEL_VIEWER_WEB_PIPELINE_VALIDATION_H_
