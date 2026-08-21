// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_PIPELINE_MESH_H_
#define GRANIT_PIPELINE_MESH_H_

#include <stddef.h>
#include <stdint.h>

#include <granit/core/result.h>
#include <granit/core/types.h>
#include <granit/pipeline/export.h>
#include <granit/renderer/buffer.h>
#include <granit/renderer/command_recorder.h>
#include <granit/renderer/pipeline.h>

/** 一次不可变 GPU Draw 的 Mesh 句柄。零值无效。 */
typedef granit_handle granit_mesh;

/** 一个 Vertex Buffer binding；布局数组只在 Mesh 创建期间借用。 */
typedef struct granit_mesh_vertex_buffer {
  granit_buffer buffer;
  uint64_t offset;
  granit_vertex_buffer_layout layout;
} granit_mesh_vertex_buffer;

/** 一个 Mesh 对应一次非索引或索引 Draw。 */
typedef struct granit_mesh_desc {
  uint32_t struct_size;
  granit_primitive_topology topology;
  const granit_mesh_vertex_buffer* vertex_buffers;
  uint32_t vertex_buffer_count;
  uint32_t indexed;
  granit_buffer index_buffer;
  uint64_t index_buffer_offset;
  granit_index_type index_type;
  uint32_t vertex_count;
  uint32_t index_count;
  uint32_t instance_count;
  uint32_t first_vertex;
  uint32_t first_index;
  int32_t vertex_offset;
  uint32_t first_instance;
  uint32_t reserved;
} granit_mesh_desc;

#define GRANIT_MESH_DESC_VERSION_1_SIZE                                                            \
  ((uint32_t)(offsetof(granit_mesh_desc, reserved) + sizeof(uint64_t)))

#define GRANIT_MESH_DESC_INIT                                                                      \
  {(uint32_t)sizeof(granit_mesh_desc),                                                             \
   GRANIT_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,                                                        \
   0,                                                                                              \
   UINT32_C(0),                                                                                    \
   UINT32_C(0),                                                                                    \
   GRANIT_NULL_HANDLE,                                                                             \
   UINT64_C(0),                                                                                    \
   GRANIT_INDEX_TYPE_UINT16,                                                                       \
   UINT32_C(0),                                                                                    \
   UINT32_C(0),                                                                                    \
   UINT32_C(1),                                                                                    \
   UINT32_C(0),                                                                                    \
   UINT32_C(0),                                                                                    \
   INT32_C(0),                                                                                     \
   UINT32_C(0),                                                                                    \
   UINT32_C(0)}

#ifdef __cplusplus
extern "C" {
#endif

/**
 * 复制不可变 Mesh 描述。Buffer 由调用者拥有，且必须在 Mesh 使用期间保持有效。
 */
GRANIT_RENDER_PIPELINE_API granit_result granit_mesh_create(granit_renderer renderer,
                                                            const granit_mesh_desc* desc,
                                                            granit_mesh* mesh);

/** 销毁 Mesh 描述，不销毁其借用的 Buffer。 */
GRANIT_RENDER_PIPELINE_API granit_result granit_mesh_destroy(granit_renderer renderer,
                                                             granit_mesh mesh);

#ifdef __cplusplus
}
#endif

#endif
