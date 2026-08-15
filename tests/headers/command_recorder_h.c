// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/renderer/command_recorder.h>

typedef char granit_command_recorder_desc_size_check
    [sizeof(granit_command_recorder_desc) == GRANIT_COMMAND_RECORDER_DESC_VERSION_1_SIZE ? 1 : -1];
typedef char granit_buffer_copy_region_size_check[sizeof(granit_buffer_copy_region) == 24 ? 1 : -1];
typedef char
    granit_texture_copy_region_size_check[sizeof(granit_texture_copy_region) == 64 ? 1 : -1];

granit_command_recorder_desc granit_command_recorder_c_header_check(void) {
  const granit_command_recorder_desc desc = GRANIT_COMMAND_RECORDER_DESC_INIT;
  return desc;
}
