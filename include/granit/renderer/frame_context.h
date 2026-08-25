// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_FRAME_CONTEXT_H_
#define GRANIT_FRAME_CONTEXT_H_

#include <stdint.h>

#include <granit/core/export.h>
#include <granit/core/result.h>
#include <granit/core/types.h>
#include <granit/renderer/command_recorder.h>
#include <granit/renderer/renderer.h>
#include <granit/renderer/swapchain.h>

/** 按 Renderer 真实在途帧槽轮转 Command Recorder 的上下文句柄。 */
typedef granit_handle granit_frame_context;

typedef struct granit_frame_context_desc {
  uint32_t struct_size;
  uint32_t flags;
  uint64_t reserved;
} granit_frame_context_desc;

#define GRANIT_FRAME_CONTEXT_DESC_VERSION_1_SIZE UINT32_C(16)
#define GRANIT_FRAME_CONTEXT_DESC_INIT                                                             \
  {GRANIT_FRAME_CONTEXT_DESC_VERSION_1_SIZE, UINT32_C(0), UINT64_C(0)}

#ifdef __cplusplus
extern "C" {
#endif

GRANIT_API granit_result granit_frame_context_create(granit_renderer renderer,
                                                     const granit_frame_context_desc* desc,
                                                     granit_frame_context* context);
/** 为 Frame 的真实槽位开始录制；返回的 Recorder 由 Context 拥有，只能借用。 */
GRANIT_API granit_result granit_frame_context_begin(granit_renderer renderer,
                                                    granit_frame_context context,
                                                    granit_frame frame,
                                                    granit_command_recorder* recorder,
                                                    uint32_t* frame_slot);
/** 结束并提交当前 Frame 对应槽位的 Recorder；不执行 present。 */
GRANIT_API granit_result granit_frame_context_submit(granit_renderer renderer,
                                                     granit_frame_context context,
                                                     granit_frame frame);
/** 放弃尚未提交的录制并重建对应 Recorder；不取消 Frame。 */
GRANIT_API granit_result granit_frame_context_abort(granit_renderer renderer,
                                                    granit_frame_context context,
                                                    granit_frame frame);
/** 先使 Context 句柄失效，再等待并销毁其全部 Recorder。 */
GRANIT_API granit_result granit_frame_context_destroy(granit_renderer renderer,
                                                      granit_frame_context context);

#ifdef __cplusplus
}
#endif

#endif
