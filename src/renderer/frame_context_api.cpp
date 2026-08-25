// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/renderer/frame_context.h>

#include <new>

#include "renderer/renderer_registry.h"

extern "C" granit_result granit_frame_context_create(granit_renderer renderer,
                                                     const granit_frame_context_desc* desc,
                                                     granit_frame_context* context) {
  if (desc == nullptr || context == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  *context = GRANIT_NULL_HANDLE;
  if (renderer == GRANIT_NULL_HANDLE)
    return GRANIT_ERROR_INVALID_HANDLE;
  if (desc->struct_size < GRANIT_FRAME_CONTEXT_DESC_VERSION_1_SIZE || desc->flags != 0 ||
      desc->reserved != 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    return granit::detail::renderer_registry::instance().create_frame_context(renderer, *context);
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

extern "C" granit_result granit_frame_context_begin(granit_renderer renderer,
                                                    granit_frame_context context,
                                                    granit_frame frame,
                                                    granit_command_recorder* recorder,
                                                    uint32_t* frame_slot) {
  if (recorder == nullptr || frame_slot == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  *recorder = GRANIT_NULL_HANDLE;
  *frame_slot = 0;
  if (renderer == GRANIT_NULL_HANDLE || context == GRANIT_NULL_HANDLE ||
      frame == GRANIT_NULL_HANDLE)
    return GRANIT_ERROR_INVALID_HANDLE;
  try {
    return granit::detail::renderer_registry::instance().begin_frame_context(
        renderer, context, frame, *recorder, *frame_slot);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

extern "C" granit_result granit_frame_context_submit(granit_renderer renderer,
                                                     granit_frame_context context,
                                                     granit_frame frame) {
  if (renderer == GRANIT_NULL_HANDLE || context == GRANIT_NULL_HANDLE ||
      frame == GRANIT_NULL_HANDLE)
    return GRANIT_ERROR_INVALID_HANDLE;
  try {
    return granit::detail::renderer_registry::instance().submit_frame_context(renderer, context,
                                                                              frame);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

extern "C" granit_result granit_frame_context_abort(granit_renderer renderer,
                                                    granit_frame_context context,
                                                    granit_frame frame) {
  if (renderer == GRANIT_NULL_HANDLE || context == GRANIT_NULL_HANDLE ||
      frame == GRANIT_NULL_HANDLE)
    return GRANIT_ERROR_INVALID_HANDLE;
  try {
    return granit::detail::renderer_registry::instance().abort_frame_context(renderer, context,
                                                                             frame);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

extern "C" granit_result granit_frame_context_destroy(granit_renderer renderer,
                                                      granit_frame_context context) {
  if (renderer == GRANIT_NULL_HANDLE || context == GRANIT_NULL_HANDLE)
    return GRANIT_ERROR_INVALID_HANDLE;
  try {
    return granit::detail::renderer_registry::instance().destroy_frame_context(renderer, context);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}
