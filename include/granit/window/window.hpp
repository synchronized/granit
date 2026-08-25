// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_WINDOW_WINDOW_HPP_
#define GRANIT_WINDOW_WINDOW_HPP_

#include <cstdint>
#include <string_view>
#include <utility>

#include <granit/core/result.hpp>
#include <granit/window/window.h>

namespace granit {

enum class window_backend : std::uint32_t {
  automatic = GRANIT_WINDOW_BACKEND_AUTO,
  win32 = GRANIT_WINDOW_BACKEND_WIN32,
  xcb = GRANIT_WINDOW_BACKEND_XCB,
  wayland = GRANIT_WINDOW_BACKEND_WAYLAND
};

struct window_system_desc {
  window_backend backend{window_backend::automatic};
};

struct window_desc {
  std::string_view title;
  std::uint32_t width{};
  std::uint32_t height{};
  std::uint32_t flags{GRANIT_WINDOW_VISIBLE_BIT | GRANIT_WINDOW_RESIZABLE_BIT};
};

using window_event = granit_window_event;

class window_system {
public:
  window_system() = default;
  ~window_system() { static_cast<void>(reset()); }
  window_system(const window_system&) = delete;
  window_system& operator=(const window_system&) = delete;
  window_system(window_system&& other) noexcept
      : handle_(std::exchange(other.handle_, GRANIT_NULL_HANDLE)) {}
  window_system& operator=(window_system&& other) noexcept {
    if (this != &other) {
      static_cast<void>(reset());
      handle_ = std::exchange(other.handle_, GRANIT_NULL_HANDLE);
    }
    return *this;
  }

  [[nodiscard]] result initialize(const window_system_desc& desc = {}) noexcept {
    if (valid())
      return result::invalid_argument;
    granit_window_system_desc native_desc{};
    native_desc.struct_size = sizeof(granit_window_system_desc);
    native_desc.backend = static_cast<std::uint32_t>(desc.backend);
    return from_native(granit_window_system_create(&native_desc, &handle_));
  }
  [[nodiscard]] result poll(window_event& event) noexcept {
    event.struct_size = sizeof(window_event);
    return from_native(granit_window_poll_event(handle_, &event));
  }
  [[nodiscard]] result reset() noexcept {
    if (!valid())
      return result::success;
    const auto value = granit_window_system_destroy(handle_);
    if (value == GRANIT_SUCCESS || value == GRANIT_ERROR_INVALID_HANDLE)
      handle_ = GRANIT_NULL_HANDLE;
    return from_native(value);
  }
  [[nodiscard]] bool valid() const noexcept { return handle_ != GRANIT_NULL_HANDLE; }
  [[nodiscard]] granit_window_system native_handle() const noexcept { return handle_; }

private:
  granit_window_system handle_{GRANIT_NULL_HANDLE};
};

class window {
public:
  window() = default;
  ~window() { static_cast<void>(reset()); }
  window(const window&) = delete;
  window& operator=(const window&) = delete;
  window(window&& other) noexcept
      : system_(std::exchange(other.system_, GRANIT_NULL_HANDLE)),
        handle_(std::exchange(other.handle_, GRANIT_NULL_HANDLE)) {}
  window& operator=(window&& other) noexcept {
    if (this != &other) {
      static_cast<void>(reset());
      system_ = std::exchange(other.system_, GRANIT_NULL_HANDLE);
      handle_ = std::exchange(other.handle_, GRANIT_NULL_HANDLE);
    }
    return *this;
  }

  [[nodiscard]] result initialize(granit_window_system system, const window_desc& desc) noexcept {
    if (valid() || desc.title.size() > UINT32_MAX)
      return result::invalid_argument;
    if (system == GRANIT_NULL_HANDLE)
      return result::invalid_handle;
    granit_window_desc native_desc{};
    native_desc.struct_size = sizeof(granit_window_desc);
    native_desc.title = desc.title.data();
    native_desc.title_length = static_cast<std::uint32_t>(desc.title.size());
    native_desc.width = desc.width;
    native_desc.height = desc.height;
    native_desc.flags = desc.flags;
    const auto value = granit_window_create(system, &native_desc, &handle_);
    if (value == GRANIT_SUCCESS)
      system_ = system;
    return from_native(value);
  }
  [[nodiscard]] result reset() noexcept {
    if (!valid())
      return result::success;
    const auto value = granit_window_destroy(system_, handle_);
    if (value == GRANIT_SUCCESS || value == GRANIT_ERROR_INVALID_HANDLE) {
      system_ = GRANIT_NULL_HANDLE;
      handle_ = GRANIT_NULL_HANDLE;
    }
    return from_native(value);
  }
  [[nodiscard]] result native_win32(void*& instance, void*& native_window) const noexcept {
    return from_native(granit_window_get_win32(system_, handle_, &instance, &native_window));
  }

  [[nodiscard]] result native_xcb(void*& connection, std::uint32_t& native_window) const noexcept {
    return from_native(granit_window_get_xcb(system_, handle_, &connection, &native_window));
  }

  [[nodiscard]] result native_wayland(void*& display, void*& surface) const noexcept {
    return from_native(granit_window_get_wayland(system_, handle_, &display, &surface));
  }
  [[nodiscard]] bool valid() const noexcept { return handle_ != GRANIT_NULL_HANDLE; }
  [[nodiscard]] granit_window native_handle() const noexcept { return handle_; }

private:
  granit_window_system system_{GRANIT_NULL_HANDLE};
  granit_window handle_{GRANIT_NULL_HANDLE};
};

} // namespace granit

#endif
