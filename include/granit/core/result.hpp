// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_RESULT_HPP_
#define GRANIT_RESULT_HPP_

#include <cstdint>
#include <string_view>

#include <granit/core/result.h>

namespace granit {

/** C++ 结果值。布尔上下文中的 true 表示操作成功。 */
struct result {
  constexpr result() noexcept = default;
  constexpr explicit result(granit_result value) noexcept : value_(value) {}

  [[nodiscard]] constexpr bool ok() const noexcept { return value_ >= GRANIT_SUCCESS; }
  [[nodiscard]] constexpr bool failed() const noexcept { return !ok(); }
  [[nodiscard]] constexpr granit_result native() const noexcept { return value_; }
  [[nodiscard]] std::string_view message() const noexcept { return granit_result_message(value_); }

  [[nodiscard]] constexpr explicit operator bool() const noexcept { return ok(); }
  [[nodiscard]] constexpr explicit operator granit_result() const noexcept { return value_; }

  friend constexpr bool operator==(result, result) noexcept = default;

  static const result success;
  static const result unknown;
  static const result invalid_argument;
  static const result invalid_handle;
  static const result out_of_memory;
  static const result unsupported;
  static const result device_lost;
  static const result internal;
  static const result backend_unavailable;
  static const result incompatible_driver;
  static const result initialization_failed;
  static const result no_suitable_device;
  static const result surface_lost;
  static const result out_of_date;
  static const result not_ready;

private:
  granit_result value_ = GRANIT_ERROR_UNKNOWN;
};

inline constexpr result result::success{GRANIT_SUCCESS};
inline constexpr result result::unknown{GRANIT_ERROR_UNKNOWN};
inline constexpr result result::invalid_argument{GRANIT_ERROR_INVALID_ARGUMENT};
inline constexpr result result::invalid_handle{GRANIT_ERROR_INVALID_HANDLE};
inline constexpr result result::out_of_memory{GRANIT_ERROR_OUT_OF_MEMORY};
inline constexpr result result::unsupported{GRANIT_ERROR_UNSUPPORTED};
inline constexpr result result::device_lost{GRANIT_ERROR_DEVICE_LOST};
inline constexpr result result::internal{GRANIT_ERROR_INTERNAL};
inline constexpr result result::backend_unavailable{GRANIT_ERROR_BACKEND_UNAVAILABLE};
inline constexpr result result::incompatible_driver{GRANIT_ERROR_INCOMPATIBLE_DRIVER};
inline constexpr result result::initialization_failed{GRANIT_ERROR_INITIALIZATION_FAILED};
inline constexpr result result::no_suitable_device{GRANIT_ERROR_NO_SUITABLE_DEVICE};
inline constexpr result result::surface_lost{GRANIT_ERROR_SURFACE_LOST};
inline constexpr result result::out_of_date{GRANIT_ERROR_OUT_OF_DATE};
inline constexpr result result::not_ready{GRANIT_ERROR_NOT_READY};

[[nodiscard]] constexpr granit_result to_native(result value) noexcept {
  return value.native();
}

[[nodiscard]] constexpr result from_native(granit_result value) noexcept {
  return result{value};
}

[[nodiscard]] inline std::string_view result_message(result value) noexcept {
  return value.message();
}

} // namespace granit

#endif
