// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_COMMAND_RECORDER_H_
#define GRANIT_COMMAND_RECORDER_H_

#include <stdint.h>

#include <granit/buffer.h>
#include <granit/export.h>
#include <granit/render_target.h>
#include <granit/renderer.h>
#include <granit/result.h>
#include <granit/swapchain.h>
#include <granit/types.h>

/** 一次录制、一次提交的命令录制器句柄。零值无效。 */
typedef granit_handle granit_command_recorder;

/** 单个 Buffer 复制区域，所有偏移和大小均以字节为单位。 */
typedef struct granit_buffer_copy_region {
  uint64_t source_offset;
  uint64_t destination_offset;
  uint64_t size;
} granit_buffer_copy_region;

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
