// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <stddef.h>
#include <stdint.h>

#include <granit/pipeline/canvas_draw_list.h>
#include <granit/pipeline/debug_draw_list.h>
#include <granit/pipeline/environment_map.h>
#include <granit/pipeline/material.h>
#include <granit/pipeline/mesh.h>
#include <granit/pipeline/render_pipeline.h>
#include <granit/pipeline/scene.h>
#include <granit/pipeline/text_atlas.h>
#include <granit/pipeline/text_draw_list.h>

#include "snapshots/0.35.0/render_pipeline_layout.h"

#define GRANIT_ABI_ASSERT(name, expression) typedef char name[(expression) ? 1 : -1]

#if UINTPTR_MAX == UINT64_MAX
GRANIT_ABI_ASSERT(granit_035_pipeline_desc_size,
                  sizeof(granit_render_pipeline_desc) == GRANIT_ABI_035_PIPELINE_DESC_SIZE);
GRANIT_ABI_ASSERT(granit_035_pipeline_desc_record, offsetof(granit_render_pipeline_desc, record) ==
                                                       GRANIT_ABI_035_PIPELINE_DESC_RECORD);
GRANIT_ABI_ASSERT(granit_035_pipeline_render_desc_size,
                  sizeof(granit_render_pipeline_render_desc) ==
                      GRANIT_ABI_035_PIPELINE_RENDER_DESC_SIZE);
GRANIT_ABI_ASSERT(granit_035_pipeline_render_desc_outputs,
                  offsetof(granit_render_pipeline_render_desc, outputs) ==
                      GRANIT_ABI_035_PIPELINE_RENDER_DESC_OUTPUTS);
GRANIT_ABI_ASSERT(granit_035_pipeline_render_desc_environment,
                  offsetof(granit_render_pipeline_render_desc, environment) ==
                      GRANIT_ABI_035_PIPELINE_RENDER_DESC_ENVIRONMENT);
GRANIT_ABI_ASSERT(granit_035_pipeline_output_size,
                  sizeof(granit_render_pipeline_output) == GRANIT_ABI_035_PIPELINE_OUTPUT_SIZE);
GRANIT_ABI_ASSERT(granit_035_pipeline_environment_size,
                  sizeof(granit_render_pipeline_environment) ==
                      GRANIT_ABI_035_PIPELINE_ENVIRONMENT_SIZE);
GRANIT_ABI_ASSERT(granit_035_pipeline_scene_desc_size,
                  sizeof(granit_scene_snapshot_desc) == GRANIT_ABI_035_PIPELINE_SCENE_DESC_SIZE);
GRANIT_ABI_ASSERT(granit_035_pipeline_scene_desc_spot_lights,
                  offsetof(granit_scene_snapshot_desc, spot_lights) ==
                      GRANIT_ABI_035_PIPELINE_SCENE_DESC_SPOT_LIGHTS);
GRANIT_ABI_ASSERT(granit_035_pipeline_material_desc_size,
                  sizeof(granit_material_desc) == GRANIT_ABI_035_PIPELINE_MATERIAL_DESC_SIZE);
GRANIT_ABI_ASSERT(granit_035_pipeline_material_desc_archive_size,
                  offsetof(granit_material_desc, archive_size) ==
                      GRANIT_ABI_035_PIPELINE_MATERIAL_DESC_ARCHIVE_SIZE);
GRANIT_ABI_ASSERT(granit_035_pipeline_mesh_desc_size,
                  sizeof(granit_mesh_desc) == GRANIT_ABI_035_PIPELINE_MESH_DESC_SIZE);
GRANIT_ABI_ASSERT(granit_035_pipeline_mesh_desc_vertex_buffers,
                  offsetof(granit_mesh_desc, vertex_buffers) ==
                      GRANIT_ABI_035_PIPELINE_MESH_DESC_VERTEX_BUFFERS);
GRANIT_ABI_ASSERT(granit_035_pipeline_canvas_desc_size,
                  sizeof(granit_canvas_draw_list_desc) == GRANIT_ABI_035_PIPELINE_CANVAS_DESC_SIZE);
GRANIT_ABI_ASSERT(granit_035_pipeline_debug_desc_size,
                  sizeof(granit_debug_draw_list_desc) == GRANIT_ABI_035_PIPELINE_DEBUG_DESC_SIZE);
GRANIT_ABI_ASSERT(granit_035_pipeline_text_atlas_desc_size,
                  sizeof(granit_text_atlas_desc) == GRANIT_ABI_035_PIPELINE_TEXT_ATLAS_DESC_SIZE);
GRANIT_ABI_ASSERT(granit_035_pipeline_text_draw_desc_size,
                  sizeof(granit_text_draw_list_desc) ==
                      GRANIT_ABI_035_PIPELINE_TEXT_DRAW_DESC_SIZE);
GRANIT_ABI_ASSERT(granit_035_pipeline_environment_map_info_size,
                  sizeof(granit_environment_map_info) ==
                      GRANIT_ABI_035_PIPELINE_ENVIRONMENT_MAP_INFO_SIZE);
#endif

#undef GRANIT_ABI_ASSERT
