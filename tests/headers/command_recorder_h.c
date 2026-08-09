// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/command_recorder.h>

typedef char granit_command_recorder_desc_size_check
    [sizeof(granit_command_recorder_desc) == GRANIT_COMMAND_RECORDER_DESC_VERSION_1_SIZE ? 1 : -1];

granit_command_recorder_desc granit_command_recorder_c_header_check(void) {
  const granit_command_recorder_desc desc = GRANIT_COMMAND_RECORDER_DESC_INIT;
  return desc;
}
