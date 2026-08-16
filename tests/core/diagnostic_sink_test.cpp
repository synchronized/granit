// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "core/diagnostic_sink.h"

#include <catch2/catch_all.hpp>

#include <atomic>
#include <string>
#include <thread>
#include <vector>

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

void count_diagnostic(granit_diagnostic_severity, granit_diagnostic_category, const char*,
                      std::uint32_t, void* user_data) {
  static_cast<std::atomic_uint32_t*>(user_data)->fetch_add(1, std::memory_order_relaxed);
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

TEST_CASE("Diagnostic Sink 允许多个产生线程并发进入回调", "[diagnostic][concurrency]") {
  std::atomic_uint32_t calls{};
  const granit::detail::diagnostic_sink sink{count_diagnostic, &calls};
  constexpr std::uint32_t thread_count = 8;
  constexpr std::uint32_t messages_per_thread = 128;
  std::vector<std::thread> threads;
  threads.reserve(thread_count);
  for (std::uint32_t thread = 0; thread < thread_count; ++thread) {
    threads.emplace_back([&sink] {
      for (std::uint32_t message = 0; message < messages_per_thread; ++message) {
        sink.emit(granit::detail::diagnostic_severity::info,
                  granit::detail::diagnostic_category::general, "concurrent message");
      }
    });
  }
  for (auto& thread : threads)
    thread.join();
  CHECK(calls.load(std::memory_order_relaxed) == thread_count * messages_per_thread);
}
