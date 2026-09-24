// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "model_viewer/pipeline_prepare.h"

#include "model_viewer/gpu_scene.h"

#include <cstdio>

namespace granit::example::model_viewer {

granit::result pipeline_prepare::begin(granit::renderer_ref renderer, gpu_scene& scene,
                                       granit::texture_format color_format,
                                       granit::sample_count samples) {
  if (!renderer || phase_ != phase::idle)
    return granit::result::invalid_argument;
  auto result = batch_.create(renderer);
  if (result.ok())
    result = scene.add_pipeline_warmups(batch_.ref(), color_format, samples, material_indices_);
  if (result.ok())
    result = batch_.submit_async(operation_);
  if (result.failed()) {
    reset();
    return result;
  }
  std::printf("GRANIT_PROGRESS:pipelines:0:%zu\n", material_indices_.size());
  phase_ = phase::running;
  return granit::result::success;
}

granit::result pipeline_prepare::poll() {
  if (phase_ == phase::complete)
    return granit::result::success;
  if (phase_ != phase::running)
    return granit::result::invalid_argument;

  granit::async_operation_status status;
  auto result = operation_.get_status(status);
  if (result.ok() && status.state == granit::async_operation_state::running)
    return granit::result::not_ready;
  if (result.ok() && status.state == granit::async_operation_state::succeeded) {
    bool software_adapter_fallback{};
    for (const auto index : material_indices_) {
      granit::pipeline_warmup_result_info info;
      if (result.ok())
        result = granit::get_pipeline_warmup_result(operation_, index, info);
      if (result.ok() && info.operation_result == granit::result::internal) {
        software_adapter_fallback = true;
      } else if (result.ok()) {
        result = info.operation_result;
      }
    }
    if (software_adapter_fallback) {
      std::fprintf(stderr,
                   "GRANIT_DIAGNOSTIC:软件 WebGPU 适配器异步编译失败，回退到按需同步创建管线\n");
    }
  } else if (result.ok()) {
    result = status.operation_result == granit::result::not_ready ? granit::result::not_ready
                                                                  : status.operation_result;
  }
  if (result.failed()) {
    reset();
    return result;
  }

  std::printf("GRANIT_PROGRESS:pipelines:%zu:%zu\n", material_indices_.size(),
              material_indices_.size());
  result = operation_.reset();
  if (result.ok())
    result = batch_.reset_handle();
  if (result.failed()) {
    reset();
    return result;
  }
  material_indices_.clear();
  phase_ = phase::complete;
  return granit::result::success;
}

void pipeline_prepare::reset() noexcept {
  static_cast<void>(operation_.reset());
  static_cast<void>(batch_.reset_handle());
  material_indices_.clear();
  phase_ = phase::idle;
}

} // namespace granit::example::model_viewer
