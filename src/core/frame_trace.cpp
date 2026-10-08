// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "core/frame_trace.h"

#include <chrono>
#include <cstdlib>
#include <fstream>
#include <limits>

namespace granit::detail {
namespace {

constexpr std::uint32_t trace_schema_version = 1;

std::uint64_t timestamp_ns() noexcept {
  static const auto origin = std::chrono::steady_clock::now();
  const auto elapsed = std::chrono::steady_clock::now() - origin;
  return static_cast<std::uint64_t>(
      std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed).count());
}

void append_json_string(std::string& output, std::string_view value) {
  output.push_back('"');
  for (const char character : value) {
    switch (character) {
    case '"':
      output += "\\\"";
      break;
    case '\\':
      output += "\\\\";
      break;
    case '\n':
      output += "\\n";
      break;
    case '\r':
      output += "\\r";
      break;
    case '\t':
      output += "\\t";
      break;
    default:
      output.push_back(character);
      break;
    }
  }
  output.push_back('"');
}

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

const char* severity_name(diagnostic_severity severity) noexcept {
  switch (severity) {
  case diagnostic_severity::info:
    return "info";
  case diagnostic_severity::warning:
    return "warning";
  case diagnostic_severity::error:
    return "error";
  }
  return "unknown";
}

std::uint64_t parse_max_events(const char* value) noexcept {
  if (value == nullptr || *value == '\0')
    return 4096;
  char* end = nullptr;
  const auto parsed = std::strtoull(value, &end, 10);
  if (end == value || *end != '\0' || parsed == 0)
    return 4096;
  return parsed > std::numeric_limits<std::uint64_t>::max()
             ? std::numeric_limits<std::uint64_t>::max()
             : static_cast<std::uint64_t>(parsed);
}

std::string environment_value(const char* name) noexcept {
  try {
#if defined(_WIN32)
    char* value = nullptr;
    std::size_t length = 0;
    if (_dupenv_s(&value, &length, name) != 0 || value == nullptr)
      return {};
    try {
      std::string result{value};
      std::free(value);
      return result;
    } catch (...) {
      std::free(value);
      throw;
    }
#else
    const char* value = std::getenv(name);
    return value == nullptr ? std::string{} : std::string{value};
#endif
  } catch (...) {
#if defined(_WIN32)
    // _dupenv_s 的失败路径不会返回需要释放的值。
#endif
    return {};
  }
}

} // namespace

frame_trace& frame_trace::instance() noexcept {
  static frame_trace trace;
  return trace;
}

frame_trace::~frame_trace() noexcept { flush(); }

void frame_trace::configure_from_environment_locked() noexcept {
  configured_ = true;
  const std::string path = environment_value("GRANIT_FRAME_TRACE");
  if (path.empty())
    return;
  try {
    path_ = path;
    const std::string max_events = environment_value("GRANIT_FRAME_TRACE_MAX_EVENTS");
    max_events_ = parse_max_events(max_events.c_str());
    enabled_ = true;
  } catch (...) {
    path_.clear();
    enabled_ = false;
  }
}

void frame_trace::append_event_locked(std::string_view kind,
                                      std::string_view payload_json) noexcept {
  try {
    std::string line;
    line.reserve(kind.size() + payload_json.size() + 96);
    line += "{\"schema_version\":";
    line += std::to_string(trace_schema_version);
    line += ",\"sequence\":";
    line += std::to_string(++sequence_);
    line += ",\"timestamp_ns\":";
    line += std::to_string(timestamp_ns());
    line += ",\"kind\":";
    append_json_string(line, kind);
    line += ",";
    line += payload_json;
    line += "}\n";
    const auto capacity = static_cast<std::size_t>(
        std::min<std::uint64_t>(max_events_, std::numeric_limits<std::size_t>::max()));
    if (capacity == 0)
      return;
    if (events_.size() < capacity) {
      events_.push_back(std::move(line));
      return;
    }
    events_[event_start_] = std::move(line);
    event_start_ = (event_start_ + 1) % capacity;
    ++dropped_events_;
  } catch (...) {
    ++dropped_events_;
  }
}

void frame_trace::emit_event(std::string_view kind, std::string_view payload_json) noexcept {
  std::lock_guard lock{mutex_};
  if (!configured_)
    configure_from_environment_locked();
  if (enabled_ && !kind.empty() && !payload_json.empty())
    append_event_locked(kind, payload_json);
}

void frame_trace::emit_diagnostic(diagnostic_severity severity, diagnostic_category category,
                                  std::string_view message) noexcept {
  try {
    std::string payload;
    payload.reserve(message.size() + 96);
    payload += "\"severity\":";
    payload += std::to_string(static_cast<std::uint32_t>(severity));
    payload += ",\"severity_name\":";
    append_json_string(payload, severity_name(severity));
    payload += ",\"category\":";
    payload += std::to_string(static_cast<std::uint32_t>(category));
    payload += ",\"category_name\":";
    append_json_string(payload, category_name(category));
    payload += ",\"message\":";
    append_json_string(payload, message);
    emit_event("diagnostic", payload);
  } catch (...) {
    // 诊断输出失败不得影响渲染路径。
  }
}

void frame_trace::flush() noexcept {
  std::lock_guard lock{mutex_};
  if (!configured_)
    configure_from_environment_locked();
  if (!enabled_)
    return;
  try {
    std::ofstream output{path_, std::ios::binary | std::ios::trunc};
    if (!output) {
      ++flush_failures_;
      return;
    }
    for (std::size_t index = 0; index < events_.size(); ++index)
      output << events_[(event_start_ + index) % events_.size()];
    output << "{\"schema_version\":" << trace_schema_version << ",\"sequence\":" << ++sequence_
           << ",\"timestamp_ns\":" << timestamp_ns()
           << ",\"kind\":\"trace_summary\",\"dropped_events\":" << dropped_events_
           << ",\"flush_failures\":" << flush_failures_ << "}\n";
    if (!output)
      ++flush_failures_;
  } catch (...) {
    // 文件系统错误不能破坏渲染路径。
    ++flush_failures_;
  }
}

bool frame_trace::enabled() const noexcept {
  std::lock_guard lock{mutex_};
  if (!configured_)
    const_cast<frame_trace*>(this)->configure_from_environment_locked();
  return enabled_;
}

void frame_trace::configure_for_testing(std::string path, std::uint64_t max_events) noexcept {
  std::lock_guard lock{mutex_};
  configured_ = true;
  enabled_ = !path.empty();
  path_ = std::move(path);
  max_events_ = max_events == 0 ? 1 : max_events;
  events_.clear();
  event_start_ = 0;
  dropped_events_ = 0;
  flush_failures_ = 0;
  sequence_ = 0;
}

void frame_trace::reset_for_testing() noexcept {
  std::lock_guard lock{mutex_};
  configured_ = false;
  enabled_ = false;
  path_.clear();
  events_.clear();
  event_start_ = 0;
  dropped_events_ = 0;
  flush_failures_ = 0;
  sequence_ = 0;
  max_events_ = 4096;
}

} // namespace granit::detail
