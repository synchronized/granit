// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "core/diagnostic_sink.h"

#include <cstdio>
#include <limits>

namespace granit::detail {
namespace {

const char* category_name(diagnostic_category category) noexcept {
  switch (category) {
  case diagnostic_category::general:
    return "general";
  case diagnostic_category::validation:
    return "validation";
  case diagnostic_category::performance:
    return "performance";
  case diagnostic_category::lifecycle:
    return "lifecycle";
  case diagnostic_category::device:
    return "device";
  }
  return "unknown";
}

} // namespace

void diagnostic_sink::emit(diagnostic_severity severity, diagnostic_category category,
                           std::string_view message) const noexcept {
  if (callback_ != nullptr && message.size() <= std::numeric_limits<std::uint32_t>::max()) {
    try {
      callback_(static_cast<granit_diagnostic_severity>(severity),
                static_cast<granit_diagnostic_category>(category), message.data(),
                static_cast<std::uint32_t>(message.size()), user_data_);
    } catch (...) {
      // 用户代码的异常不得越过 C ABI 或 Vulkan 回调边界。
    }
    return;
  }

  std::lock_guard lock{fallback_mutex_};
  std::fprintf(stderr, "[granit][%s] ", category_name(category));
  if (!message.empty()) {
    std::fwrite(message.data(), sizeof(char), message.size(), stderr);
  }
  std::fputc('\n', stderr);
}

const diagnostic_sink& default_diagnostic_sink() noexcept {
  static const diagnostic_sink sink;
  return sink;
}

} // namespace granit::detail
