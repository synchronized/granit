// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_TEXTURE_H_
#define GRANIT_TEXTURE_H_

#include <granit/export.h>
#include <granit/renderer.h>
#include <granit/resource_types.h>
#include <granit/result.h>
#include <granit/types.h>

/** Texture 存储句柄。零值无效。 */
typedef granit_handle granit_texture;
/** Texture 子资源访问视图句柄。零值无效。 */
typedef granit_handle granit_texture_view;

#ifdef __cplusplus
extern "C" {
#endif

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
/** 销毁 View，不影响父 Texture。 */
GRANIT_API granit_result granit_texture_view_destroy(granit_renderer renderer,
                                                     granit_texture_view view);
/** 销毁 Texture，并级联使全部子 View 失效。 */
GRANIT_API granit_result granit_texture_destroy(granit_renderer renderer, granit_texture texture);

#ifdef __cplusplus
}
#endif

#endif
