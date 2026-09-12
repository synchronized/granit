// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/tools/shader_compiler.h>
#include <granit/tools/shader_library_builder.h>
#include <granit/tools/shader_reflection.h>

#include <stddef.h>
#include <stdint.h>
#include <string.h>

int main(int argc, char** argv) {
  granit_shader_tools_compiler_desc compiler_desc = GRANIT_SHADER_TOOLS_COMPILER_DESC_INIT;
  granit_shader_tools_compile_desc compile_desc = GRANIT_SHADER_TOOLS_COMPILE_DESC_INIT;
  granit_shader_tools_compiler compiler = 0;
  granit_shader_tools_inspect_desc desc;
  granit_shader_tools_compilation compilation = 0;
  granit_shader_tools_reflection reflection = 0;
  granit_shader_tools_reflection_info info;
  granit_shader_tools_binding_info binding;
  uint64_t binding_count = 0;
  uint64_t output_count = 0;
  granit_shader_tools_interface_variable_info shader_output;
  granit_shader_tools_expected_binding expected[3];
  const char* reflection_json = NULL;
  uint64_t reflection_json_length = 0;
  uint64_t tool_identity_length = 0;
  char tool_identity[64];
  uint32_t library_cache_hit = 1;
  granit_shader_tools_source_library_desc source_library_desc =
      GRANIT_SHADER_TOOLS_SOURCE_LIBRARY_DESC_INIT;
  if (argc != 2)
    return 1;
  if (granit_shader_tools_build_library_from_manifest(&source_library_desc, &library_cache_hit) !=
          GRANIT_ERROR_INVALID_ARGUMENT ||
      library_cache_hit != 0)
    return 17;
  if (granit_shader_tools_compiler_create(&compiler_desc, &compiler) != GRANIT_SUCCESS ||
      compiler == 0)
    return 15;
  compile_desc.input_path = argv[1];
  compile_desc.input_path_length = (uint64_t)strlen(argv[1]);
  compile_desc.entry_point = "fragment_main";
  compile_desc.entry_point_length = UINT64_C(13);
  compile_desc.stage = GRANIT_SHADER_STAGE_FRAGMENT;
  compile_desc.spirv_output_path = argv[1];
  compile_desc.spirv_output_path_length = (uint64_t)strlen(argv[1]);
  compile_desc.wgsl_output_path = argv[1];
  compile_desc.wgsl_output_path_length = (uint64_t)strlen(argv[1]);
  if (granit_shader_tools_compiler_compile(compiler, &compile_desc, &compilation) !=
          GRANIT_ERROR_NOT_READY ||
      compilation != 0 || granit_shader_tools_compiler_destroy(compiler) != GRANIT_SUCCESS ||
      granit_shader_tools_compiler_destroy(compiler) != GRANIT_ERROR_INVALID_HANDLE)
    return 15;
  if (granit_shader_tools_get_tool_identity(argv[1], (uint64_t)strlen(argv[1]), NULL,
                                            &tool_identity_length) != GRANIT_SUCCESS ||
      tool_identity_length != sizeof(tool_identity))
    return 14;
  tool_identity_length = 1;
  if (granit_shader_tools_get_tool_identity(argv[1], (uint64_t)strlen(argv[1]), tool_identity,
                                            &tool_identity_length) !=
          GRANIT_ERROR_INVALID_ARGUMENT ||
      tool_identity_length != sizeof(tool_identity) ||
      granit_shader_tools_get_tool_identity(argv[1], (uint64_t)strlen(argv[1]), tool_identity,
                                            &tool_identity_length) != GRANIT_SUCCESS)
    return 14;
  memset(&desc, 0, sizeof(desc));
  desc.struct_size = (uint32_t)sizeof(desc);
  desc.input_path = argv[1];
  desc.input_path_length = (uint64_t)strlen(argv[1]);
  memset(expected, 0, sizeof(expected));
  expected[0].struct_size = (uint32_t)sizeof(expected[0]);
  expected[1].struct_size = (uint32_t)sizeof(expected[1]);
  expected[2].struct_size = (uint32_t)sizeof(expected[2]);
  expected[0].binding = 0;
  expected[1].binding = 1;
  expected[2].binding = 2;
  desc.validate_binding_set = 1;
  desc.expected_bindings = expected;
  desc.expected_binding_count = 3;
  if (granit_shader_tools_inspect_spirv(&desc, &reflection) != GRANIT_SUCCESS || reflection == 0)
    return 2;
  memset(&info, 0, sizeof(info));
  info.struct_size = (uint32_t)sizeof(info);
  if (granit_shader_tools_reflection_get_info(reflection, &info) != GRANIT_SUCCESS ||
      info.status != GRANIT_SUCCESS || info.stage != GRANIT_SHADER_STAGE_FRAGMENT ||
      info.entry_point_length == 0 || info.output_length == 0) {
    granit_shader_tools_reflection_destroy(reflection);
    return 3;
  }
  if (granit_shader_tools_reflection_get_binding_count(reflection, &binding_count) !=
          GRANIT_SUCCESS ||
      binding_count != 3)
    return 4;
  memset(&binding, 0, sizeof(binding));
  binding.struct_size = (uint32_t)sizeof(binding);
  if (granit_shader_tools_reflection_get_binding(reflection, 0, &binding) != GRANIT_SUCCESS ||
      binding.group != 0 || binding.binding != 0 ||
      binding.type != GRANIT_SHADER_TOOLS_BINDING_UNIFORM_BUFFER ||
      binding.access != GRANIT_SHADER_TOOLS_ACCESS_READ || binding.minimum_binding_size != 16 ||
      binding.name_length == 0)
    return 5;
  if (granit_shader_tools_reflection_get_binding(reflection, binding_count, &binding) !=
      GRANIT_ERROR_INVALID_ARGUMENT)
    return 6;
  if (granit_shader_tools_reflection_get_fragment_output_count(reflection, &output_count) !=
          GRANIT_SUCCESS ||
      output_count != 1)
    return 7;
  memset(&shader_output, 0, sizeof(shader_output));
  shader_output.struct_size = (uint32_t)sizeof(shader_output);
  if (granit_shader_tools_reflection_get_fragment_output(reflection, 0, &shader_output) !=
          GRANIT_SUCCESS ||
      shader_output.location != 0 ||
      shader_output.scalar_type != GRANIT_SHADER_TOOLS_SCALAR_FLOAT ||
      shader_output.bit_width != 32 || shader_output.vector_size != 4)
    return 8;
  if (granit_shader_tools_reflection_get_json(reflection, &reflection_json,
                                              &reflection_json_length) != GRANIT_SUCCESS ||
      reflection_json == NULL || reflection_json_length == 0)
    return 13;
  if (granit_shader_tools_compilation_destroy(reflection) != GRANIT_ERROR_INVALID_HANDLE)
    return 16;
  if (granit_shader_tools_reflection_destroy(reflection) != GRANIT_SUCCESS ||
      granit_shader_tools_reflection_destroy(reflection) != GRANIT_ERROR_INVALID_HANDLE)
    return 9;
  reflection = 0;
  desc.expected_binding_count = 2;
  if (granit_shader_tools_inspect_spirv(&desc, &reflection) != GRANIT_ERROR_INVALID_ARGUMENT ||
      reflection == 0)
    return 10;
  memset(&info, 0, sizeof(info));
  info.struct_size = (uint32_t)sizeof(info);
  if (granit_shader_tools_reflection_get_info(reflection, &info) != GRANIT_SUCCESS ||
      info.diagnostic_length == 0 ||
      granit_shader_tools_reflection_destroy(reflection) != GRANIT_SUCCESS)
    return 11;
  reflection = 0;
  desc.struct_size = (uint32_t)offsetof(granit_shader_tools_inspect_desc, validate_binding_set);
  if (granit_shader_tools_inspect_spirv(&desc, &reflection) != GRANIT_SUCCESS || reflection == 0 ||
      granit_shader_tools_reflection_destroy(reflection) != GRANIT_SUCCESS)
    return 12;
  return 0;
}
