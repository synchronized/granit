// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_PIPELINE_H_
#define GRANIT_PIPELINE_H_

#include <stddef.h>
#include <stdint.h>

#include <granit/core/export.h>
#include <granit/core/result.h>
#include <granit/core/types.h>
#include <granit/renderer/renderer.h>
#include <granit/renderer/resource_types.h>
#include <granit/renderer/shader.h>

/** Pipeline 资源布局句柄，由零至多个 Bind Group Layout 按组序号组成。 */
typedef granit_handle granit_pipeline_layout;
/** 单组资源绑定布局句柄。 */
typedef granit_handle granit_bind_group_layout;
/** 符合单个 Bind Group Layout 的不可变资源组。 */
typedef granit_handle granit_bind_group;
/** 图形 Pipeline 句柄。 */
typedef granit_handle granit_graphics_pipeline;
/** 计算 Pipeline 句柄。 */
typedef granit_handle granit_compute_pipeline;

typedef uint32_t granit_binding_type;
#define GRANIT_BINDING_TYPE_UNIFORM_BUFFER UINT32_C(1)
#define GRANIT_BINDING_TYPE_STORAGE_BUFFER UINT32_C(2)
#define GRANIT_BINDING_TYPE_SAMPLED_TEXTURE UINT32_C(3)
#define GRANIT_BINDING_TYPE_STORAGE_TEXTURE UINT32_C(4)
#define GRANIT_BINDING_TYPE_SAMPLER UINT32_C(5)
#define GRANIT_BINDING_TYPE_DYNAMIC_UNIFORM_BUFFER UINT32_C(6)

typedef uint32_t granit_shader_stage_flags;
#define GRANIT_SHADER_STAGE_VERTEX_BIT (UINT32_C(1) << 0)
#define GRANIT_SHADER_STAGE_FRAGMENT_BIT (UINT32_C(1) << 1)
#define GRANIT_SHADER_STAGE_COMPUTE_BIT (UINT32_C(1) << 2)

typedef struct granit_bind_group_layout_entry {
  uint32_t binding;
  granit_binding_type type;
  uint32_t array_count;
  granit_shader_stage_flags visibility;
} granit_bind_group_layout_entry;

typedef struct granit_bind_group_layout_desc {
  uint32_t struct_size;
  uint32_t entry_count;
  const granit_bind_group_layout_entry* entries;
  uint64_t reserved;
} granit_bind_group_layout_desc;
#define GRANIT_BIND_GROUP_LAYOUT_DESC_VERSION_1_SIZE UINT32_C(24)
#define GRANIT_BIND_GROUP_LAYOUT_DESC_INIT                                                         \
  {GRANIT_BIND_GROUP_LAYOUT_DESC_VERSION_1_SIZE, UINT32_C(0), 0, UINT64_C(0)}

/** 单个数组元素的资源绑定；资源类型由 Layout 中同 binding 的声明决定。 */
typedef struct granit_bind_group_entry {
  uint32_t binding;
  uint32_t array_element;
  granit_handle resource;
  uint64_t offset;
  uint64_t size;
} granit_bind_group_entry;

typedef struct granit_bind_group_desc {
  uint32_t struct_size;
  uint32_t entry_count;
  granit_bind_group_layout layout;
  const granit_bind_group_entry* entries;
  uint64_t reserved;
} granit_bind_group_desc;
#define GRANIT_BIND_GROUP_DESC_VERSION_1_SIZE UINT32_C(32)
#define GRANIT_BIND_GROUP_DESC_INIT                                                                \
  {GRANIT_BIND_GROUP_DESC_VERSION_1_SIZE, UINT32_C(0), GRANIT_NULL_HANDLE, 0, UINT64_C(0)}
#define GRANIT_WHOLE_SIZE UINT64_MAX

typedef struct granit_pipeline_layout_desc {
  uint32_t struct_size;
  uint32_t bind_group_layout_count;
  const granit_bind_group_layout* bind_group_layouts;
  uint64_t reserved;
} granit_pipeline_layout_desc;
#define GRANIT_PIPELINE_LAYOUT_DESC_VERSION_1_SIZE UINT32_C(24)
#define GRANIT_PIPELINE_LAYOUT_DESC_INIT                                                           \
  {GRANIT_PIPELINE_LAYOUT_DESC_VERSION_1_SIZE, UINT32_C(0), 0, UINT64_C(0)}

typedef uint32_t granit_vertex_format;
#define GRANIT_VERTEX_FORMAT_FLOAT32 UINT32_C(1)
#define GRANIT_VERTEX_FORMAT_FLOAT32X2 UINT32_C(2)
#define GRANIT_VERTEX_FORMAT_FLOAT32X3 UINT32_C(3)
#define GRANIT_VERTEX_FORMAT_FLOAT32X4 UINT32_C(4)
#define GRANIT_VERTEX_FORMAT_UINT32 UINT32_C(5)
#define GRANIT_VERTEX_FORMAT_UINT32X2 UINT32_C(6)
#define GRANIT_VERTEX_FORMAT_UINT32X3 UINT32_C(7)
#define GRANIT_VERTEX_FORMAT_UINT32X4 UINT32_C(8)
#define GRANIT_VERTEX_FORMAT_SINT32 UINT32_C(9)
#define GRANIT_VERTEX_FORMAT_SINT32X2 UINT32_C(10)
#define GRANIT_VERTEX_FORMAT_SINT32X3 UINT32_C(11)
#define GRANIT_VERTEX_FORMAT_SINT32X4 UINT32_C(12)

typedef uint32_t granit_vertex_step_mode;
#define GRANIT_VERTEX_STEP_MODE_VERTEX UINT32_C(1)
#define GRANIT_VERTEX_STEP_MODE_INSTANCE UINT32_C(2)

typedef struct granit_vertex_attribute {
  uint32_t location;
  granit_vertex_format format;
  uint32_t offset;
  uint32_t reserved;
} granit_vertex_attribute;

/** 单个 Vertex Buffer binding 的步长、步进方式和 Attribute 列表。 */
typedef struct granit_vertex_buffer_layout {
  uint32_t stride;
  granit_vertex_step_mode step_mode;
  uint32_t attribute_count;
  uint32_t reserved;
  const granit_vertex_attribute* attributes;
} granit_vertex_buffer_layout;

typedef uint32_t granit_primitive_topology;
#define GRANIT_PRIMITIVE_TOPOLOGY_POINT_LIST UINT32_C(1)
#define GRANIT_PRIMITIVE_TOPOLOGY_LINE_LIST UINT32_C(2)
#define GRANIT_PRIMITIVE_TOPOLOGY_LINE_STRIP UINT32_C(3)
#define GRANIT_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST UINT32_C(4)
#define GRANIT_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP UINT32_C(5)

typedef uint32_t granit_front_face;
#define GRANIT_FRONT_FACE_COUNTER_CLOCKWISE UINT32_C(1)
#define GRANIT_FRONT_FACE_CLOCKWISE UINT32_C(2)

typedef uint32_t granit_cull_mode;
#define GRANIT_CULL_MODE_NONE UINT32_C(1)
#define GRANIT_CULL_MODE_FRONT UINT32_C(2)
#define GRANIT_CULL_MODE_BACK UINT32_C(3)
#define GRANIT_CULL_MODE_FRONT_AND_BACK UINT32_C(4)

typedef uint32_t granit_polygon_mode;
#define GRANIT_POLYGON_MODE_FILL UINT32_C(1)
#define GRANIT_POLYGON_MODE_LINE UINT32_C(2)
#define GRANIT_POLYGON_MODE_POINT UINT32_C(3)

/** 图元装配与光栅化状态。 */
typedef struct granit_primitive_state {
  granit_primitive_topology topology;
  granit_front_face front_face;
  granit_cull_mode cull_mode;
  granit_polygon_mode polygon_mode;
} granit_primitive_state;
#define GRANIT_PRIMITIVE_STATE_INIT                                                                \
  {GRANIT_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, GRANIT_FRONT_FACE_COUNTER_CLOCKWISE,                   \
   GRANIT_CULL_MODE_NONE, GRANIT_POLYGON_MODE_FILL}

typedef struct granit_depth_state {
  uint32_t test_enabled;
  uint32_t write_enabled;
  granit_compare_operation compare;
  uint32_t reserved;
} granit_depth_state;
#define GRANIT_DEPTH_STATE_INIT                                                                    \
  {UINT32_C(0), UINT32_C(0), GRANIT_COMPARE_OPERATION_LESS_EQUAL, UINT32_C(0)}

/** 固定光栅化深度偏移；指针为空表示禁用。 */
typedef struct granit_depth_bias_state {
  float constant_factor;
  float slope_factor;
  float clamp;
  uint32_t reserved;
} granit_depth_bias_state;
#define GRANIT_DEPTH_BIAS_STATE_INIT {0.0F, 0.0F, 0.0F, UINT32_C(0)}

typedef uint32_t granit_blend_factor;
#define GRANIT_BLEND_FACTOR_ZERO UINT32_C(1)
#define GRANIT_BLEND_FACTOR_ONE UINT32_C(2)
#define GRANIT_BLEND_FACTOR_SOURCE_COLOR UINT32_C(3)
#define GRANIT_BLEND_FACTOR_ONE_MINUS_SOURCE_COLOR UINT32_C(4)
#define GRANIT_BLEND_FACTOR_SOURCE_ALPHA UINT32_C(5)
#define GRANIT_BLEND_FACTOR_ONE_MINUS_SOURCE_ALPHA UINT32_C(6)
#define GRANIT_BLEND_FACTOR_DESTINATION_COLOR UINT32_C(7)
#define GRANIT_BLEND_FACTOR_ONE_MINUS_DESTINATION_COLOR UINT32_C(8)
#define GRANIT_BLEND_FACTOR_DESTINATION_ALPHA UINT32_C(9)
#define GRANIT_BLEND_FACTOR_ONE_MINUS_DESTINATION_ALPHA UINT32_C(10)

typedef uint32_t granit_blend_operation;
#define GRANIT_BLEND_OPERATION_ADD UINT32_C(1)
#define GRANIT_BLEND_OPERATION_SUBTRACT UINT32_C(2)
#define GRANIT_BLEND_OPERATION_REVERSE_SUBTRACT UINT32_C(3)
#define GRANIT_BLEND_OPERATION_MIN UINT32_C(4)
#define GRANIT_BLEND_OPERATION_MAX UINT32_C(5)

typedef uint32_t granit_color_write_mask;
#define GRANIT_COLOR_WRITE_RED_BIT (UINT32_C(1) << 0)
#define GRANIT_COLOR_WRITE_GREEN_BIT (UINT32_C(1) << 1)
#define GRANIT_COLOR_WRITE_BLUE_BIT (UINT32_C(1) << 2)
#define GRANIT_COLOR_WRITE_ALPHA_BIT (UINT32_C(1) << 3)
#define GRANIT_COLOR_WRITE_ALL_BITS                                                                \
  (GRANIT_COLOR_WRITE_RED_BIT | GRANIT_COLOR_WRITE_GREEN_BIT | GRANIT_COLOR_WRITE_BLUE_BIT |       \
   GRANIT_COLOR_WRITE_ALPHA_BIT)

typedef struct granit_color_blend_state {
  uint32_t enabled;
  granit_blend_factor source_color_factor;
  granit_blend_factor destination_color_factor;
  granit_blend_operation color_operation;
  granit_blend_factor source_alpha_factor;
  granit_blend_factor destination_alpha_factor;
  granit_blend_operation alpha_operation;
  granit_color_write_mask write_mask;
} granit_color_blend_state;
#define GRANIT_COLOR_BLEND_STATE_INIT                                                              \
  {UINT32_C(0),                                                                                    \
   GRANIT_BLEND_FACTOR_ONE,                                                                        \
   GRANIT_BLEND_FACTOR_ZERO,                                                                       \
   GRANIT_BLEND_OPERATION_ADD,                                                                     \
   GRANIT_BLEND_FACTOR_ONE,                                                                        \
   GRANIT_BLEND_FACTOR_ZERO,                                                                       \
   GRANIT_BLEND_OPERATION_ADD,                                                                     \
   GRANIT_COLOR_WRITE_ALL_BITS}

/** Dynamic Rendering 图形 Pipeline 描述；输入数组只在调用期间有效。 */
typedef struct granit_graphics_pipeline_desc {
  uint32_t struct_size;
  uint32_t reserved;
  granit_pipeline_layout layout;
  granit_shader vertex_shader;
  granit_shader fragment_shader;
  uint32_t color_format_count;
  const granit_texture_format* color_formats;
  granit_texture_format depth_stencil_format;
  granit_sample_count sample_count;
  uint32_t reserved_2;
  uint32_t vertex_buffer_layout_count;
  uint32_t reserved_3;
  const granit_vertex_buffer_layout* vertex_buffer_layouts;
  granit_primitive_state primitive;
  const granit_depth_state* depth;
  uint32_t color_blend_count;
  uint32_t reserved_4;
  const granit_color_blend_state* color_blends;
  const granit_depth_bias_state* depth_bias;
} granit_graphics_pipeline_desc;
#define GRANIT_GRAPHICS_PIPELINE_DESC_VERSION_1_SIZE                                               \
  ((uint32_t)(offsetof(granit_graphics_pipeline_desc, reserved_2) + sizeof(uint32_t)))
#define GRANIT_GRAPHICS_PIPELINE_DESC_VERSION_2_SIZE                                               \
  ((uint32_t)(offsetof(granit_graphics_pipeline_desc, vertex_buffer_layouts) + sizeof(void*)))
#define GRANIT_GRAPHICS_PIPELINE_DESC_VERSION_3_SIZE                                               \
  ((uint32_t)(offsetof(granit_graphics_pipeline_desc, primitive) + sizeof(granit_primitive_state)))
#define GRANIT_GRAPHICS_PIPELINE_DESC_VERSION_4_SIZE                                               \
  ((uint32_t)(offsetof(granit_graphics_pipeline_desc, color_blends) + sizeof(void*)))
#define GRANIT_GRAPHICS_PIPELINE_DESC_VERSION_5_SIZE                                               \
  ((uint32_t)(offsetof(granit_graphics_pipeline_desc, depth_bias) + sizeof(void*)))
#define GRANIT_GRAPHICS_PIPELINE_DESC_INIT                                                         \
  {(uint32_t)sizeof(granit_graphics_pipeline_desc),                                                \
   UINT32_C(0),                                                                                    \
   GRANIT_NULL_HANDLE,                                                                             \
   GRANIT_NULL_HANDLE,                                                                             \
   GRANIT_NULL_HANDLE,                                                                             \
   UINT32_C(0),                                                                                    \
   0,                                                                                              \
   GRANIT_TEXTURE_FORMAT_UNDEFINED,                                                                \
   GRANIT_SAMPLE_COUNT_1,                                                                          \
   UINT32_C(0),                                                                                    \
   UINT32_C(0),                                                                                    \
   UINT32_C(0),                                                                                    \
   0,                                                                                              \
   GRANIT_PRIMITIVE_STATE_INIT,                                                                    \
   0,                                                                                              \
   UINT32_C(0),                                                                                    \
   UINT32_C(0),                                                                                    \
   0,                                                                                              \
   0}

/** Compute Pipeline 描述。 */
typedef struct granit_compute_pipeline_desc {
  uint32_t struct_size;
  uint32_t reserved;
  granit_pipeline_layout layout;
  granit_shader compute_shader;
  uint64_t reserved_2;
} granit_compute_pipeline_desc;
#define GRANIT_COMPUTE_PIPELINE_DESC_VERSION_1_SIZE                                                \
  ((uint32_t)(offsetof(granit_compute_pipeline_desc, reserved_2) + sizeof(uint64_t)))
#define GRANIT_COMPUTE_PIPELINE_DESC_INIT                                                          \
  {GRANIT_COMPUTE_PIPELINE_DESC_VERSION_1_SIZE, UINT32_C(0), GRANIT_NULL_HANDLE,                   \
   GRANIT_NULL_HANDLE, UINT64_C(0)}

#ifdef __cplusplus
extern "C" {
#endif

GRANIT_API granit_result granit_bind_group_layout_create(granit_renderer renderer,
                                                         const granit_bind_group_layout_desc* desc,
                                                         granit_bind_group_layout* layout);
GRANIT_API granit_result granit_bind_group_layout_destroy(granit_renderer renderer,
                                                          granit_bind_group_layout layout);
GRANIT_API granit_result granit_bind_group_create(granit_renderer renderer,
                                                  const granit_bind_group_desc* desc,
                                                  granit_bind_group* bind_group);
GRANIT_API granit_result granit_bind_group_destroy(granit_renderer renderer,
                                                   granit_bind_group bind_group);
GRANIT_API granit_result granit_pipeline_layout_create(granit_renderer renderer,
                                                       const granit_pipeline_layout_desc* desc,
                                                       granit_pipeline_layout* layout);
GRANIT_API granit_result granit_pipeline_layout_destroy(granit_renderer renderer,
                                                        granit_pipeline_layout layout);
GRANIT_API granit_result granit_graphics_pipeline_create(granit_renderer renderer,
                                                         const granit_graphics_pipeline_desc* desc,
                                                         granit_graphics_pipeline* pipeline);
GRANIT_API granit_result granit_graphics_pipeline_destroy(granit_renderer renderer,
                                                          granit_graphics_pipeline pipeline);
GRANIT_API granit_result granit_compute_pipeline_create(granit_renderer renderer,
                                                        const granit_compute_pipeline_desc* desc,
                                                        granit_compute_pipeline* pipeline);
GRANIT_API granit_result granit_compute_pipeline_destroy(granit_renderer renderer,
                                                         granit_compute_pipeline pipeline);

#ifdef __cplusplus
}
#endif

#endif
