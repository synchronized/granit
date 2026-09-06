// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_ASYNC_OPERATION_H_
#define GRANIT_ASYNC_OPERATION_H_

#include <stddef.h>
#include <stdint.h>

#include <granit/core/export.h>
#include <granit/core/result.h>
#include <granit/core/types.h>
#include <granit/renderer/renderer.h>

/** Granit 发起的 GPU 异步操作句柄。零值无效。 */
typedef granit_handle granit_async_operation;

typedef uint32_t granit_async_operation_state;
#define GRANIT_ASYNC_OPERATION_STATE_PENDING UINT32_C(1)
#define GRANIT_ASYNC_OPERATION_STATE_RUNNING UINT32_C(2)
#define GRANIT_ASYNC_OPERATION_STATE_SUCCEEDED UINT32_C(3)
#define GRANIT_ASYNC_OPERATION_STATE_FAILED UINT32_C(4)
#define GRANIT_ASYNC_OPERATION_STATE_CANCELLED UINT32_C(5)

/** 异步操作状态快照；仅终态的 result 表示最终结果。 */
typedef struct granit_async_operation_status {
  uint32_t struct_size;
  granit_async_operation_state state;
  granit_result result;
  uint32_t cancel_requested;
} granit_async_operation_status;

#define GRANIT_ASYNC_OPERATION_STATUS_VERSION_1_SIZE                                              \
  ((uint32_t)(offsetof(granit_async_operation_status, cancel_requested) + sizeof(uint32_t)))
#define GRANIT_ASYNC_OPERATION_STATUS_INIT                                                        \
  {(uint32_t)sizeof(granit_async_operation_status), GRANIT_ASYNC_OPERATION_STATE_PENDING,         \
   GRANIT_ERROR_NOT_READY, UINT32_C(0)}

#ifdef __cplusplus
extern "C" {
#endif

/** 非阻塞查询操作状态；未完成操作的 result 为 GRANIT_ERROR_NOT_READY。 */
GRANIT_API granit_result granit_async_operation_get_status(
    granit_renderer renderer, granit_async_operation operation,
    granit_async_operation_status* status);

/** 请求取消尚未开始的工作；已经提交给 GPU 的工作仍会安全排空。 */
GRANIT_API granit_result granit_async_operation_request_cancel(granit_renderer renderer,
                                                               granit_async_operation operation);

/** 销毁操作句柄；正在运行的后端工作仍由 Granit 保持到安全完成。 */
GRANIT_API granit_result granit_async_operation_destroy(granit_renderer renderer,
                                                        granit_async_operation operation);

#ifdef __cplusplus
}
#endif

#endif
