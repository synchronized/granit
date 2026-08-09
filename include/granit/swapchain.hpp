// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_SWAPCHAIN_HPP_
#define GRANIT_SWAPCHAIN_HPP_

#include <cstdint>
#include <utility>

#include <granit/result.hpp>
#include <granit/swapchain.h>

namespace granit {

enum class present_mode : std::uint32_t {
  fifo = GRANIT_PRESENT_MODE_FIFO,
  mailbox = GRANIT_PRESENT_MODE_MAILBOX,
  immediate = GRANIT_PRESENT_MODE_IMMEDIATE,
};

struct swapchain_desc {
  std::uint32_t width{1};
  std::uint32_t height{1};
  std::uint32_t minimum_image_count{};
  present_mode presentation{present_mode::fifo};
};

struct swapchain_info {
  std::uint32_t width{};
  std::uint32_t height{};
  std::uint32_t image_count{};
  present_mode presentation{present_mode::fifo};
};

struct acquired_frame {
  acquired_frame() = default;
  ~acquired_frame() {
    if (valid()) {
      std::uint32_t recreate{};
      const auto presented = granit_swapchain_present(renderer, swapchain, handle, &recreate);
      if (presented == GRANIT_ERROR_INVALID_ARGUMENT)
        static_cast<void>(granit_frame_cancel(renderer, swapchain, handle, &recreate));
    }
  }
  acquired_frame(const acquired_frame&) = delete;
  acquired_frame& operator=(const acquired_frame&) = delete;
  acquired_frame(acquired_frame&& other) noexcept
      : handle(std::exchange(other.handle, GRANIT_NULL_HANDLE)), image_index(other.image_index),
        needs_recreate(other.needs_recreate), renderer(other.renderer), swapchain(other.swapchain) {
  }
  acquired_frame& operator=(acquired_frame&&) = delete;

  granit_frame handle{GRANIT_NULL_HANDLE};
  std::uint32_t image_index{};
  bool needs_recreate{};
  granit_renderer renderer{GRANIT_NULL_HANDLE};
  granit_swapchain swapchain{GRANIT_NULL_HANDLE};

  [[nodiscard]] bool valid() const noexcept { return handle != GRANIT_NULL_HANDLE; }
};

class swapchain {
public:
  swapchain() = default;
  ~swapchain() { static_cast<void>(reset()); }

  swapchain(const swapchain&) = delete;
  swapchain& operator=(const swapchain&) = delete;
  swapchain(swapchain&& other) noexcept
      : renderer_(std::exchange(other.renderer_, GRANIT_NULL_HANDLE)),
        handle_(std::exchange(other.handle_, GRANIT_NULL_HANDLE)) {}
  swapchain& operator=(swapchain&& other) noexcept {
    if (this != &other) {
      static_cast<void>(reset());
      renderer_ = std::exchange(other.renderer_, GRANIT_NULL_HANDLE);
      handle_ = std::exchange(other.handle_, GRANIT_NULL_HANDLE);
    }
    return *this;
  }

  [[nodiscard]] result initialize(granit_renderer renderer, granit_surface surface,
                                  const swapchain_desc& desc) noexcept {
    if (valid() || renderer == GRANIT_NULL_HANDLE || surface == GRANIT_NULL_HANDLE) {
      return result::invalid_argument;
    }
    const auto native_desc = to_native(desc);
    const auto native_result = granit_swapchain_create(renderer, surface, &native_desc, &handle_);
    if (native_result == GRANIT_SUCCESS) {
      renderer_ = renderer;
    }
    return from_native(native_result);
  }

  [[nodiscard]] result recreate(const swapchain_desc& desc) noexcept {
    if (!valid()) {
      return result::invalid_handle;
    }
    const auto native_desc = to_native(desc);
    return from_native(granit_swapchain_recreate(renderer_, handle_, &native_desc));
  }

  [[nodiscard]] result query_info(swapchain_info& info) const noexcept {
    if (!valid()) {
      return result::invalid_handle;
    }
    granit_swapchain_info native_info = GRANIT_SWAPCHAIN_INFO_INIT;
    const auto native_result = granit_swapchain_get_info(renderer_, handle_, &native_info);
    if (native_result == GRANIT_SUCCESS) {
      info = {.width = native_info.width,
              .height = native_info.height,
              .image_count = native_info.image_count,
              .presentation = static_cast<present_mode>(native_info.present_mode)};
    }
    return from_native(native_result);
  }

  [[nodiscard]] result backbuffer(std::uint32_t index, granit_texture& texture,
                                  granit_texture_view& view) const noexcept {
    return from_native(granit_swapchain_get_backbuffer(renderer_, handle_, index, &texture, &view));
  }

  [[nodiscard]] result acquire(acquired_frame& frame) const noexcept {
    if (!valid() || frame.valid())
      return result::invalid_argument;
    std::uint32_t recreate{};
    const auto value =
        granit_swapchain_acquire(renderer_, handle_, &frame.handle, &frame.image_index, &recreate);
    frame.needs_recreate = recreate != 0;
    if (value == GRANIT_SUCCESS) {
      frame.renderer = renderer_;
      frame.swapchain = handle_;
    }
    return from_native(value);
  }

  [[nodiscard]] result present(acquired_frame& frame) const noexcept {
    if (!valid() || !frame.valid())
      return result::invalid_argument;
    std::uint32_t recreate{};
    const auto value = granit_swapchain_present(renderer_, handle_, frame.handle, &recreate);
    if (value != GRANIT_ERROR_INVALID_ARGUMENT && value != GRANIT_ERROR_INVALID_HANDLE)
      frame.handle = GRANIT_NULL_HANDLE;
    frame.needs_recreate = recreate != 0;
    return from_native(value);
  }

  [[nodiscard]] result cancel(acquired_frame& frame) const noexcept {
    if (!valid() || !frame.valid())
      return result::invalid_argument;
    std::uint32_t recreate{};
    const auto value = granit_frame_cancel(renderer_, handle_, frame.handle, &recreate);
    if (value != GRANIT_ERROR_INVALID_ARGUMENT && value != GRANIT_ERROR_INVALID_HANDLE)
      frame.handle = GRANIT_NULL_HANDLE;
    frame.needs_recreate = recreate != 0;
    return from_native(value);
  }

  [[nodiscard]] result reset() noexcept {
    if (!valid()) {
      return result::success;
    }
    const auto value = granit_swapchain_destroy(renderer_, handle_);
    if (value == GRANIT_SUCCESS) {
      renderer_ = GRANIT_NULL_HANDLE;
      handle_ = GRANIT_NULL_HANDLE;
    }
    return from_native(value);
  }

  [[nodiscard]] bool valid() const noexcept { return handle_ != GRANIT_NULL_HANDLE; }
  [[nodiscard]] explicit operator bool() const noexcept { return valid(); }
  [[nodiscard]] granit_swapchain native_handle() const noexcept { return handle_; }

private:
  [[nodiscard]] static granit_swapchain_desc to_native(const swapchain_desc& desc) noexcept {
    return {.struct_size = sizeof(granit_swapchain_desc),
            .width = desc.width,
            .height = desc.height,
            .minimum_image_count = desc.minimum_image_count,
            .present_mode = static_cast<granit_present_mode>(desc.presentation)};
  }

  granit_renderer renderer_{GRANIT_NULL_HANDLE};
  granit_swapchain handle_{GRANIT_NULL_HANDLE};
};

} // namespace granit

#endif
