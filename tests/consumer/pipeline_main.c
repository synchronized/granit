// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/pipeline/material.h>
#include <granit/pipeline/environment_map.h>
#include <granit/pipeline/mesh.h>
#include <granit/pipeline/render_pipeline.h>
#include <granit/pipeline/scene.h>

#include "linkage_check.h"

int main(void) {
  const granit_render_pipeline_desc pipeline = GRANIT_RENDER_PIPELINE_DESC_INIT;
  const granit_scene_snapshot_desc scene = GRANIT_SCENE_SNAPSHOT_DESC_INIT;
  const granit_material_desc material = GRANIT_MATERIAL_DESC_INIT;
  const granit_mesh_desc mesh = GRANIT_MESH_DESC_INIT;
  const uint64_t id = granit_material_parameter_id("base_color", UINT32_C(10));
  if (pipeline.struct_size == 0 || scene.struct_size == 0 || material.struct_size == 0 ||
      mesh.struct_size == 0 || id == 0)
    return 1;
  granit_render_pipeline handle = GRANIT_NULL_HANDLE;
  if (granit_render_pipeline_create(GRANIT_NULL_HANDLE, &pipeline, &handle) !=
          GRANIT_ERROR_INVALID_HANDLE ||
      handle != GRANIT_NULL_HANDLE)
    return 2;

  granit_renderer renderer = GRANIT_NULL_HANDLE;
  const granit_renderer_desc renderer_desc = GRANIT_RENDERER_DESC_INIT;
  const granit_result renderer_result = granit_renderer_create(&renderer_desc, &renderer);
  if (renderer_result == GRANIT_ERROR_BACKEND_UNAVAILABLE ||
      renderer_result == GRANIT_ERROR_INCOMPATIBLE_DRIVER ||
      renderer_result == GRANIT_ERROR_NO_SUITABLE_DEVICE)
    return renderer == GRANIT_NULL_HANDLE ? 0 : 3;
  if (renderer_result != GRANIT_SUCCESS)
    return 4;
  if (granit_render_pipeline_create(renderer, &pipeline, &handle) != GRANIT_SUCCESS ||
      handle == GRANIT_NULL_HANDLE)
    return 5;
  if (granit_render_pipeline_destroy(renderer, handle) != GRANIT_SUCCESS)
    return 6;
  if (granit_render_pipeline_destroy(renderer, handle) != GRANIT_ERROR_INVALID_HANDLE)
    return 7;
  granit_environment_map environment = GRANIT_NULL_HANDLE;
  if (granit_environment_map_create_builtin(renderer, &environment) != GRANIT_SUCCESS)
    return 8;
  granit_environment_map_info environment_info = GRANIT_ENVIRONMENT_MAP_INFO_INIT;
  if (granit_environment_map_get_info(renderer, environment, &environment_info) != GRANIT_SUCCESS ||
      environment_info.environment.irradiance == GRANIT_NULL_HANDLE)
    return 9;
  if (granit_environment_map_destroy(renderer, environment) != GRANIT_SUCCESS)
    return 10;
  return granit_renderer_destroy(renderer) == GRANIT_SUCCESS ? 0 : 11;
}
