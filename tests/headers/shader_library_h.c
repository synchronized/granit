// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/renderer/shader_library.h>

typedef char granit_shader_library_desc_size_check
    [sizeof(granit_shader_library_desc) == GRANIT_SHADER_LIBRARY_DESC_VERSION_1_SIZE ? 1 : -1];
typedef char granit_shader_library_info_size_check
    [sizeof(granit_shader_library_info) == GRANIT_SHADER_LIBRARY_INFO_VERSION_1_SIZE ? 1 : -1];

granit_shader_library_desc granit_shader_library_desc_header_check(void) {
  const granit_shader_library_desc desc = GRANIT_SHADER_LIBRARY_DESC_INIT;
  return desc;
}

granit_shader_library_info granit_shader_library_info_header_check(void) {
  const granit_shader_library_info info = GRANIT_SHADER_LIBRARY_INFO_INIT;
  return info;
}
