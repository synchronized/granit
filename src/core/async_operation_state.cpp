// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "core/async_operation_state.h"

namespace granit::detail {

bool async_operation_state_machine::terminal() const noexcept {
  return state_ == GRANIT_ASYNC_OPERATION_STATE_SUCCEEDED ||
         state_ == GRANIT_ASYNC_OPERATION_STATE_FAILED ||
         state_ == GRANIT_ASYNC_OPERATION_STATE_CANCELLED;
}

granit_async_operation_status async_operation_state_machine::status() const noexcept {
  std::lock_guard lock{mutex_};
  return {sizeof(granit_async_operation_status), state_, result_, cancel_requested_ ? 1U : 0U};
}

bool async_operation_state_machine::begin() noexcept {
  std::lock_guard lock{mutex_};
  if (state_ != GRANIT_ASYNC_OPERATION_STATE_PENDING)
    return false;
  if (cancel_requested_) {
    state_ = GRANIT_ASYNC_OPERATION_STATE_CANCELLED;
    result_ = GRANIT_ERROR_CANCELLED;
    return false;
  }
  state_ = GRANIT_ASYNC_OPERATION_STATE_RUNNING;
  return true;
}

bool async_operation_state_machine::request_cancel() noexcept {
  std::lock_guard lock{mutex_};
  if (terminal())
    return false;
  cancel_requested_ = true;
  if (state_ == GRANIT_ASYNC_OPERATION_STATE_PENDING) {
    state_ = GRANIT_ASYNC_OPERATION_STATE_CANCELLED;
    result_ = GRANIT_ERROR_CANCELLED;
  }
  return true;
}

void async_operation_state_machine::complete(granit_result result) noexcept {
  std::lock_guard lock{mutex_};
  if (terminal())
    return;
  state_ = result == GRANIT_SUCCESS ? GRANIT_ASYNC_OPERATION_STATE_SUCCEEDED
                                    : GRANIT_ASYNC_OPERATION_STATE_FAILED;
  result_ = result;
}

} // namespace granit::detail
