// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_WINDOW_WINDOW_HPP_
#define GRANIT_WINDOW_WINDOW_HPP_

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <type_traits>
#include <utility>

#include <granit/core/result.hpp>
#include <granit/window/input.hpp>
#include <granit/window/window.h>
#include <granit/window/window_ref.hpp>

namespace granit {

class renderer;
class renderer_ref;
class surface;

enum class window_backend : std::uint32_t {
  automatic = GRANIT_WINDOW_BACKEND_AUTO,
  win32 = GRANIT_WINDOW_BACKEND_WIN32,
  xcb = GRANIT_WINDOW_BACKEND_XCB,
  wayland = GRANIT_WINDOW_BACKEND_WAYLAND,
  emscripten = GRANIT_WINDOW_BACKEND_EMSCRIPTEN
};

struct window_system_desc {
  window_backend backend{window_backend::automatic};
};

enum class window_flag : std::uint32_t {
  none = 0,
  visible = GRANIT_WINDOW_VISIBLE_BIT,
  resizable = GRANIT_WINDOW_RESIZABLE_BIT,
  high_dpi = GRANIT_WINDOW_HIGH_DPI_BIT,
};

[[nodiscard]] constexpr window_flag operator|(window_flag left, window_flag right) noexcept {
  return static_cast<window_flag>(static_cast<std::uint32_t>(left) |
                                  static_cast<std::uint32_t>(right));
}

struct window_desc {
  std::string_view title;
  std::uint32_t width{};
  std::uint32_t height{};
  window_flag flags{window_flag::visible | window_flag::resizable};
};

enum class window_event_type : std::uint32_t {
  none = 0,
  close_requested = GRANIT_WINDOW_EVENT_CLOSE_REQUESTED,
  resized = GRANIT_WINDOW_EVENT_RESIZED,
  focus_changed = GRANIT_WINDOW_EVENT_FOCUS_CHANGED,
  scale_changed = GRANIT_WINDOW_EVENT_SCALE_CHANGED,
  native_handle_changed = GRANIT_WINDOW_EVENT_NATIVE_HANDLE_CHANGED,
};

struct window_resize_event {
  std::uint32_t width{};
  std::uint32_t height{};
};

struct window_focus_event {
  bool focused{};
};

struct window_scale_event {
  float horizontal{};
  float vertical{};
  std::uint32_t width{};
  std::uint32_t height{};
};

struct window_native_handle_event {
  window_backend backend{window_backend::automatic};
};

union window_event_data {
  window_resize_event resized;
  window_focus_event focus;
  window_scale_event scale;
  window_native_handle_event native_handle;
  std::uint8_t reserved[32]{};
};

struct window_event {
  window_event_type type{};
  window_ref window;
  std::uint64_t timestamp_ns{};
  window_event_data data{};
};

struct window_state {
  std::uint32_t width{};
  std::uint32_t height{};
  std::uint32_t framebuffer_width{};
  std::uint32_t framebuffer_height{};
  float content_scale_horizontal{1.0F};
  float content_scale_vertical{1.0F};
};

static_assert(std::is_trivially_copyable_v<window_event_data>);

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
    granit_window_event native = GRANIT_WINDOW_EVENT_INIT;
    const auto value = granit_window_poll_event(handle_, &native);
    if (value == GRANIT_SUCCESS) {
      event.type = static_cast<window_event_type>(native.type);
      event.window = window_ref::from_native(native.window);
      event.timestamp_ns = native.timestamp_ns;
      event.data = {};
      switch (event.type) {
      case window_event_type::resized:
        event.data.resized = {.width = native.data.resized.width,
                              .height = native.data.resized.height};
        break;
      case window_event_type::focus_changed:
        event.data.focus = {.focused = native.data.focus.focused != 0};
        break;
      case window_event_type::scale_changed:
        event.data.scale = {.horizontal = native.data.scale.horizontal,
                            .vertical = native.data.scale.vertical,
                            .width = native.data.scale.width,
                            .height = native.data.scale.height};
        break;
      case window_event_type::native_handle_changed:
        event.data.native_handle = {
            .backend = static_cast<window_backend>(native.data.native_handle.backend)};
        break;
      default:
        break;
      }
    }
    return from_native(value);
  }
  [[nodiscard]] result process_events() noexcept {
    return from_native(granit_window_system_process_events(handle_));
  }
  [[nodiscard]] result poll(input_event& event) noexcept {
    granit_input_event native = GRANIT_INPUT_EVENT_INIT;
    const auto value = granit_window_poll_input_event(handle_, &native);
    if (value == GRANIT_SUCCESS)
      event = detail::from_native(native);
    return from_native(value);
  }
  [[nodiscard]] result keyboard(window_ref window, keyboard_state& state) const noexcept {
    granit_keyboard_state native = GRANIT_KEYBOARD_STATE_INIT;
    const auto value = granit_window_get_keyboard_state(handle_, window.native_handle(), &native);
    if (value == GRANIT_SUCCESS) {
      state.modifiers = native.modifiers;
      for (std::size_t index = 0; index < state.pressed_keys.size(); ++index)
        state.pressed_keys[index] = native.pressed_keys[index];
    }
    return from_native(value);
  }
  [[nodiscard]] result pointer(window_ref window, pointer_state& state) const noexcept {
    granit_pointer_state native = GRANIT_POINTER_STATE_INIT;
    const auto value = granit_window_get_pointer_state(handle_, window.native_handle(), &native);
    if (value == GRANIT_SUCCESS) {
      state = {.buttons = native.buttons,
               .x = native.x,
               .y = native.y,
               .inside = native.inside != 0};
    }
    return from_native(value);
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

  [[nodiscard]] result initialize(window_system& system, const window_desc& desc) noexcept {
    if (valid() || desc.title.size() > UINT32_MAX)
      return result::invalid_argument;
    granit_window_desc native_desc{};
    native_desc.struct_size = sizeof(granit_window_desc);
    native_desc.title = desc.title.data();
    native_desc.title_length = static_cast<std::uint32_t>(desc.title.size());
    native_desc.width = desc.width;
    native_desc.height = desc.height;
    native_desc.flags = static_cast<std::uint32_t>(desc.flags);
    const auto value = granit_window_create(system.native_handle(), &native_desc, &handle_);
    if (value == GRANIT_SUCCESS)
      system_ = system.native_handle();
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
  [[nodiscard]] result create_surface(renderer_ref owner, surface& output) const noexcept;
  [[nodiscard]] result create_surface(renderer& owner, surface& output) const noexcept;
  [[nodiscard]] result get_state(window_state& state) const noexcept {
    granit_window_state native = GRANIT_WINDOW_STATE_INIT;
    const auto value = granit_window_get_state(system_, handle_, &native);
    if (value == GRANIT_SUCCESS) {
      state = {.width = native.width,
               .height = native.height,
               .framebuffer_width = native.framebuffer_width,
               .framebuffer_height = native.framebuffer_height,
               .content_scale_horizontal = native.content_scale_horizontal,
               .content_scale_vertical = native.content_scale_vertical};
    }
    return from_native(value);
  }

  [[nodiscard]] bool valid() const noexcept { return handle_ != GRANIT_NULL_HANDLE; }
  [[nodiscard]] constexpr window_ref ref() const noexcept { return window_ref{handle_}; }
  [[nodiscard]] granit_window native_handle() const noexcept { return handle_; }

private:
  granit_window_system system_{GRANIT_NULL_HANDLE};
  granit_window handle_{GRANIT_NULL_HANDLE};
};

} // namespace granit

#include <granit/window/presentation.hpp>

#endif
