// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/renderer/buffer.h>

typedef char granit_buffer_initial_data_size_check
    [(sizeof(granit_buffer_initial_data) == GRANIT_BUFFER_INITIAL_DATA_VERSION_1_SIZE) ? 1 : -1];

granit_buffer granit_buffer_header_check(void) {
  (void)&granit_buffer_flush;
  return GRANIT_NULL_HANDLE;
}
