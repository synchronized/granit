// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_BACKEND_LIFECYCLE_H_
#define GRANIT_BACKEND_LIFECYCLE_H_

#include <atomic>
#include <cstdint>

#include <granit/core/result.h>

namespace granit::detail {

enum class backend_lifecycle_state : std::uint32_t { initializing, ready, failed, device_lost };

struct backend_lifecycle_status {
  backend_lifecycle_state state{backend_lifecycle_state::initializing};
  granit_result failure_result{GRANIT_SUCCESS};
};

/** 保存可并发查询的后端生命周期终态；失败和设备丢失一旦发生便保持终止。 */
class backend_lifecycle {
public:
  [[nodiscard]] backend_lifecycle_status status() const noexcept {
    return unpack(value_.load());
  }

  [[nodiscard]] granit_result gate() const noexcept {
    const auto snapshot = status();
    switch (snapshot.state) {
    case backend_lifecycle_state::initializing:
      return GRANIT_ERROR_NOT_READY;
    case backend_lifecycle_state::ready:
      return GRANIT_SUCCESS;
    case backend_lifecycle_state::failed:
    case backend_lifecycle_state::device_lost:
      return snapshot.failure_result;
    }
    return GRANIT_ERROR_INTERNAL;
  }

  void mark_ready() noexcept {
    auto expected = pack({});
    static_cast<void>(value_.compare_exchange_strong(
        expected, pack({backend_lifecycle_state::ready, GRANIT_SUCCESS})));
  }

  void mark_failed(granit_result result) noexcept {
    if (result == GRANIT_SUCCESS)
      result = GRANIT_ERROR_INITIALIZATION_FAILED;
    auto current = value_.load();
    while (true) {
      const auto snapshot = unpack(current);
      if (snapshot.state == backend_lifecycle_state::failed ||
          snapshot.state == backend_lifecycle_state::device_lost)
        return;
      if (value_.compare_exchange_weak(
              current, pack({backend_lifecycle_state::failed, result})))
        return;
    }
  }

  void mark_device_lost() noexcept {
    value_.store(pack({backend_lifecycle_state::device_lost, GRANIT_ERROR_DEVICE_LOST}));
  }

private:
  static constexpr std::uint64_t pack(backend_lifecycle_status status) noexcept {
    return (static_cast<std::uint64_t>(status.state) << 32) |
           static_cast<std::uint32_t>(status.failure_result);
  }

  static constexpr backend_lifecycle_status unpack(std::uint64_t value) noexcept {
    return {static_cast<backend_lifecycle_state>(value >> 32),
            static_cast<granit_result>(static_cast<std::int32_t>(value))};
  }

  std::atomic_uint64_t value_{pack({})};
};

} // namespace granit::detail

#endif
