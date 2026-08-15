// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_DIAGNOSTIC_HPP_
#define GRANIT_DIAGNOSTIC_HPP_

#include <cstdint>

#include <granit/core/diagnostic.h>

namespace granit {

enum class diagnostic_severity : std::uint32_t {
  info = GRANIT_DIAGNOSTIC_SEVERITY_INFO,
  warning = GRANIT_DIAGNOSTIC_SEVERITY_WARNING,
  error = GRANIT_DIAGNOSTIC_SEVERITY_ERROR,
};

enum class diagnostic_category : std::uint32_t {
  general = GRANIT_DIAGNOSTIC_CATEGORY_GENERAL,
  validation = GRANIT_DIAGNOSTIC_CATEGORY_VALIDATION,
  performance = GRANIT_DIAGNOSTIC_CATEGORY_PERFORMANCE,
  lifecycle = GRANIT_DIAGNOSTIC_CATEGORY_LIFECYCLE,
  device = GRANIT_DIAGNOSTIC_CATEGORY_DEVICE,
};

using diagnostic_callback = void (*)(granit_diagnostic_severity severity,
                                     granit_diagnostic_category category, const char* message,
                                     std::uint32_t message_length, void* user_data) noexcept;

} // namespace granit

#endif
