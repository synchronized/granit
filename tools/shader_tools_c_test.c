// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/tools/shader_tools.h>

#include <stdint.h>
#include <string.h>

int main(int argc, char** argv) {
  granit_shader_tools_inspect_desc desc;
  granit_shader_tools_result result = 0;
  granit_shader_tools_result_info info;
  if (argc != 2)
    return 1;
  memset(&desc, 0, sizeof(desc));
  desc.struct_size = (uint32_t)sizeof(desc);
  desc.input_path = argv[1];
  desc.input_path_length = (uint64_t)strlen(argv[1]);
  if (granit_shader_tools_inspect_spirv(&desc, &result) != GRANIT_SUCCESS || result == 0)
    return 2;
  memset(&info, 0, sizeof(info));
  info.struct_size = (uint32_t)sizeof(info);
  if (granit_shader_tools_result_get_info(result, &info) != GRANIT_SUCCESS ||
      info.status != GRANIT_SUCCESS || info.stage != GRANIT_SHADER_TOOLS_STAGE_FRAGMENT ||
      info.entry_point_length == 0 || info.output_length == 0) {
    granit_shader_tools_result_destroy(result);
    return 3;
  }
  if (granit_shader_tools_result_destroy(result) != GRANIT_SUCCESS ||
      granit_shader_tools_result_destroy(result) != GRANIT_ERROR_INVALID_HANDLE)
    return 4;
  return 0;
}
