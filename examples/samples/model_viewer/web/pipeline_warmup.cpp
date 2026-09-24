// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "pipeline_warmup.h"

#include "model_viewer/gpu_scene.h"

#include <granit/core/result.hpp>
#include <granit/renderer/pipeline_warmup.hpp>

#include <cstdio>

namespace granit::example::model_viewer::web {

granit_result pipeline_warmup::begin(granit_renderer renderer, gpu_scene& scene,
                                     granit_texture_format color_format,
                                     granit_sample_count samples) {
  if (renderer == GRANIT_NULL_HANDLE || phase_ != phase::idle)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  renderer_ = renderer;
  const granit_pipeline_warmup_batch_desc desc = GRANIT_PIPELINE_WARMUP_BATCH_DESC_INIT;
  auto result = granit_pipeline_warmup_batch_create(renderer_, &desc, &batch_);
  if (result == GRANIT_SUCCESS) {
    result = granit::to_native(
        scene.add_pipeline_warmups(granit::pipeline_warmup_batch_ref::from_native(batch_),
                                   static_cast<granit::texture_format>(color_format),
                                   static_cast<granit::sample_count>(samples), material_indices_));
  }
  if (result == GRANIT_SUCCESS)
    result = granit_pipeline_warmup_batch_submit_async(renderer_, batch_, &operation_);
  if (result != GRANIT_SUCCESS) {
    reset();
    return result;
  }
  std::printf("GRANIT_PROGRESS:pipelines:0:%zu\n", material_indices_.size());
  phase_ = phase::running;
  return GRANIT_SUCCESS;
}

granit_result pipeline_warmup::poll() {
  if (phase_ == phase::complete)
    return GRANIT_SUCCESS;
  if (phase_ != phase::running)
    return GRANIT_ERROR_INVALID_ARGUMENT;

  granit_async_operation_status status = GRANIT_ASYNC_OPERATION_STATUS_INIT;
  auto result = granit_async_operation_get_status(renderer_, operation_, &status);
  if (result == GRANIT_SUCCESS && status.state == GRANIT_ASYNC_OPERATION_STATE_RUNNING)
    return GRANIT_ERROR_NOT_READY;
  if (result == GRANIT_SUCCESS && status.state == GRANIT_ASYNC_OPERATION_STATE_SUCCEEDED) {
    bool software_adapter_fallback{};
    for (const auto index : material_indices_) {
      granit_pipeline_warmup_result_info info = GRANIT_PIPELINE_WARMUP_RESULT_INFO_INIT;
      if (result == GRANIT_SUCCESS)
        result = granit_pipeline_warmup_operation_get_result(renderer_, operation_, index, &info);
      if (result == GRANIT_SUCCESS && info.result == GRANIT_ERROR_INTERNAL) {
        software_adapter_fallback = true;
      } else if (result == GRANIT_SUCCESS) {
        result = info.result;
      }
    }
    if (software_adapter_fallback) {
      std::fprintf(stderr,
                   "GRANIT_DIAGNOSTIC:软件 WebGPU 适配器异步编译失败，回退到按需同步创建管线\n");
    }
  } else if (result == GRANIT_SUCCESS) {
    result = status.result == GRANIT_ERROR_NOT_READY ? GRANIT_ERROR_NOT_READY : status.result;
  }
  if (result != GRANIT_SUCCESS) {
    reset();
    return result;
  }

  std::printf("GRANIT_PROGRESS:pipelines:%zu:%zu\n", material_indices_.size(),
              material_indices_.size());
  result = granit_async_operation_destroy(renderer_, operation_);
  operation_ = GRANIT_NULL_HANDLE;
  if (result == GRANIT_SUCCESS)
    result = granit_pipeline_warmup_batch_destroy(renderer_, batch_);
  batch_ = GRANIT_NULL_HANDLE;
  if (result != GRANIT_SUCCESS) {
    reset();
    return result;
  }
  material_indices_.clear();
  phase_ = phase::complete;
  return GRANIT_SUCCESS;
}

void pipeline_warmup::reset() noexcept {
  if (renderer_ != GRANIT_NULL_HANDLE) {
    if (operation_ != GRANIT_NULL_HANDLE)
      static_cast<void>(granit_async_operation_destroy(renderer_, operation_));
    if (batch_ != GRANIT_NULL_HANDLE)
      static_cast<void>(granit_pipeline_warmup_batch_destroy(renderer_, batch_));
  }
  renderer_ = GRANIT_NULL_HANDLE;
  batch_ = GRANIT_NULL_HANDLE;
  operation_ = GRANIT_NULL_HANDLE;
  material_indices_.clear();
  phase_ = phase::idle;
}

} // namespace granit::example::model_viewer::web
