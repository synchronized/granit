// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_CORE_DIAGNOSTIC_SINK_H_
#define GRANIT_CORE_DIAGNOSTIC_SINK_H_

#include <cstdint>
#include <mutex>
#include <string_view>

namespace granit::detail {

enum class diagnostic_severity : std::uint32_t { info = 1, warning = 2, error = 3 };

enum class diagnostic_category : std::uint32_t {
  general = 1,
  validation = 2,
  performance = 3,
  lifecycle = 4,
  device = 5,
};

using diagnostic_callback = void (*)(diagnostic_severity severity, diagnostic_category category,
                                     const char* message, std::uint32_t message_length,
                                     void* user_data);

/** Renderer 拥有的同步诊断出口；未配置回调时写入标准错误流。 */
class diagnostic_sink {
public:
  diagnostic_sink() = default;
  diagnostic_sink(diagnostic_callback callback, void* user_data) noexcept
      : callback_(callback), user_data_(user_data) {}

  diagnostic_sink(const diagnostic_sink&) = delete;
  diagnostic_sink& operator=(const diagnostic_sink&) = delete;

  void emit(diagnostic_severity severity, diagnostic_category category,
            std::string_view message) const noexcept;

private:
  diagnostic_callback callback_{};
  void* user_data_{};
  mutable std::mutex fallback_mutex_;
};

/** 供尚未绑定 Renderer 的初始化诊断使用。 */
[[nodiscard]] const diagnostic_sink& default_diagnostic_sink() noexcept;

} // namespace granit::detail

#endif
