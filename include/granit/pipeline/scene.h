// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_PIPELINE_SCENE_H_
#define GRANIT_PIPELINE_SCENE_H_

#include <stddef.h>
#include <stdint.h>

#include <granit/core/result.h>
#include <granit/core/types.h>
#include <granit/math/types.h>
#include <granit/pipeline/export.h>
#include <granit/renderer/renderer.h>

/** 事务式复制后的多 View 场景快照句柄。零值无效。 */
typedef granit_handle granit_scene_snapshot;

typedef struct granit_scene_view {
  granit_matrix4 view;
  granit_matrix4 projection;
  granit_matrix4 view_projection;
  granit_float3 camera_position;
  float viewport_x;
  float viewport_y;
  float viewport_width;
  float viewport_height;
  uint64_t layer_mask;
} granit_scene_view;

typedef struct granit_scene_renderable {
  granit_matrix4 model;
  granit_matrix4 normal_matrix;
  granit_float3 bounds_center;
  float bounds_radius;
  uint64_t layer_mask;
  uint64_t sort_key;
  uint64_t payload;
  uint32_t object_id;
  uint32_t reserved;
} granit_scene_renderable;

typedef struct granit_scene_directional_light {
  granit_float3 direction_to_light;
  granit_float3 radiance;
  uint64_t layer_mask;
} granit_scene_directional_light;

typedef struct granit_scene_point_light {
  granit_float3 position;
  granit_float3 intensity;
  float radius;
  uint64_t layer_mask;
} granit_scene_point_light;

typedef struct granit_scene_spot_light {
  granit_float3 position;
  granit_float3 direction;
  granit_float3 intensity;
  float radius;
  float inner_angle;
  float outer_angle;
  uint64_t layer_mask;
} granit_scene_spot_light;

typedef struct granit_scene_snapshot_desc {
  uint32_t struct_size;
  uint32_t reserved;
  const granit_scene_view* views;
  uint32_t view_count;
  const granit_scene_renderable* renderables;
  uint32_t renderable_count;
  const granit_scene_directional_light* directional_lights;
  uint32_t directional_light_count;
  const granit_scene_point_light* point_lights;
  uint32_t point_light_count;
  const granit_scene_spot_light* spot_lights;
  uint32_t spot_light_count;
} granit_scene_snapshot_desc;

#define GRANIT_SCENE_SNAPSHOT_DESC_INIT                                                            \
  {(uint32_t)sizeof(granit_scene_snapshot_desc),                                                   \
   UINT32_C(0),                                                                                    \
   0,                                                                                              \
   UINT32_C(0),                                                                                    \
   0,                                                                                              \
   UINT32_C(0),                                                                                    \
   0,                                                                                              \
   UINT32_C(0),                                                                                    \
   0,                                                                                              \
   UINT32_C(0),                                                                                    \
   0,                                                                                              \
   UINT32_C(0)}

#ifdef __cplusplus
extern "C" {
#endif

/** 校验并复制场景输入；成功后由调用者销毁快照。 */
GRANIT_RENDER_PIPELINE_API granit_result
granit_scene_snapshot_create(granit_renderer renderer, const granit_scene_snapshot_desc* desc,
                             granit_scene_snapshot* snapshot);

/** 销毁快照并使旧句柄立即失效。 */
GRANIT_RENDER_PIPELINE_API granit_result
granit_scene_snapshot_destroy(granit_renderer renderer, granit_scene_snapshot snapshot);

#ifdef __cplusplus
}
#endif

#endif
