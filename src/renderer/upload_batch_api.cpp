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
  try {
    return granit::detail::renderer_registry::instance().create_upload_batch(renderer, *batch);
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
