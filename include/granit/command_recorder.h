// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_COMMAND_RECORDER_H_
#define GRANIT_COMMAND_RECORDER_H_

#include <stdint.h>

#include <granit/export.h>
#include <granit/renderer.h>
#include <granit/result.h>
#include <granit/types.h>

/** 一次录制、一次提交的命令录制器句柄。零值无效。 */
typedef granit_handle granit_command_recorder;

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
GRANIT_API granit_result granit_command_recorder_reset(granit_renderer renderer,
                                                       granit_command_recorder recorder);
GRANIT_API granit_result granit_command_recorder_destroy(granit_renderer renderer,
                                                         granit_command_recorder recorder);

#ifdef __cplusplus
}
#endif

#endif
