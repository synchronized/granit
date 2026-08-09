// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_COMMAND_RECORDER_HPP_
#define GRANIT_COMMAND_RECORDER_HPP_

#include <utility>

#include <granit/command_recorder.h>
#include <granit/result.hpp>

namespace granit {

/** 无异常、move-only 的 Command Recorder 包装。 */
class command_recorder {
public:
  command_recorder() = default;
  ~command_recorder() { static_cast<void>(destroy()); }
  command_recorder(const command_recorder&) = delete;
  command_recorder& operator=(const command_recorder&) = delete;
  command_recorder(command_recorder&& other) noexcept
      : renderer_(std::exchange(other.renderer_, GRANIT_NULL_HANDLE)),
        handle_(std::exchange(other.handle_, GRANIT_NULL_HANDLE)) {}
  command_recorder& operator=(command_recorder&& other) noexcept {
    if (this != &other) {
      static_cast<void>(destroy());
      renderer_ = std::exchange(other.renderer_, GRANIT_NULL_HANDLE);
      handle_ = std::exchange(other.handle_, GRANIT_NULL_HANDLE);
    }
    return *this;
  }

  [[nodiscard]] result initialize(granit_renderer renderer) noexcept {
    if (valid() || renderer == GRANIT_NULL_HANDLE) {
      return result::invalid_argument;
    }
    const granit_command_recorder_desc desc = GRANIT_COMMAND_RECORDER_DESC_INIT;
    const auto value = granit_command_recorder_create(renderer, &desc, &handle_);
    if (value == GRANIT_SUCCESS) {
      renderer_ = renderer;
    }
    return from_native(value);
  }
  [[nodiscard]] result begin() noexcept {
    return from_native(granit_command_recorder_begin(renderer_, handle_));
  }
  [[nodiscard]] result end() noexcept {
    return from_native(granit_command_recorder_end(renderer_, handle_));
  }
  [[nodiscard]] result reset() noexcept {
    return from_native(granit_command_recorder_reset(renderer_, handle_));
  }
  [[nodiscard]] result destroy() noexcept {
    if (!valid()) {
      return result::success;
    }
    const auto renderer = std::exchange(renderer_, GRANIT_NULL_HANDLE);
    const auto handle = std::exchange(handle_, GRANIT_NULL_HANDLE);
    return from_native(granit_command_recorder_destroy(renderer, handle));
  }
  [[nodiscard]] bool valid() const noexcept { return handle_ != GRANIT_NULL_HANDLE; }
  [[nodiscard]] explicit operator bool() const noexcept { return valid(); }
  [[nodiscard]] granit_command_recorder native_handle() const noexcept { return handle_; }

private:
  granit_renderer renderer_{GRANIT_NULL_HANDLE};
  granit_command_recorder handle_{GRANIT_NULL_HANDLE};
};

} // namespace granit

#endif
