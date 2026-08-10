// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_RESULT_HPP_
#define GRANIT_RESULT_HPP_

#include <cstdint>
#include <string_view>

#include <granit/core/result.h>

namespace granit {

enum class result : std::int32_t {
  success = GRANIT_SUCCESS,
  unknown = GRANIT_ERROR_UNKNOWN,
  invalid_argument = GRANIT_ERROR_INVALID_ARGUMENT,
  invalid_handle = GRANIT_ERROR_INVALID_HANDLE,
  out_of_memory = GRANIT_ERROR_OUT_OF_MEMORY,
  unsupported = GRANIT_ERROR_UNSUPPORTED,
  device_lost = GRANIT_ERROR_DEVICE_LOST,
  internal = GRANIT_ERROR_INTERNAL,
  backend_unavailable = GRANIT_ERROR_BACKEND_UNAVAILABLE,
  incompatible_driver = GRANIT_ERROR_INCOMPATIBLE_DRIVER,
  initialization_failed = GRANIT_ERROR_INITIALIZATION_FAILED,
  no_suitable_device = GRANIT_ERROR_NO_SUITABLE_DEVICE,
  surface_lost = GRANIT_ERROR_SURFACE_LOST,
  out_of_date = GRANIT_ERROR_OUT_OF_DATE,
  not_ready = GRANIT_ERROR_NOT_READY,
};

[[nodiscard]] constexpr granit_result to_native(result value) noexcept {
  return static_cast<granit_result>(value);
}

[[nodiscard]] constexpr result from_native(granit_result value) noexcept {
  return static_cast<result>(value);
}

[[nodiscard]] constexpr bool succeeded(result value) noexcept {
  return to_native(value) >= GRANIT_SUCCESS;
}

[[nodiscard]] constexpr bool failed(result value) noexcept { return !succeeded(value); }

[[nodiscard]] inline std::string_view result_message(result value) noexcept {
  return granit_result_message(to_native(value));
}

} // namespace granit

#endif
