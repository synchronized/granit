// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/renderer/readback_batch.h>

#include "renderer/renderer_registry.h"

extern "C" granit_result granit_readback_batch_create(granit_renderer renderer,
                                                       const granit_readback_batch_desc* desc,
                                                       granit_readback_batch* batch) {
  if (renderer == GRANIT_NULL_HANDLE || desc == nullptr || batch == nullptr ||
      desc->struct_size < GRANIT_READBACK_BATCH_DESC_VERSION_1_SIZE || desc->flags != 0 ||
      (desc->texture_layout != GRANIT_READBACK_LAYOUT_TIGHT &&
       desc->texture_layout != GRANIT_READBACK_LAYOUT_BACKEND))
    return GRANIT_ERROR_INVALID_ARGUMENT;
  *batch = GRANIT_NULL_HANDLE;
  return granit::detail::renderer_registry::instance().create_readback_batch(renderer, *desc,
                                                                              *batch);
}

extern "C" granit_result granit_readback_batch_read_buffer(
    granit_renderer renderer, granit_readback_batch batch, granit_buffer buffer, uint64_t offset,
    uint64_t size, uint32_t* result_index) {
  if (renderer == GRANIT_NULL_HANDLE || batch == GRANIT_NULL_HANDLE ||
      buffer == GRANIT_NULL_HANDLE || size == 0 || result_index == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  return granit::detail::renderer_registry::instance().readback_batch_read_buffer(
      renderer, batch, buffer, offset, size, *result_index);
}

extern "C" granit_result granit_readback_batch_read_texture(
    granit_renderer renderer, granit_readback_batch batch, granit_texture texture,
    const granit_texture_write_region* region, uint32_t* result_index) {
  if (renderer == GRANIT_NULL_HANDLE || batch == GRANIT_NULL_HANDLE ||
      texture == GRANIT_NULL_HANDLE || region == nullptr || result_index == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  return granit::detail::renderer_registry::instance().readback_batch_read_texture(
      renderer, batch, texture, *region, *result_index);
}

extern "C" granit_result granit_readback_batch_get_info(granit_renderer renderer,
                                                        granit_readback_batch batch,
                                                        granit_readback_batch_info* info) {
  if (renderer == GRANIT_NULL_HANDLE || batch == GRANIT_NULL_HANDLE || info == nullptr ||
      info->struct_size < GRANIT_READBACK_BATCH_INFO_VERSION_1_SIZE)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  return granit::detail::renderer_registry::instance().get_readback_batch_info(renderer, batch,
                                                                                *info);
}

extern "C" granit_result granit_readback_batch_submit_async(
    granit_renderer renderer, granit_readback_batch batch, granit_async_operation* operation) {
  if (renderer == GRANIT_NULL_HANDLE || batch == GRANIT_NULL_HANDLE || operation == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  *operation = GRANIT_NULL_HANDLE;
  return granit::detail::renderer_registry::instance().submit_readback_batch_async(
      renderer, batch, *operation);
}

extern "C" granit_result granit_readback_operation_get_result_info(
    granit_renderer renderer, granit_async_operation operation, uint32_t result_index,
    granit_readback_result_info* info) {
  if (renderer == GRANIT_NULL_HANDLE || operation == GRANIT_NULL_HANDLE || info == nullptr ||
      info->struct_size < GRANIT_READBACK_RESULT_INFO_VERSION_1_SIZE)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  return granit::detail::renderer_registry::instance().get_readback_result_info(
      renderer, operation, result_index, *info);
}

extern "C" granit_result granit_readback_operation_copy_result(
    granit_renderer renderer, granit_async_operation operation, uint32_t result_index, void* data,
    uint64_t* data_size) {
  if (renderer == GRANIT_NULL_HANDLE || operation == GRANIT_NULL_HANDLE || data_size == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  return granit::detail::renderer_registry::instance().copy_readback_result(
      renderer, operation, result_index, data, *data_size);
}

extern "C" granit_result granit_readback_batch_reset(granit_renderer renderer,
                                                      granit_readback_batch batch) {
  if (renderer == GRANIT_NULL_HANDLE || batch == GRANIT_NULL_HANDLE)
    return GRANIT_ERROR_INVALID_HANDLE;
  return granit::detail::renderer_registry::instance().reset_readback_batch(renderer, batch);
}

extern "C" granit_result granit_readback_batch_destroy(granit_renderer renderer,
                                                        granit_readback_batch batch) {
  if (renderer == GRANIT_NULL_HANDLE || batch == GRANIT_NULL_HANDLE)
    return GRANIT_ERROR_INVALID_HANDLE;
  return granit::detail::renderer_registry::instance().destroy_readback_batch(renderer, batch);
}
