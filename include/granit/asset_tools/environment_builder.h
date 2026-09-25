// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_ENVIRONMENT_BUILDER_H_
#define GRANIT_ENVIRONMENT_BUILDER_H_

#include <stdint.h>

#include <granit/asset_tools/export.h>
#include <granit/core/result.h>

/** Environment Asset 构建或检查结果句柄。零值无效。 */
typedef uint64_t granit_asset_tools_environment_result;

/** Environment Asset 构建或检查结果信息；所有视图在结果句柄销毁前有效。 */
typedef struct granit_asset_tools_environment_result_info {
  uint32_t struct_size;
  uint32_t reserved;
  const void* package;
  uint64_t package_size;
  const char* debug_json;
  uint64_t debug_json_length;
  const char* diagnostic;
  uint64_t diagnostic_length;
} granit_asset_tools_environment_result_info;

#define GRANIT_ASSET_TOOLS_ENVIRONMENT_RESULT_INFO_INIT                                            \
  {(uint32_t)sizeof(granit_asset_tools_environment_result_info),                                   \
   UINT32_C(0),                                                                                    \
   0,                                                                                              \
   UINT64_C(0),                                                                                    \
   0,                                                                                              \
   UINT64_C(0),                                                                                    \
   0,                                                                                              \
   UINT64_C(0)}

/** 一个紧密排列的 RGBA16F Prefiltered Cube mip；六个面按层连续排列。 */
typedef struct granit_asset_tools_environment_mip_desc {
  uint32_t struct_size;
  uint32_t resolution;
  const void* pixels;
  uint64_t pixels_size;
  uint32_t reserved[2];
} granit_asset_tools_environment_mip_desc;

#define GRANIT_ASSET_TOOLS_ENVIRONMENT_MIP_DESC_INIT                                               \
  {                                                                                                \
    (uint32_t)sizeof(granit_asset_tools_environment_mip_desc), UINT32_C(0), 0, UINT64_C(0), {      \
      UINT32_C(0), UINT32_C(0)                                                                     \
    }                                                                                              \
  }

/**
 * Environment Asset 构建描述。所有像素均为紧密排列的 RGBA16F，输入内存只需在构建调用期间
 * 有效。Irradiance 和 Prefiltered 输入均按六个 Cube 面连续排列。
 */
typedef struct granit_asset_tools_environment_build_desc {
  uint32_t struct_size;
  float recommended_environment_intensity;
  float recommended_exposure_ev;
  uint32_t irradiance_resolution;
  const void* irradiance_pixels;
  uint64_t irradiance_pixels_size;
  const granit_asset_tools_environment_mip_desc* prefiltered_mips;
  uint32_t prefiltered_mip_count;
  uint32_t brdf_width;
  uint32_t brdf_height;
  const void* brdf_pixels;
  uint64_t brdf_pixels_size;
  uint32_t reserved[2];
} granit_asset_tools_environment_build_desc;

#define GRANIT_ASSET_TOOLS_ENVIRONMENT_BUILD_DESC_INIT                                             \
  {                                                                                                \
    (uint32_t)sizeof(granit_asset_tools_environment_build_desc), 0.12F, -0.5F, UINT32_C(0), 0,     \
        UINT64_C(0), 0, UINT32_C(0), UINT32_C(0), UINT32_C(0), 0, UINT64_C(0), {                   \
      UINT32_C(0), UINT32_C(0)                                                                     \
    }                                                                                              \
  }

#ifdef __cplusplus
extern "C" {
#endif

/** 构建确定性的 GRENV 包。 */
GRANIT_ASSET_TOOLS_API granit_result
granit_asset_tools_environment_build(const granit_asset_tools_environment_build_desc* desc,
                                     granit_asset_tools_environment_result* result);

/** 严格检查已有 GRENV 包，并生成稳定调试 JSON。 */
GRANIT_ASSET_TOOLS_API granit_result granit_asset_tools_environment_inspect(
    const void* package, uint64_t package_size, granit_asset_tools_environment_result* result);

/** 查询 GRENV 包字节、调试 JSON 和诊断。 */
GRANIT_ASSET_TOOLS_API granit_result granit_asset_tools_environment_result_get_info(
    granit_asset_tools_environment_result result, granit_asset_tools_environment_result_info* info);

/** 销毁结果。零值和已经销毁的句柄返回 GRANIT_ERROR_INVALID_HANDLE。 */
GRANIT_ASSET_TOOLS_API granit_result
granit_asset_tools_environment_result_destroy(granit_asset_tools_environment_result result);

#ifdef __cplusplus
}
#endif

#endif
