// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_WINDOW_WINDOW_REF_HPP_
#define GRANIT_WINDOW_WINDOW_REF_HPP_

#include <granit/window/window.h>

namespace granit {

class window;

/** 不拥有 Window，只在来源窗口及其 Window System 的有效期内使用。 */
class window_ref {
public:
  window_ref() = default;

  [[nodiscard]] constexpr bool valid() const noexcept { return handle_ != GRANIT_NULL_HANDLE; }
  [[nodiscard]] constexpr explicit operator bool() const noexcept { return valid(); }
  [[nodiscard]] constexpr granit_window native_handle() const noexcept { return handle_; }
  [[nodiscard]] static constexpr window_ref from_native(granit_window handle) noexcept {
    return window_ref{handle};
  }

  friend constexpr bool operator==(window_ref, window_ref) noexcept = default;

private:
  friend class window;
  explicit constexpr window_ref(granit_window handle) noexcept : handle_(handle) {}

  granit_window handle_{GRANIT_NULL_HANDLE};
};

} // namespace granit

#endif
