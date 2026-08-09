// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_COMMAND_RECORDER_HPP_
#define GRANIT_COMMAND_RECORDER_HPP_

#include <array>
#include <span>
#include <utility>

#include <granit/command_recorder.h>
#include <granit/render_target.hpp>
#include <granit/result.hpp>
#include <granit/swapchain.hpp>

namespace granit {

using buffer_copy_region = granit_buffer_copy_region;

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
  [[nodiscard]] result submit() noexcept {
    return from_native(granit_command_recorder_submit(renderer_, handle_));
  }
  [[nodiscard]] result submit(const acquired_frame& frame) noexcept {
    return from_native(granit_command_recorder_submit_frame(renderer_, handle_, frame.handle));
  }
  [[nodiscard]] result reset() noexcept {
    return from_native(granit_command_recorder_reset(renderer_, handle_));
  }
  [[nodiscard]] result copy_buffer(granit_buffer source, granit_buffer destination,
                                   std::span<const buffer_copy_region> regions) noexcept {
    if (regions.size() > UINT32_MAX) {
      return result::invalid_argument;
    }
    return from_native(
        granit_command_recorder_copy_buffer(renderer_, handle_, source, destination, regions.data(),
                                            static_cast<std::uint32_t>(regions.size())));
  }
  [[nodiscard]] result fill_buffer(granit_buffer buffer, std::uint64_t offset, std::uint64_t size,
                                   std::uint32_t value) noexcept {
    return from_native(
        granit_command_recorder_fill_buffer(renderer_, handle_, buffer, offset, size, value));
  }
  [[nodiscard]] result begin_rendering(const rendering_desc& desc) noexcept {
    if (desc.color_attachments.size() > GRANIT_MAX_COLOR_ATTACHMENTS) {
      return result::invalid_argument;
    }
    std::array<granit_color_attachment_desc, GRANIT_MAX_COLOR_ATTACHMENTS> colors{};
    for (std::size_t index = 0; index < desc.color_attachments.size(); ++index) {
      colors[index] = desc.color_attachments[index].native();
    }
    granit_depth_stencil_attachment_desc depth{};
    const granit_depth_stencil_attachment_desc* depth_pointer = nullptr;
    if (desc.depth_stencil_attachment != nullptr) {
      depth = desc.depth_stencil_attachment->native();
      depth_pointer = &depth;
    }
    const granit_rendering_desc native{
        .struct_size = GRANIT_RENDERING_DESC_VERSION_1_SIZE,
        .color_attachment_count = static_cast<std::uint32_t>(desc.color_attachments.size()),
        .color_attachments = colors.data(),
        .depth_stencil_attachment = depth_pointer,
        .area = {desc.area.x, desc.area.y, desc.area.width, desc.area.height},
        .layer_count = desc.layer_count,
        .reserved = 0,
        .reserved_2 = 0,
    };
    return from_native(granit_command_recorder_begin_rendering(renderer_, handle_, &native));
  }
  [[nodiscard]] result end_rendering() noexcept {
    return from_native(granit_command_recorder_end_rendering(renderer_, handle_));
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
