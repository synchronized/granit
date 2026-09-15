// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/asset_tools/asset_tools.h>

#include <stdint.h>

int main(void) {
  static const uint8_t invalid_asset[] = {0};
  static const char missing_shader[] = "missing-handle-safety-test.spv";
  granit_asset_tools_material_result material = 0;
  granit_asset_tools_texture_result texture = 0;
  granit_asset_tools_texture_result replacement_texture = 0;
  granit_asset_tools_environment_result environment = 0;
  granit_asset_tools_shader_reflection reflection = 0;
  granit_asset_tools_shader_inspect_desc shader_desc = {0};
  granit_asset_tools_shader_reflection_info reflection_info = {0};
  const void* bytes = 0;
  uint64_t byte_count = 0;

  shader_desc.struct_size = (uint32_t)sizeof(shader_desc);
  shader_desc.input_path = missing_shader;
  shader_desc.input_path_length = (uint64_t)(sizeof(missing_shader) - 1);
  reflection_info.struct_size = (uint32_t)sizeof(reflection_info);

  if (granit_asset_tools_material_inspect(invalid_asset, sizeof(invalid_asset), &material) !=
          GRANIT_ERROR_INVALID_ARGUMENT ||
      granit_asset_tools_texture_inspect(invalid_asset, sizeof(invalid_asset), &texture) !=
          GRANIT_ERROR_INVALID_ARGUMENT ||
      granit_asset_tools_environment_inspect(invalid_asset, sizeof(invalid_asset), &environment) !=
          GRANIT_ERROR_INVALID_ARGUMENT ||
      granit_asset_tools_shader_inspect_spirv(&shader_desc, &reflection) !=
          GRANIT_ERROR_INVALID_ARGUMENT ||
      material == 0 || texture == 0 || environment == 0 || reflection == 0)
    return 1;

  if (material == texture || material == environment || material == reflection ||
      texture == environment || texture == reflection || environment == reflection)
    return 2;

  if (granit_asset_tools_material_result_get_archive(texture, &bytes, &byte_count) !=
          GRANIT_ERROR_INVALID_HANDLE ||
      granit_asset_tools_texture_result_get_manifest(environment, &bytes, &byte_count) !=
          GRANIT_ERROR_INVALID_HANDLE ||
      granit_asset_tools_environment_result_get_package(material, &bytes, &byte_count) !=
          GRANIT_ERROR_INVALID_HANDLE ||
      granit_asset_tools_shader_reflection_get_info(material, &reflection_info) !=
          GRANIT_ERROR_INVALID_HANDLE)
    return 3;

  if (granit_asset_tools_material_result_destroy(texture) != GRANIT_ERROR_INVALID_HANDLE ||
      granit_asset_tools_texture_result_destroy(environment) != GRANIT_ERROR_INVALID_HANDLE ||
      granit_asset_tools_environment_result_destroy(reflection) != GRANIT_ERROR_INVALID_HANDLE ||
      granit_asset_tools_shader_reflection_destroy(material) != GRANIT_ERROR_INVALID_HANDLE)
    return 4;

  if (granit_asset_tools_texture_result_destroy(texture) != GRANIT_SUCCESS ||
      granit_asset_tools_texture_inspect(invalid_asset, sizeof(invalid_asset),
                                         &replacement_texture) != GRANIT_ERROR_INVALID_ARGUMENT ||
      replacement_texture == 0 || replacement_texture == texture ||
      granit_asset_tools_texture_result_get_manifest(texture, &bytes, &byte_count) !=
          GRANIT_ERROR_INVALID_HANDLE)
    return 5;

  if (granit_asset_tools_material_result_destroy(material) != GRANIT_SUCCESS ||
      granit_asset_tools_texture_result_destroy(replacement_texture) != GRANIT_SUCCESS ||
      granit_asset_tools_environment_result_destroy(environment) != GRANIT_SUCCESS ||
      granit_asset_tools_shader_reflection_destroy(reflection) != GRANIT_SUCCESS)
    return 6;

  return 0;
}
