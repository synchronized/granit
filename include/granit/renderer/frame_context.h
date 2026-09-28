// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_FRAME_CONTEXT_H_
#define GRANIT_FRAME_CONTEXT_H_

#include <stdint.h>

#include <granit/core/export.h>
#include <granit/core/result.h>
#include <granit/core/types.h>
#include <granit/renderer/buffer.h>
#include <granit/renderer/command_recorder.h>
#include <granit/renderer/frame.h>
#include <granit/renderer/renderer.h>

/** 按 Renderer 真实在途帧槽轮转 Command Recorder 的上下文句柄。 */
typedef granit_handle granit_frame_context;

/** 帧录制期临时 Buffer 分配描述。 */
typedef struct granit_transient_buffer_desc {
  uint32_t struct_size;
  granit_buffer_usage usage;
  granit_memory_location memory_location;
  uint32_t reserved;
  uint64_t size;
  uint64_t alignment;
} granit_transient_buffer_desc;

#define GRANIT_TRANSIENT_BUFFER_DESC_VERSION_1_SIZE UINT32_C(32)
#define GRANIT_TRANSIENT_BUFFER_DESC_INIT                                                          \
  {GRANIT_TRANSIENT_BUFFER_DESC_VERSION_1_SIZE,                                                    \
   UINT32_C(0),                                                                                    \
   GRANIT_MEMORY_LOCATION_DEVICE,                                                                  \
   UINT32_C(0),                                                                                    \
   UINT64_C(0),                                                                                    \
   UINT64_C(1)}

/** 借用的帧临时 Buffer 区间；只能在产生它的 Frame Recording 内使用。 */
typedef struct granit_transient_buffer_slice {
  granit_buffer buffer;
  uint64_t offset;
  uint64_t size;
  granit_buffer_usage usage;
  uint32_t reserved;
} granit_transient_buffer_slice;

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

/** 查询存活 Frame 的真实帧槽；返回信息不延长 Frame 生命周期。 */
GRANIT_API granit_result granit_frame_get_info(granit_renderer renderer, granit_frame frame,
                                               granit_frame_info* info);
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
GRANIT_API granit_result granit_frame_context_allocate_transient_buffer(
    granit_renderer renderer, granit_frame_context context, granit_frame frame,
    const granit_transient_buffer_desc* desc, granit_transient_buffer_slice* slice);
/** 先使 Context 句柄失效，再等待并销毁其全部 Recorder。 */
GRANIT_API granit_result granit_frame_context_destroy(granit_renderer renderer,
                                                      granit_frame_context context);

#ifdef __cplusplus
}
#endif

#endif
