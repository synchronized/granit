// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/window/window.h>

#include "window/platform/backend.h"

using namespace granit::window::detail;

extern "C" granit_result granit_window_system_create(const granit_window_system_desc* desc,
                                                     granit_window_system* output) {
  if (output == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  *output = GRANIT_NULL_HANDLE;
  if (desc == nullptr || desc->struct_size < GRANIT_WINDOW_SYSTEM_DESC_VERSION_1_SIZE ||
      desc->flags != 0 || desc->reserved != 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  return create_backend_system(desc->backend, output);
}

extern "C" granit_result granit_window_system_destroy(granit_window_system handle) {
  auto system = acquire_system(handle);
  if (!system)
    return GRANIT_ERROR_INVALID_HANDLE;
  if (!on_owner_thread(*system))
    return GRANIT_ERROR_INVALID_ARGUMENT;
  if (system->loop_running)
    return GRANIT_ERROR_RESOURCE_IN_USE;
  return system->operations == nullptr ? GRANIT_ERROR_INTERNAL
                                       : system->operations->destroy_system(handle, system);
}

extern "C" granit_result granit_window_system_process_events(granit_window_system handle) {
  auto system = acquire_system(handle);
  if (!system)
    return GRANIT_ERROR_INVALID_HANDLE;
  if (!on_owner_thread(*system))
    return GRANIT_ERROR_INVALID_ARGUMENT;
  return system->operations == nullptr ? GRANIT_ERROR_INTERNAL
                                       : system->operations->process_events(system);
}
