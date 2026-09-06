// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_CORE_ASYNC_OPERATION_STATE_H_
#define GRANIT_CORE_ASYNC_OPERATION_STATE_H_

#include <mutex>

#include <granit/renderer/async_operation.h>

namespace granit::detail {

/** 与后端无关的异步操作状态机。 */
class async_operation_state_machine {
public:
  [[nodiscard]] granit_async_operation_status status() const noexcept;
  [[nodiscard]] bool begin() noexcept;
  [[nodiscard]] bool request_cancel() noexcept;
  /** 后端已停止或忽略剩余工作时确认取消。 */
  void acknowledge_cancel() noexcept;
  void complete(granit_result result) noexcept;

private:
  [[nodiscard]] bool terminal() const noexcept;

  mutable std::mutex mutex_;
  granit_async_operation_state state_{GRANIT_ASYNC_OPERATION_STATE_PENDING};
  granit_result result_{GRANIT_ERROR_NOT_READY};
  bool cancel_requested_{};
};

} // namespace granit::detail

#endif
