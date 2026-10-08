// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "core/frame_trace.h"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

#include <catch2/catch_all.hpp>

namespace {

using granit::detail::diagnostic_category;
using granit::detail::diagnostic_severity;
using granit::detail::frame_trace;

std::string read_file(const std::filesystem::path& path) {
  std::ifstream input{path, std::ios::binary};
  return {std::istreambuf_iterator<char>{input}, {}};
}

TEST_CASE("帧 Trace 默认关闭且诊断不会产生输出", "[frame-trace]") {
  auto& trace = frame_trace::instance();
  trace.reset_for_testing();
  trace.emit_diagnostic(diagnostic_severity::info, diagnostic_category::general, "ignored");
  CHECK_FALSE(trace.enabled());
}

TEST_CASE("帧 Trace 输出诊断 JSONL 并转义消息", "[frame-trace]") {
  const auto path = std::filesystem::temp_directory_path() / "granit-frame-trace-test.jsonl";
  auto& trace = frame_trace::instance();
  trace.configure_for_testing(path.string(), 8);
  trace.emit_diagnostic(diagnostic_severity::warning, diagnostic_category::validation,
                        "quote=\"line\n");
  trace.flush();

  const auto content = read_file(path);
  CHECK(content.find("\"kind\":\"diagnostic\"") != std::string::npos);
  CHECK(content.find("\"severity_name\":\"warning\"") != std::string::npos);
  CHECK(content.find("quote=\\\"line\\n") != std::string::npos);
  std::filesystem::remove(path);
  trace.reset_for_testing();
}

TEST_CASE("帧 Trace 超出容量时只保留最新事件并记录丢弃数", "[frame-trace]") {
  const auto path = std::filesystem::temp_directory_path() / "granit-frame-trace-ring-test.jsonl";
  auto& trace = frame_trace::instance();
  trace.configure_for_testing(path.string(), 2);
  trace.emit_event("first", "\"value\":1");
  trace.emit_event("second", "\"value\":2");
  trace.emit_event("third", "\"value\":3");
  trace.flush();

  const auto content = read_file(path);
  CHECK(content.find("\"kind\":\"first\"") == std::string::npos);
  CHECK(content.find("\"kind\":\"second\"") != std::string::npos);
  CHECK(content.find("\"kind\":\"third\"") != std::string::npos);
  CHECK(content.find("\"dropped_events\":1") != std::string::npos);
  std::filesystem::remove(path);
  trace.reset_for_testing();
}

} // namespace
