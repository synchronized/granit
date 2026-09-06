// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/renderer/async_operation.h>

#include "renderer/renderer_registry.h"

extern "C" granit_result granit_async_operation_get_status(
    granit_renderer renderer, granit_async_operation operation,
    granit_async_operation_status* status) {
  if (status == nullptr || status->struct_size < GRANIT_ASYNC_OPERATION_STATUS_VERSION_1_SIZE)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  return granit::detail::renderer_registry::instance().get_async_operation_status(renderer,
                                                                                   operation,
                                                                                   *status);
}

extern "C" granit_result
granit_async_operation_request_cancel(granit_renderer renderer,
                                      granit_async_operation operation) {
  return granit::detail::renderer_registry::instance().request_async_operation_cancel(renderer,
                                                                                       operation);
}

extern "C" granit_result granit_async_operation_destroy(granit_renderer renderer,
                                                         granit_async_operation operation) {
  return granit::detail::renderer_registry::instance().destroy_async_operation(renderer,
                                                                                operation);
}
