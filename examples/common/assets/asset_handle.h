// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_EXAMPLES_COMMON_ASSETS_ASSET_HANDLE_H_
#define GRANIT_EXAMPLES_COMMON_ASSETS_ASSET_HANDLE_H_

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>

namespace granit::example::assets {

enum class asset_status {
  queued,
  reading,
  discovering_dependencies,
  loading_dependencies,
  decoding,
  ready,
  failed,
  cancelled
};
enum class asset_stage {
  queued,
  reading,
  discovering_dependencies,
  loading_dependencies,
  decoding,
  complete
};
enum class asset_error {
  none,
  invalid_location,
  source_not_registered,
  loader_not_registered,
  io_error,
  transport_error,
  invalid_data,
  out_of_memory,
  cancelled,
  internal,
};

struct asset_progress {
  asset_stage stage{asset_stage::queued};
  std::uint64_t completed_bytes{};
  std::optional<std::uint64_t> total_bytes;
  std::uint32_t completed_dependencies{};
  std::optional<std::uint32_t> total_dependencies;
  std::optional<float> fraction;
};

namespace detail {

struct asset_observer {
  std::atomic_bool cancelled{};
};

struct asset_state {
  mutable std::mutex mutex;
  std::atomic<asset_status> status{asset_status::queued};
  asset_progress progress;
  asset_error error{asset_error::none};
  std::string diagnostic;
  std::shared_ptr<const void> value;
};

} // namespace detail

/** 共享底层请求的强类型只读观察句柄。 */
template <typename T> class asset_handle {
public:
  asset_handle() = default;

  [[nodiscard]] bool valid() const noexcept { return state_ != nullptr && observer_ != nullptr; }
  [[nodiscard]] explicit operator bool() const noexcept { return valid(); }
  [[nodiscard]] asset_status status() const noexcept {
    if (!valid() || observer_->cancelled.load(std::memory_order_acquire))
      return asset_status::cancelled;
    return state_->status.load(std::memory_order_acquire);
  }
  [[nodiscard]] bool ready() const noexcept { return status() == asset_status::ready; }
  void cancel() noexcept {
    if (observer_)
      observer_->cancelled.store(true, std::memory_order_release);
  }
  [[nodiscard]] asset_progress progress() const noexcept {
    if (!state_)
      return {};
    std::scoped_lock lock{state_->mutex};
    return state_->progress;
  }
  [[nodiscard]] asset_error error() const noexcept {
    if (!state_)
      return asset_error::cancelled;
    std::scoped_lock lock{state_->mutex};
    return state_->error;
  }
  [[nodiscard]] std::string diagnostic() const {
    if (!state_)
      return {};
    std::scoped_lock lock{state_->mutex};
    return state_->diagnostic;
  }
  [[nodiscard]] std::shared_ptr<const T> value() const noexcept {
    if (!ready())
      return {};
    std::scoped_lock lock{state_->mutex};
    return std::static_pointer_cast<const T>(state_->value);
  }

private:
  friend class asset_group;
  friend class asset_manager;
  asset_handle(std::shared_ptr<detail::asset_state> state,
               std::shared_ptr<detail::asset_observer> observer) noexcept
      : state_(std::move(state)), observer_(std::move(observer)) {}

  std::shared_ptr<detail::asset_state> state_;
  std::shared_ptr<detail::asset_observer> observer_;
};

} // namespace granit::example::assets

#endif
