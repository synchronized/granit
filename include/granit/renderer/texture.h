// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_TEXTURE_H_
#define GRANIT_TEXTURE_H_

#include <granit/core/export.h>
#include <granit/core/result.h>
#include <granit/core/types.h>
#include <granit/renderer/renderer.h>
#include <granit/renderer/resource_types.h>

/** Texture 存储句柄。零值无效。 */
typedef granit_handle granit_texture;
/** Texture 子资源访问视图句柄。零值无效。 */
typedef granit_handle granit_texture_view;

/** 同步原始像素读取返回的紧密布局。 */
typedef struct granit_texture_readback_info {
  uint32_t struct_size;
  granit_texture_format format;
  uint32_t width;
  uint32_t height;
  uint32_t depth;
  uint32_t array_layer_count;
  uint32_t bytes_per_row;
  uint32_t rows_per_image;
  uint64_t required_size;
  uint32_t reserved[2];
} granit_texture_readback_info;
#define GRANIT_TEXTURE_READBACK_INFO_VERSION_1_SIZE UINT32_C(48)
#define GRANIT_TEXTURE_READBACK_INFO_INIT                                                       \
  {GRANIT_TEXTURE_READBACK_INFO_VERSION_1_SIZE, GRANIT_TEXTURE_FORMAT_UNDEFINED, UINT32_C(0),  \
   UINT32_C(0), UINT32_C(0), UINT32_C(0), UINT32_C(0), UINT32_C(0), UINT64_C(0), {             \
     UINT32_C(0), UINT32_C(0)                                                                  \
   }}

#ifdef __cplusplus
extern "C" {
#endif

/** 查询格式的紧密排列块信息；结果不包含设备相关的行或 Buffer 对齐。 */
GRANIT_API granit_result granit_texture_format_get_footprint(
    granit_texture_format format, granit_texture_format_footprint* footprint);

/** 创建未初始化的 Texture 存储。 */
GRANIT_API granit_result granit_texture_create(granit_renderer renderer,
                                               const granit_texture_desc* desc,
                                               granit_texture* texture);
/** 创建引用父 Texture 的 View。 */
GRANIT_API granit_result granit_texture_view_create(granit_renderer renderer,
                                                    granit_texture texture,
                                                    const granit_texture_view_desc* desc,
                                                    granit_texture_view* view);
/** 原子地创建 Texture 及覆盖完整范围的默认 View。 */
GRANIT_API granit_result granit_texture_create_with_default_view(granit_renderer renderer,
                                                                 const granit_texture_desc* desc,
                                                                 granit_texture* texture,
                                                                 granit_texture_view* view);
/**
 * 同步复制 CPU 数据到 Texture。函数返回后不再访问 data；GPU 上传可能在内部异步实现。
 * 当前支持非压缩颜色格式和单采样 Texture，同一 Texture 的并发写入由调用方排序。
 */
GRANIT_API granit_result granit_texture_write(granit_renderer renderer, granit_texture texture,
                                              const void* data, uint64_t size,
                                              const granit_texture_data_layout* layout,
                                              const granit_texture_write_region* region);
/**
 * 阻塞读取一个非压缩颜色区域。data 为 NULL 时只查询布局和所需容量；data_size 始终返回所需容量。
 * 函数不翻转 Y、不转换颜色空间或通道顺序。
 */
GRANIT_API granit_result granit_texture_read(granit_renderer renderer, granit_texture texture,
                                             const granit_texture_write_region* region, void* data,
                                             uint64_t* data_size,
                                             granit_texture_readback_info* info);
/** 销毁 View，不影响父 Texture。 */
GRANIT_API granit_result granit_texture_view_destroy(granit_renderer renderer,
                                                     granit_texture_view view);
/** 销毁 Texture，并级联使全部子 View 失效。 */
GRANIT_API granit_result granit_texture_destroy(granit_renderer renderer, granit_texture texture);

#ifdef __cplusplus
}
#endif

#endif
