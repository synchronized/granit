// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "core/diagnostic_sink.h"

#include <catch2/catch_all.hpp>

#include <string>

namespace {

struct callback_state {
  granit_diagnostic_severity severity{};
  granit_diagnostic_category category{};
  std::string message;
  void* received_user_data{};
};

void capture_diagnostic(granit_diagnostic_severity severity, granit_diagnostic_category category,
                        const char* message, std::uint32_t message_length, void* user_data) {
  auto& state = *static_cast<callback_state*>(user_data);
  state.severity = severity;
  state.category = category;
  state.message.assign(message, message_length);
  state.received_user_data = user_data;
}

void throwing_diagnostic(granit_diagnostic_severity, granit_diagnostic_category, const char*,
                         std::uint32_t, void*) {
  throw 1;
}

} // namespace

TEST_CASE("Diagnostic Sink 保留消息边界与用户数据", "[diagnostic]") {
  callback_state state;
  const granit::detail::diagnostic_sink sink{capture_diagnostic, &state};

  sink.emit(granit::detail::diagnostic_severity::warning,
            granit::detail::diagnostic_category::lifecycle, "含零尾部之外的 UTF-8 消息");

  CHECK(state.severity == GRANIT_DIAGNOSTIC_SEVERITY_WARNING);
  CHECK(state.category == GRANIT_DIAGNOSTIC_CATEGORY_LIFECYCLE);
  CHECK(state.message == "含零尾部之外的 UTF-8 消息");
  CHECK(state.received_user_data == &state);
}

TEST_CASE("Diagnostic Sink 吞掉用户回调异常", "[diagnostic]") {
  const granit::detail::diagnostic_sink sink{throwing_diagnostic, nullptr};
  CHECK_NOTHROW(sink.emit(granit::detail::diagnostic_severity::error,
                          granit::detail::diagnostic_category::validation, "callback failure"));
}
