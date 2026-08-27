// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/tools/shader_tools.h>

#include <stdint.h>
#include <string.h>

int main(int argc, char** argv) {
  granit_shader_tools_inspect_desc desc;
  granit_shader_tools_result result = 0;
  granit_shader_tools_result_info info;
  granit_shader_tools_binding_info binding;
  uint64_t binding_count = 0;
  uint64_t output_count = 0;
  granit_shader_tools_interface_variable_info shader_output;
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
  if (granit_shader_tools_result_get_binding_count(result, &binding_count) != GRANIT_SUCCESS ||
      binding_count != 3)
    return 4;
  memset(&binding, 0, sizeof(binding));
  binding.struct_size = (uint32_t)sizeof(binding);
  if (granit_shader_tools_result_get_binding(result, 0, &binding) != GRANIT_SUCCESS ||
      binding.group != 0 || binding.binding != 0 ||
      binding.type != GRANIT_SHADER_TOOLS_BINDING_UNIFORM_BUFFER ||
      binding.access != GRANIT_SHADER_TOOLS_ACCESS_READ || binding.minimum_binding_size != 16 ||
      binding.name_length == 0)
    return 5;
  if (granit_shader_tools_result_get_binding(result, binding_count, &binding) !=
      GRANIT_ERROR_INVALID_ARGUMENT)
    return 6;
  if (granit_shader_tools_result_get_fragment_output_count(result, &output_count) !=
          GRANIT_SUCCESS ||
      output_count != 1)
    return 7;
  memset(&shader_output, 0, sizeof(shader_output));
  shader_output.struct_size = (uint32_t)sizeof(shader_output);
  if (granit_shader_tools_result_get_fragment_output(result, 0, &shader_output) != GRANIT_SUCCESS ||
      shader_output.location != 0 ||
      shader_output.scalar_type != GRANIT_SHADER_TOOLS_SCALAR_FLOAT ||
      shader_output.bit_width != 32 || shader_output.vector_size != 4)
    return 8;
  if (granit_shader_tools_result_destroy(result) != GRANIT_SUCCESS ||
      granit_shader_tools_result_destroy(result) != GRANIT_ERROR_INVALID_HANDLE)
    return 9;
  return 0;
}
