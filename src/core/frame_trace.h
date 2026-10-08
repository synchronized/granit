// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_CORE_FRAME_TRACE_H_
#define GRANIT_CORE_FRAME_TRACE_H_

#include <cstdint>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

#include "core/diagnostic_sink.h"

namespace granit::detail {

/** 开发期帧 Trace；默认关闭，事件只保存在内部固定容量缓冲区。 */
class frame_trace final {
public:
  static frame_trace& instance() noexcept;

  ~frame_trace() noexcept;

  frame_trace(const frame_trace&) = delete;
  frame_trace& operator=(const frame_trace&) = delete;

  void emit_event(std::string_view kind, std::string_view payload_json) noexcept;
  void emit_diagnostic(diagnostic_severity severity, diagnostic_category category,
                       std::string_view message) noexcept;
  void flush() noexcept;

  [[nodiscard]] bool enabled() const noexcept;

  // 仅供内部测试使用，避免测试依赖进程环境变量和固定文件名。
  void configure_for_testing(std::string path, std::uint64_t max_events) noexcept;
  void reset_for_testing() noexcept;

private:
  frame_trace() = default;

  void configure_from_environment_locked() noexcept;
  void append_event_locked(std::string_view kind, std::string_view payload_json) noexcept;

  mutable std::mutex mutex_;
  bool configured_{};
  bool enabled_{};
  std::string path_;
  std::vector<std::string> events_;
  std::size_t event_start_{};
  std::uint64_t max_events_{4096};
  std::uint64_t dropped_events_{};
  std::uint64_t flush_failures_{};
  std::uint64_t sequence_{};
};

} // namespace granit::detail

#endif
