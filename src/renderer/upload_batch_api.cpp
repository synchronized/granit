// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/renderer/upload_batch.h>

#include "renderer/renderer_registry.h"

extern "C" granit_result granit_upload_batch_create(granit_renderer renderer,
                                                    const granit_upload_batch_desc* desc,
                                                    granit_upload_batch* batch) {
  if (desc == nullptr || batch == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  *batch = GRANIT_NULL_HANDLE;
  if (renderer == GRANIT_NULL_HANDLE)
    return GRANIT_ERROR_INVALID_HANDLE;
  if (desc->struct_size < GRANIT_UPLOAD_BATCH_DESC_VERSION_1_SIZE || desc->flags != 0 ||
      desc->reserved != 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const auto max_staged_bytes = desc->struct_size >= GRANIT_UPLOAD_BATCH_DESC_VERSION_2_SIZE
                                    ? desc->max_staged_bytes
                                    : UINT64_C(0);
  const auto max_operation_count = desc->struct_size >= GRANIT_UPLOAD_BATCH_DESC_VERSION_2_SIZE
                                       ? desc->max_operation_count
                                       : UINT32_C(0);
  if (desc->struct_size >= GRANIT_UPLOAD_BATCH_DESC_VERSION_2_SIZE && desc->reserved_2 != 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    return granit::detail::renderer_registry::instance().create_upload_batch(
        renderer, max_staged_bytes, max_operation_count, *batch);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

extern "C" granit_result granit_upload_batch_get_info(granit_renderer renderer,
                                                      granit_upload_batch batch,
                                                      granit_upload_batch_info* info) {
  if (info == nullptr || info->struct_size < GRANIT_UPLOAD_BATCH_INFO_VERSION_1_SIZE ||
      info->reserved != 0 || info->reserved_2 != 0 || info->reserved_3 != 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    return granit::detail::renderer_registry::instance().get_upload_batch_info(renderer, batch,
                                                                               *info);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

extern "C" granit_result granit_upload_batch_write_buffer(granit_renderer renderer,
                                                          granit_upload_batch batch,
                                                          granit_buffer buffer, uint64_t offset,
                                                          const void* data, uint64_t size) {
  if (data == nullptr || size == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  if (renderer == GRANIT_NULL_HANDLE || batch == GRANIT_NULL_HANDLE || buffer == GRANIT_NULL_HANDLE)
    return GRANIT_ERROR_INVALID_HANDLE;
  try {
    return granit::detail::renderer_registry::instance().upload_batch_write_buffer(
        renderer, batch, buffer, offset, data, size);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

extern "C" granit_result
granit_upload_batch_write_texture(granit_renderer renderer, granit_upload_batch batch,
                                  granit_texture texture, const void* data, uint64_t size,
                                  const granit_texture_data_layout* layout,
                                  const granit_texture_write_region* region) {
  if (data == nullptr || size == 0 || layout == nullptr || region == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  if (renderer == GRANIT_NULL_HANDLE || batch == GRANIT_NULL_HANDLE ||
      texture == GRANIT_NULL_HANDLE)
    return GRANIT_ERROR_INVALID_HANDLE;
  try {
    return granit::detail::renderer_registry::instance().upload_batch_write_texture(
        renderer, batch, texture, data, size, *layout, *region);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

extern "C" granit_result granit_upload_batch_submit(granit_renderer renderer,
                                                    granit_upload_batch batch) {
  try {
    return granit::detail::renderer_registry::instance().submit_upload_batch(renderer, batch);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

extern "C" granit_result granit_upload_batch_reset(granit_renderer renderer,
                                                   granit_upload_batch batch) {
  try {
    return granit::detail::renderer_registry::instance().reset_upload_batch(renderer, batch);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

extern "C" granit_result granit_upload_batch_destroy(granit_renderer renderer,
                                                     granit_upload_batch batch) {
  try {
    return granit::detail::renderer_registry::instance().destroy_upload_batch(renderer, batch);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}
