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
  granit_asset_tools_material_shader_index index = GRANIT_ASSET_TOOLS_MATERIAL_SHADER_INDEX_INIT;
  granit_asset_tools_material_result built = 0;
  granit_asset_tools_material_result inspected = 0;
  const void* archive = NULL;
  const char* text = NULL;
  uint64_t archive_size = 0;
  uint64_t text_size = 0;
  uint64_t source_size = 0;
  uint64_t index_size = 0;
  void* source;
  void* index_json;
  if (argc != 3)
    return 1;
  source = read_file(argv[1], &source_size);
  index_json = read_file(argv[2], &index_size);
  if (source == NULL || index_json == NULL) {
    free(source);
    free(index_json);
    return 2;
  }
  desc.source_json = (const char*)source;
  desc.source_json_length = source_size;
  desc.shader_indices = &index;
  desc.shader_index_count = 1;
  index.json = (const char*)index_json;
  index.json_length = index_size;
  if (granit_asset_tools_material_build(&desc, &built) != GRANIT_SUCCESS || built == 0 ||
      granit_asset_tools_material_result_get_archive(built, &archive, &archive_size) !=
          GRANIT_SUCCESS ||
      archive == NULL || archive_size == 0 ||
      granit_asset_tools_material_result_get_debug_json(built, &text, &text_size) !=
          GRANIT_SUCCESS ||
      text == NULL || text_size == 0 ||
      granit_asset_tools_material_inspect(archive, archive_size, &inspected) != GRANIT_SUCCESS ||
      inspected == 0) {
    free(source);
    free(index_json);
    return 3;
  }
  if (granit_asset_tools_material_result_destroy(inspected) != GRANIT_SUCCESS ||
      granit_asset_tools_material_result_destroy(built) != GRANIT_SUCCESS ||
      granit_asset_tools_material_result_destroy(built) != GRANIT_ERROR_INVALID_HANDLE) {
    free(source);
    free(index_json);
    return 4;
  }
  archive = (const void*)1;
  archive_size = 1;
  if (granit_asset_tools_material_result_get_archive(0, &archive, &archive_size) !=
          GRANIT_ERROR_INVALID_HANDLE ||
      archive != NULL || archive_size != 0 ||
      granit_asset_tools_material_inspect(NULL, 0, &inspected) != GRANIT_ERROR_INVALID_ARGUMENT ||
      inspected != 0) {
    free(source);
    free(index_json);
    return 5;
  }
  free(source);
  free(index_json);
  return 0;
}
