// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/command_recorder.h>

#include <cstdint>
#include <new>

#include "core/resource_validation.h"
#include "renderer/renderer_registry.h"

extern "C" granit_result granit_command_recorder_create(granit_renderer renderer,
                                                        const granit_command_recorder_desc* desc,
                                                        granit_command_recorder* recorder) {
  if (renderer == GRANIT_NULL_HANDLE || desc == nullptr || recorder == nullptr) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  *recorder = GRANIT_NULL_HANDLE;
  if (desc->struct_size < GRANIT_COMMAND_RECORDER_DESC_VERSION_1_SIZE || desc->flags != 0 ||
      desc->reserved != 0) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  return granit::detail::renderer_registry::instance().create_command_recorder(renderer, *recorder);
}

extern "C" granit_result granit_command_recorder_begin(granit_renderer renderer,
                                                       granit_command_recorder recorder) {
  if (renderer == GRANIT_NULL_HANDLE || recorder == GRANIT_NULL_HANDLE) {
    return GRANIT_ERROR_INVALID_HANDLE;
  }
  try {
    return granit::detail::renderer_registry::instance().begin_command_recorder(renderer, recorder);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

extern "C" granit_result granit_command_recorder_end(granit_renderer renderer,
                                                     granit_command_recorder recorder) {
  if (renderer == GRANIT_NULL_HANDLE || recorder == GRANIT_NULL_HANDLE) {
    return GRANIT_ERROR_INVALID_HANDLE;
  }
  try {
    return granit::detail::renderer_registry::instance().end_command_recorder(renderer, recorder);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

extern "C" granit_result granit_command_recorder_submit(granit_renderer renderer,
                                                        granit_command_recorder recorder) {
  if (renderer == GRANIT_NULL_HANDLE || recorder == GRANIT_NULL_HANDLE) {
    return GRANIT_ERROR_INVALID_HANDLE;
  }
  try {
    return granit::detail::renderer_registry::instance().submit_command_recorder(renderer,
                                                                                 recorder);
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

extern "C" granit_result granit_command_recorder_submit_frame(granit_renderer renderer,
                                                              granit_command_recorder recorder,
                                                              granit_frame frame) {
  if (renderer == GRANIT_NULL_HANDLE || recorder == GRANIT_NULL_HANDLE ||
      frame == GRANIT_NULL_HANDLE)
    return GRANIT_ERROR_INVALID_HANDLE;
  try {
    return granit::detail::renderer_registry::instance().submit_command_recorder_frame(
        renderer, recorder, frame);
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

extern "C" granit_result granit_command_recorder_reset(granit_renderer renderer,
                                                       granit_command_recorder recorder) {
  if (renderer == GRANIT_NULL_HANDLE || recorder == GRANIT_NULL_HANDLE) {
    return GRANIT_ERROR_INVALID_HANDLE;
  }
  try {
    return granit::detail::renderer_registry::instance().reset_command_recorder(renderer, recorder);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

extern "C" granit_result
granit_command_recorder_copy_buffer(granit_renderer renderer, granit_command_recorder recorder,
                                    granit_buffer source, granit_buffer destination,
                                    const granit_buffer_copy_region* regions,
                                    std::uint32_t region_count) {
  if (renderer == GRANIT_NULL_HANDLE || recorder == GRANIT_NULL_HANDLE ||
      source == GRANIT_NULL_HANDLE || destination == GRANIT_NULL_HANDLE) {
    return GRANIT_ERROR_INVALID_HANDLE;
  }
  if (regions == nullptr || region_count == 0) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  try {
    return granit::detail::renderer_registry::instance().copy_buffer(
        renderer, recorder, source, destination, {regions, region_count});
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

extern "C" granit_result
granit_command_recorder_fill_buffer(granit_renderer renderer, granit_command_recorder recorder,
                                    granit_buffer buffer, std::uint64_t offset, std::uint64_t size,
                                    std::uint32_t value) {
  if (renderer == GRANIT_NULL_HANDLE || recorder == GRANIT_NULL_HANDLE ||
      buffer == GRANIT_NULL_HANDLE) {
    return GRANIT_ERROR_INVALID_HANDLE;
  }
  try {
    return granit::detail::renderer_registry::instance().fill_buffer(renderer, recorder, buffer,
                                                                     offset, size, value);
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

extern "C" granit_result granit_command_recorder_bind_graphics_pipeline(
    granit_renderer renderer, granit_command_recorder recorder, granit_graphics_pipeline pipeline) {
  if (renderer == GRANIT_NULL_HANDLE || recorder == GRANIT_NULL_HANDLE ||
      pipeline == GRANIT_NULL_HANDLE)
    return GRANIT_ERROR_INVALID_HANDLE;
  try {
    return granit::detail::renderer_registry::instance().bind_graphics_pipeline(renderer, recorder,
                                                                                pipeline);
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

extern "C" granit_result granit_command_recorder_bind_graphics_groups(
    granit_renderer renderer, granit_command_recorder recorder, granit_pipeline_layout layout,
    uint32_t first_group, const granit_bind_group* bind_groups, uint32_t bind_group_count) {
  if (renderer == GRANIT_NULL_HANDLE || recorder == GRANIT_NULL_HANDLE ||
      layout == GRANIT_NULL_HANDLE)
    return GRANIT_ERROR_INVALID_HANDLE;
  if (!bind_groups || bind_group_count == 0 || first_group > 8 || bind_group_count > 8 ||
      first_group + bind_group_count > 8)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    return granit::detail::renderer_registry::instance().bind_graphics_groups(
        renderer, recorder, layout, first_group, {bind_groups, bind_group_count});
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

extern "C" granit_result
granit_command_recorder_begin_rendering(granit_renderer renderer, granit_command_recorder recorder,
                                        const granit_rendering_desc* desc) {
  if (renderer == GRANIT_NULL_HANDLE || recorder == GRANIT_NULL_HANDLE)
    return GRANIT_ERROR_INVALID_HANDLE;
  if (desc == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const auto validation = granit::detail::validate_rendering_desc(*desc);
  if (validation != GRANIT_SUCCESS)
    return validation;
  try {
    return granit::detail::renderer_registry::instance().begin_rendering(renderer, recorder, *desc);
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

extern "C" granit_result granit_command_recorder_end_rendering(granit_renderer renderer,
                                                               granit_command_recorder recorder) {
  if (renderer == GRANIT_NULL_HANDLE || recorder == GRANIT_NULL_HANDLE)
    return GRANIT_ERROR_INVALID_HANDLE;
  try {
    return granit::detail::renderer_registry::instance().end_rendering(renderer, recorder);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

extern "C" granit_result granit_command_recorder_destroy(granit_renderer renderer,
                                                         granit_command_recorder recorder) {
  if (renderer == GRANIT_NULL_HANDLE || recorder == GRANIT_NULL_HANDLE) {
    return GRANIT_ERROR_INVALID_HANDLE;
  }
  try {
    return granit::detail::renderer_registry::instance().destroy_command_recorder(renderer,
                                                                                  recorder);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}
