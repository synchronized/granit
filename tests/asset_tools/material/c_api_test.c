// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/asset_tools/material_builder.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static void* read_file(const char* path, uint64_t* size) {
  FILE* stream = NULL;
  long length;
  void* bytes;
  *size = 0;
#if defined(_WIN32)
  if (fopen_s(&stream, path, "rb") != 0)
    return NULL;
#else
  stream = fopen(path, "rb");
#endif
  if (stream == NULL)
    return NULL;
  if (fseek(stream, 0, SEEK_END) != 0 || (length = ftell(stream)) <= 0 ||
      fseek(stream, 0, SEEK_SET) != 0) {
    fclose(stream);
    return NULL;
  }
  bytes = malloc((size_t)length);
  if (bytes == NULL || fread(bytes, 1, (size_t)length, stream) != (size_t)length) {
    free(bytes);
    fclose(stream);
    return NULL;
  }
  fclose(stream);
  *size = (uint64_t)length;
  return bytes;
}

int main(int argc, char** argv) {
  granit_asset_tools_material_build_desc desc = GRANIT_ASSET_TOOLS_MATERIAL_BUILD_DESC_INIT;
  granit_asset_tools_material_shader_library library =
      GRANIT_ASSET_TOOLS_MATERIAL_SHADER_LIBRARY_INIT;
  granit_asset_tools_material_result built = 0;
  granit_asset_tools_material_result inspected = 0;
  granit_asset_tools_material_result_info info = GRANIT_ASSET_TOOLS_MATERIAL_RESULT_INFO_INIT;
  uint64_t source_size = 0;
  uint64_t library_size = 0;
  void* source;
  void* library_archive;
  if (argc != 3)
    return 1;
  source = read_file(argv[1], &source_size);
  library_archive = read_file(argv[2], &library_size);
  if (source == NULL || library_archive == NULL) {
    free(source);
    free(library_archive);
    return 2;
  }
  desc.source_json = (const char*)source;
  desc.source_json_length = source_size;
  desc.shader_libraries = &library;
  desc.shader_library_count = 1;
  library.archive = library_archive;
  library.archive_size = library_size;
  if (granit_asset_tools_material_build(&desc, &built) != GRANIT_SUCCESS || built == 0 ||
      granit_asset_tools_material_result_get_info(built, &info) != GRANIT_SUCCESS ||
      info.archive == NULL || info.archive_size == 0 || info.debug_json == NULL ||
      info.debug_json_length == 0 || info.diagnostic_length != 0 ||
      granit_asset_tools_material_inspect(info.archive, info.archive_size, &inspected) !=
          GRANIT_SUCCESS ||
      inspected == 0) {
    free(source);
    free(library_archive);
    return 3;
  }
  if (granit_asset_tools_material_result_destroy(inspected) != GRANIT_SUCCESS ||
      granit_asset_tools_material_result_destroy(built) != GRANIT_SUCCESS ||
      granit_asset_tools_material_result_destroy(built) != GRANIT_ERROR_INVALID_HANDLE) {
    free(source);
    free(library_archive);
    return 4;
  }
  info = (granit_asset_tools_material_result_info)GRANIT_ASSET_TOOLS_MATERIAL_RESULT_INFO_INIT;
  if (granit_asset_tools_material_result_get_info(0, &info) != GRANIT_ERROR_INVALID_HANDLE ||
      info.archive != NULL || info.archive_size != 0 ||
      granit_asset_tools_material_inspect(NULL, 0, &inspected) != GRANIT_ERROR_INVALID_ARGUMENT ||
      inspected != 0) {
    free(source);
    free(library_archive);
    return 5;
  }
  free(source);
  free(library_archive);
  return 0;
}
