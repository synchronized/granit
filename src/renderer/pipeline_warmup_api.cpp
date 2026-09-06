// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/renderer/pipeline_warmup.h>

#include "renderer/renderer_registry.h"

extern "C" granit_result granit_pipeline_warmup_batch_create(
    granit_renderer renderer, const granit_pipeline_warmup_batch_desc* desc,
    granit_pipeline_warmup_batch* batch) {
  if (renderer == GRANIT_NULL_HANDLE || desc == nullptr || batch == nullptr ||
      desc->struct_size < GRANIT_PIPELINE_WARMUP_BATCH_DESC_VERSION_1_SIZE || desc->flags != 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  *batch = GRANIT_NULL_HANDLE;
  return granit::detail::renderer_registry::instance().create_pipeline_warmup_batch(renderer,
                                                                                     *desc, *batch);
}

extern "C" granit_result granit_pipeline_warmup_batch_add_graphics(
    granit_renderer renderer, granit_pipeline_warmup_batch batch,
    const granit_graphics_pipeline_desc* desc, uint32_t* result_index) {
  if (renderer == GRANIT_NULL_HANDLE || batch == GRANIT_NULL_HANDLE || desc == nullptr ||
      result_index == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  return granit::detail::renderer_registry::instance().add_graphics_pipeline_warmup(
      renderer, batch, *desc, *result_index);
}

extern "C" granit_result granit_pipeline_warmup_batch_add_compute(
    granit_renderer renderer, granit_pipeline_warmup_batch batch,
    const granit_compute_pipeline_desc* desc, uint32_t* result_index) {
  if (renderer == GRANIT_NULL_HANDLE || batch == GRANIT_NULL_HANDLE || desc == nullptr ||
      result_index == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  return granit::detail::renderer_registry::instance().add_compute_pipeline_warmup(
      renderer, batch, *desc, *result_index);
}

extern "C" granit_result granit_pipeline_warmup_batch_get_info(
    granit_renderer renderer, granit_pipeline_warmup_batch batch,
    granit_pipeline_warmup_batch_info* info) {
  if (renderer == GRANIT_NULL_HANDLE || batch == GRANIT_NULL_HANDLE || info == nullptr ||
      info->struct_size < GRANIT_PIPELINE_WARMUP_BATCH_INFO_VERSION_1_SIZE)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  return granit::detail::renderer_registry::instance().get_pipeline_warmup_batch_info(renderer,
                                                                                        batch,
                                                                                        *info);
}

extern "C" granit_result granit_pipeline_warmup_batch_submit_async(
    granit_renderer renderer, granit_pipeline_warmup_batch batch,
    granit_async_operation* operation) {
  if (renderer == GRANIT_NULL_HANDLE || batch == GRANIT_NULL_HANDLE || operation == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  *operation = GRANIT_NULL_HANDLE;
  return granit::detail::renderer_registry::instance().submit_pipeline_warmup_batch_async(
      renderer, batch, *operation);
}

extern "C" granit_result granit_pipeline_warmup_operation_get_result(
    granit_renderer renderer, granit_async_operation operation, uint32_t result_index,
    granit_pipeline_warmup_result_info* info) {
  if (renderer == GRANIT_NULL_HANDLE || operation == GRANIT_NULL_HANDLE || info == nullptr ||
      info->struct_size < GRANIT_PIPELINE_WARMUP_RESULT_INFO_VERSION_1_SIZE)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  return granit::detail::renderer_registry::instance().get_pipeline_warmup_result(
      renderer, operation, result_index, *info);
}

extern "C" granit_result granit_pipeline_warmup_batch_reset(
    granit_renderer renderer, granit_pipeline_warmup_batch batch) {
  if (renderer == GRANIT_NULL_HANDLE || batch == GRANIT_NULL_HANDLE)
    return GRANIT_ERROR_INVALID_HANDLE;
  return granit::detail::renderer_registry::instance().reset_pipeline_warmup_batch(renderer,
                                                                                    batch);
}

extern "C" granit_result granit_pipeline_warmup_batch_destroy(
    granit_renderer renderer, granit_pipeline_warmup_batch batch) {
  if (renderer == GRANIT_NULL_HANDLE || batch == GRANIT_NULL_HANDLE)
    return GRANIT_ERROR_INVALID_HANDLE;
  return granit::detail::renderer_registry::instance().destroy_pipeline_warmup_batch(renderer,
                                                                                      batch);
}
