// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/pipeline/material.h>
#include <granit/pipeline/render_pipeline.h>
#include <granit/pipeline/scene.h>

int main(void) {
  const granit_render_pipeline_desc pipeline = GRANIT_RENDER_PIPELINE_DESC_INIT;
  const granit_scene_snapshot_desc scene = GRANIT_SCENE_SNAPSHOT_DESC_INIT;
  const granit_material_desc material = GRANIT_MATERIAL_DESC_INIT;
  const uint64_t id = granit_material_parameter_id("base_color", UINT32_C(10));
  return pipeline.struct_size != 0 && scene.struct_size != 0 && material.struct_size != 0 && id != 0
             ? 0
             : 1;
}
