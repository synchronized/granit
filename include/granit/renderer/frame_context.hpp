// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_FRAME_CONTEXT_HPP_
#define GRANIT_FRAME_CONTEXT_HPP_

#include <cstdint>
#include <utility>

#include <granit/core/result.hpp>
#include <granit/renderer/command_recorder.hpp>
#include <granit/renderer/frame_context.h>
#include <granit/renderer/swapchain.hpp>

namespace granit {

/** 不借用 Swapchain 查询已获取 Frame 的真实槽位。 */
[[nodiscard]] inline result get_frame_slot_info(granit_renderer renderer,
                                                const acquired_frame& frame,
                                                granit_frame_info& info) noexcept {
  if (!frame.valid())
    return result::invalid_handle;
  info = GRANIT_FRAME_INFO_INIT;
  return from_native(granit_frame_get_slot_info(renderer, frame.handle, &info));
}

class frame_recording {
public:
  frame_recording() = default;
  ~frame_recording() { static_cast<void>(abort()); }
  frame_recording(const frame_recording&) = delete;
  frame_recording& operator=(const frame_recording&) = delete;
  frame_recording(frame_recording&& other) noexcept
      : renderer_(std::exchange(other.renderer_, GRANIT_NULL_HANDLE)),
        context_(std::exchange(other.context_, GRANIT_NULL_HANDLE)),
        frame_(std::exchange(other.frame_, GRANIT_NULL_HANDLE)),
        recorder_(std::move(other.recorder_)), frame_slot_(other.frame_slot_) {}
  frame_recording& operator=(frame_recording&&) = delete;

  /** 显式结束并提交录制；成功后对象失效，但 Frame 仍须由 Swapchain present。 */
  [[nodiscard]] result submit() noexcept {
    if (!valid())
      return result::invalid_argument;
    const auto value = granit_frame_context_submit(renderer_, context_, frame_);
    if (value == GRANIT_SUCCESS)
      release();
    return from_native(value);
  }

  /** 放弃未提交录制；不取消对应 Frame。 */
  [[nodiscard]] result abort() noexcept {
    if (!valid())
      return result::success;
    const auto value = granit_frame_context_abort(renderer_, context_, frame_);
    if (value == GRANIT_SUCCESS || value == GRANIT_ERROR_INVALID_HANDLE)
      release();
    return from_native(value);
  }

  [[nodiscard]] command_recorder& recorder() noexcept { return recorder_; }
  [[nodiscard]] const command_recorder& recorder() const noexcept { return recorder_; }
  [[nodiscard]] std::uint32_t frame_slot() const noexcept { return frame_slot_; }
  [[nodiscard]] bool valid() const noexcept { return frame_ != GRANIT_NULL_HANDLE; }
  [[nodiscard]] explicit operator bool() const noexcept { return valid(); }

private:
  friend class frame_context;

  void release() noexcept {
    static_cast<void>(recorder_.destroy());
    renderer_ = GRANIT_NULL_HANDLE;
    context_ = GRANIT_NULL_HANDLE;
    frame_ = GRANIT_NULL_HANDLE;
  }

  granit_renderer renderer_{GRANIT_NULL_HANDLE};
  granit_frame_context context_{GRANIT_NULL_HANDLE};
  granit_frame frame_{GRANIT_NULL_HANDLE};
  command_recorder recorder_;
  std::uint32_t frame_slot_{};
};

class frame_context {
public:
  frame_context() = default;
  ~frame_context() { static_cast<void>(reset()); }
  frame_context(const frame_context&) = delete;
  frame_context& operator=(const frame_context&) = delete;
  frame_context(frame_context&& other) noexcept
      : renderer_(std::exchange(other.renderer_, GRANIT_NULL_HANDLE)),
        handle_(std::exchange(other.handle_, GRANIT_NULL_HANDLE)) {}
  frame_context& operator=(frame_context&& other) noexcept {
    if (this != &other) {
      static_cast<void>(reset());
      renderer_ = std::exchange(other.renderer_, GRANIT_NULL_HANDLE);
      handle_ = std::exchange(other.handle_, GRANIT_NULL_HANDLE);
    }
    return *this;
  }

  [[nodiscard]] result initialize(granit_renderer renderer) noexcept {
    if (valid())
      return result::invalid_argument;
    if (renderer == GRANIT_NULL_HANDLE)
      return result::invalid_handle;
    const granit_frame_context_desc desc = GRANIT_FRAME_CONTEXT_DESC_INIT;
    const auto value = granit_frame_context_create(renderer, &desc, &handle_);
    if (value == GRANIT_SUCCESS)
      renderer_ = renderer;
    return from_native(value);
  }

  [[nodiscard]] result begin(const acquired_frame& frame, frame_recording& recording) noexcept {
    if (!valid() || !frame.valid() || recording.valid())
      return result::invalid_argument;
    granit_command_recorder recorder{};
    std::uint32_t frame_slot{};
    const auto value =
        granit_frame_context_begin(renderer_, handle_, frame.handle, &recorder, &frame_slot);
    if (value == GRANIT_SUCCESS) {
      recording.renderer_ = renderer_;
      recording.context_ = handle_;
      recording.frame_ = frame.handle;
      recording.recorder_ = command_recorder::borrow(renderer_, recorder);
      recording.frame_slot_ = frame_slot;
    }
    return from_native(value);
  }

  [[nodiscard]] result reset() noexcept {
    if (!valid())
      return result::success;
    const auto renderer = std::exchange(renderer_, GRANIT_NULL_HANDLE);
    const auto handle = std::exchange(handle_, GRANIT_NULL_HANDLE);
    return from_native(granit_frame_context_destroy(renderer, handle));
  }

  [[nodiscard]] bool valid() const noexcept { return handle_ != GRANIT_NULL_HANDLE; }
  [[nodiscard]] explicit operator bool() const noexcept { return valid(); }
  [[nodiscard]] granit_frame_context native_handle() const noexcept { return handle_; }

private:
  granit_renderer renderer_{GRANIT_NULL_HANDLE};
  granit_frame_context handle_{GRANIT_NULL_HANDLE};
};

} // namespace granit

#endif
