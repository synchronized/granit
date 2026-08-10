// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_COMMAND_RECORDER_H_
#define GRANIT_COMMAND_RECORDER_H_

#include <stdint.h>

#include <granit/renderer/buffer.h>
#include <granit/core/export.h>
#include <granit/renderer/pipeline.h>
#include <granit/renderer/render_target.h>
#include <granit/renderer/renderer.h>
#include <granit/core/result.h>
#include <granit/renderer/swapchain.h>
#include <granit/core/types.h>

/** 一次录制、一次提交的命令录制器句柄。零值无效。 */
typedef granit_handle granit_command_recorder;

/** 单个 Buffer 复制区域，所有偏移和大小均以字节为单位。 */
typedef struct granit_buffer_copy_region {
  uint64_t source_offset;
  uint64_t destination_offset;
  uint64_t size;
} granit_buffer_copy_region;

typedef struct granit_viewport {
  float x;
  float y;
  float width;
  float height;
  float min_depth;
  float max_depth;
} granit_viewport;

typedef struct granit_scissor {
  int32_t x;
  int32_t y;
  uint32_t width;
  uint32_t height;
} granit_scissor;

typedef struct granit_vertex_buffer_binding {
  granit_buffer buffer;
  uint64_t offset;
} granit_vertex_buffer_binding;

typedef uint32_t granit_index_type;
#define GRANIT_INDEX_TYPE_UINT16 UINT32_C(1)
#define GRANIT_INDEX_TYPE_UINT32 UINT32_C(2)

typedef struct granit_command_recorder_desc {
  uint32_t struct_size;
  uint32_t flags;
  uint64_t reserved;
} granit_command_recorder_desc;
#define GRANIT_COMMAND_RECORDER_DESC_VERSION_1_SIZE UINT32_C(16)
#define GRANIT_COMMAND_RECORDER_DESC_INIT                                                          \
  {GRANIT_COMMAND_RECORDER_DESC_VERSION_1_SIZE, UINT32_C(0), UINT64_C(0)}

#ifdef __cplusplus
extern "C" {
#endif

GRANIT_API granit_result granit_command_recorder_create(granit_renderer renderer,
                                                        const granit_command_recorder_desc* desc,
                                                        granit_command_recorder* recorder);
GRANIT_API granit_result granit_command_recorder_begin(granit_renderer renderer,
                                                       granit_command_recorder recorder);
GRANIT_API granit_result granit_command_recorder_end(granit_renderer renderer,
                                                     granit_command_recorder recorder);
/** 异步提交 executable Recorder；同一 Recorder 完成前不能再次提交。 */
GRANIT_API granit_result granit_command_recorder_submit(granit_renderer renderer,
                                                        granit_command_recorder recorder);
GRANIT_API granit_result granit_command_recorder_submit_frame(granit_renderer renderer,
                                                              granit_command_recorder recorder,
                                                              granit_frame frame);
GRANIT_API granit_result granit_command_recorder_reset(granit_renderer renderer,
                                                       granit_command_recorder recorder);
GRANIT_API granit_result granit_command_recorder_copy_buffer(
    granit_renderer renderer, granit_command_recorder recorder, granit_buffer source,
    granit_buffer destination, const granit_buffer_copy_region* regions, uint32_t region_count);
GRANIT_API granit_result granit_command_recorder_fill_buffer(granit_renderer renderer,
                                                             granit_command_recorder recorder,
                                                             granit_buffer buffer, uint64_t offset,
                                                             uint64_t size, uint32_t value);
GRANIT_API granit_result granit_command_recorder_bind_graphics_pipeline(
    granit_renderer renderer, granit_command_recorder recorder, granit_graphics_pipeline pipeline);
GRANIT_API granit_result granit_command_recorder_bind_graphics_groups(
    granit_renderer renderer, granit_command_recorder recorder, granit_pipeline_layout layout,
    uint32_t first_group, const granit_bind_group* bind_groups, uint32_t bind_group_count);
GRANIT_API granit_result granit_command_recorder_bind_compute_pipeline(
    granit_renderer renderer, granit_command_recorder recorder, granit_compute_pipeline pipeline);
GRANIT_API granit_result granit_command_recorder_bind_compute_groups(
    granit_renderer renderer, granit_command_recorder recorder, granit_pipeline_layout layout,
    uint32_t first_group, const granit_bind_group* bind_groups, uint32_t bind_group_count);
GRANIT_API granit_result granit_command_recorder_dispatch(granit_renderer renderer,
                                                          granit_command_recorder recorder,
                                                          uint32_t group_count_x,
                                                          uint32_t group_count_y,
                                                          uint32_t group_count_z);
GRANIT_API granit_result granit_command_recorder_set_viewports(granit_renderer renderer,
                                                               granit_command_recorder recorder,
                                                               uint32_t first_viewport,
                                                               const granit_viewport* viewports,
                                                               uint32_t viewport_count);
GRANIT_API granit_result granit_command_recorder_set_scissors(granit_renderer renderer,
                                                              granit_command_recorder recorder,
                                                              uint32_t first_scissor,
                                                              const granit_scissor* scissors,
                                                              uint32_t scissor_count);
GRANIT_API granit_result granit_command_recorder_bind_vertex_buffers(
    granit_renderer renderer, granit_command_recorder recorder, uint32_t first_binding,
    const granit_vertex_buffer_binding* bindings, uint32_t binding_count);
GRANIT_API granit_result granit_command_recorder_bind_index_buffer(granit_renderer renderer,
                                                                   granit_command_recorder recorder,
                                                                   granit_buffer buffer,
                                                                   uint64_t offset,
                                                                   granit_index_type index_type);
GRANIT_API granit_result granit_command_recorder_draw(
    granit_renderer renderer, granit_command_recorder recorder, uint32_t vertex_count,
    uint32_t instance_count, uint32_t first_vertex, uint32_t first_instance);
GRANIT_API granit_result granit_command_recorder_draw_indexed(
    granit_renderer renderer, granit_command_recorder recorder, uint32_t index_count,
    uint32_t instance_count, uint32_t first_index, int32_t vertex_offset, uint32_t first_instance);
GRANIT_API granit_result granit_command_recorder_begin_rendering(granit_renderer renderer,
                                                                 granit_command_recorder recorder,
                                                                 const granit_rendering_desc* desc);
GRANIT_API granit_result granit_command_recorder_end_rendering(granit_renderer renderer,
                                                               granit_command_recorder recorder);
GRANIT_API granit_result granit_command_recorder_destroy(granit_renderer renderer,
                                                         granit_command_recorder recorder);

#ifdef __cplusplus
}
#endif

#endif
