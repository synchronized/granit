// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/renderer/shader.h>

typedef char granit_shader_desc_size_check
    [sizeof(granit_shader_desc) >= GRANIT_SHADER_DESC_VERSION_1_SIZE ? 1 : -1];

granit_shader_desc granit_shader_header_check(void) {
  const granit_shader_desc desc = GRANIT_SHADER_DESC_INIT;
  return desc;
}
