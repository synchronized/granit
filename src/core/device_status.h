// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_CORE_DEVICE_STATUS_H_
#define GRANIT_CORE_DEVICE_STATUS_H_

#include <atomic>

#include <granit/core/result.h>

namespace granit::detail {

/** 保存 Renderer 的粘滞 Device Lost 状态。 */
class device_status {
public:
  [[nodiscard]] granit_result gate() const noexcept {
    return lost_.load() ? GRANIT_ERROR_DEVICE_LOST : GRANIT_SUCCESS;
  }

  [[nodiscard]] granit_result observe(granit_result result, bool* first_loss = nullptr) noexcept {
    bool first = false;
    if (result == GRANIT_ERROR_DEVICE_LOST)
      first = !lost_.exchange(true);
    if (first_loss != nullptr)
      *first_loss = first;
    return gate() == GRANIT_ERROR_DEVICE_LOST ? GRANIT_ERROR_DEVICE_LOST : result;
  }

private:
  std::atomic_bool lost_{};
};

} // namespace granit::detail

#endif
