// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_PIPELINE_ENVIRONMENT_MAP_H_
#define GRANIT_PIPELINE_ENVIRONMENT_MAP_H_

#include <stdint.h>

#include <granit/core/result.h>
#include <granit/core/types.h>
#include <granit/pipeline/export.h>
#include <granit/pipeline/render_pipeline.h>
#include <granit/renderer/renderer.h>

/** 拥有一组 Render Pipeline IBL 纹理的环境资源句柄。零值无效。 */
typedef granit_handle granit_environment_map;

typedef struct granit_environment_map_asset_desc {
  uint32_t struct_size;
  uint32_t reserved;
  const void* data;
  uint64_t size;
} granit_environment_map_asset_desc;

#define GRANIT_ENVIRONMENT_MAP_ASSET_DESC_VERSION_1_SIZE                                           \
  ((uint32_t)sizeof(granit_environment_map_asset_desc))

#define GRANIT_ENVIRONMENT_MAP_ASSET_DESC_INIT                                                     \
  {(uint32_t)sizeof(granit_environment_map_asset_desc), UINT32_C(0), 0, UINT64_C(0)}

typedef struct granit_environment_map_info {
  uint32_t struct_size;
  uint32_t reserved;
  granit_render_pipeline_environment environment;
  float recommended_exposure_ev;
  uint32_t reserved_tail;
} granit_environment_map_info;

#define GRANIT_ENVIRONMENT_MAP_INFO_VERSION_1_SIZE ((uint32_t)sizeof(granit_environment_map_info))

#define GRANIT_ENVIRONMENT_MAP_INFO_INIT                                                           \
  {(uint32_t)sizeof(granit_environment_map_info), UINT32_C(0),                                     \
   GRANIT_RENDER_PIPELINE_ENVIRONMENT_INIT, 0.0F, UINT32_C(0)}

#ifdef __cplusplus
extern "C" {
#endif

/** 从调用方提供的 GRENV v3 字节创建环境资源；输入仅在调用期间借用。 */
GRANIT_RENDER_PIPELINE_API granit_result granit_environment_map_create_from_asset(
    granit_renderer renderer, const granit_environment_map_asset_desc* desc,
    granit_environment_map* environment_map);

/** 创建无需外部资产的低分辨率中性环境。 */
GRANIT_RENDER_PIPELINE_API granit_result granit_environment_map_create_builtin(
    granit_renderer renderer, granit_environment_map* environment_map);

/** 获取可在 Render Pipeline 渲染描述中借用的环境视图和推荐曝光。 */
GRANIT_RENDER_PIPELINE_API granit_result
granit_environment_map_get_info(granit_renderer renderer, granit_environment_map environment_map,
                                granit_environment_map_info* info);

/** 销毁环境及其纹理，使旧句柄立即失效。 */
GRANIT_RENDER_PIPELINE_API granit_result
granit_environment_map_destroy(granit_renderer renderer, granit_environment_map environment_map);

#ifdef __cplusplus
}
#endif

#endif
