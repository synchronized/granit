// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_ASYNC_OPERATION_HPP_
#define GRANIT_ASYNC_OPERATION_HPP_

#include <utility>

#include <granit/core/result.hpp>
#include <granit/renderer/async_operation.h>

namespace granit {

enum class async_operation_state : std::uint32_t {
  pending = GRANIT_ASYNC_OPERATION_STATE_PENDING,
  running = GRANIT_ASYNC_OPERATION_STATE_RUNNING,
  succeeded = GRANIT_ASYNC_OPERATION_STATE_SUCCEEDED,
  failed = GRANIT_ASYNC_OPERATION_STATE_FAILED,
  cancelled = GRANIT_ASYNC_OPERATION_STATE_CANCELLED,
};

struct async_operation_status {
  async_operation_state state{async_operation_state::pending};
  result operation_result{result::not_ready};
  bool cancel_requested{};

  [[nodiscard]] bool complete() const noexcept {
    return state == async_operation_state::succeeded || state == async_operation_state::failed ||
           state == async_operation_state::cancelled;
  }
};

class async_operation {
public:
  async_operation() = default;
  async_operation(granit_renderer renderer, granit_async_operation handle) noexcept
      : renderer_(renderer), handle_(handle) {}
  ~async_operation() { static_cast<void>(reset()); }
  async_operation(const async_operation&) = delete;
  async_operation& operator=(const async_operation&) = delete;
  async_operation(async_operation&& other) noexcept
      : renderer_(std::exchange(other.renderer_, GRANIT_NULL_HANDLE)),
        handle_(std::exchange(other.handle_, GRANIT_NULL_HANDLE)) {}
  async_operation& operator=(async_operation&& other) noexcept {
    if (this != &other) {
      static_cast<void>(reset());
      renderer_ = std::exchange(other.renderer_, GRANIT_NULL_HANDLE);
      handle_ = std::exchange(other.handle_, GRANIT_NULL_HANDLE);
    }
    return *this;
  }

  [[nodiscard]] result get_status(async_operation_status& status) const noexcept {
    granit_async_operation_status native = GRANIT_ASYNC_OPERATION_STATUS_INIT;
    const auto query_result = from_native(
        granit_async_operation_get_status(renderer_, handle_, &native));
    if (!query_result)
      return query_result;
    status = {.state = static_cast<async_operation_state>(native.state),
              .operation_result = from_native(native.result),
              .cancel_requested = native.cancel_requested != 0};
    return result::success;
  }
  [[nodiscard]] result request_cancel() noexcept {
    return from_native(granit_async_operation_request_cancel(renderer_, handle_));
  }
  [[nodiscard]] result reset() noexcept {
    if (!valid())
      return result::success;
    const auto renderer = std::exchange(renderer_, GRANIT_NULL_HANDLE);
    const auto handle = std::exchange(handle_, GRANIT_NULL_HANDLE);
    return from_native(granit_async_operation_destroy(renderer, handle));
  }
  [[nodiscard]] bool valid() const noexcept { return handle_ != GRANIT_NULL_HANDLE; }
  [[nodiscard]] granit_renderer native_renderer() const noexcept { return renderer_; }
  [[nodiscard]] granit_async_operation native_handle() const noexcept { return handle_; }

private:
  granit_renderer renderer_{GRANIT_NULL_HANDLE};
  granit_async_operation handle_{GRANIT_NULL_HANDLE};
};

} // namespace granit

#endif
