// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "model_viewer/frame_executor.h"

#include <catch2/catch_all.hpp>

namespace {

struct callback_state {
  bool called{};
  std::uint32_t width{};
};

granit::result execute_frame(granit::example::model_viewer::frame_packet&& packet,
                             granit::example::model_viewer::frame_execution_result& output,
                             void* user_data) {
  auto& state = *static_cast<callback_state*>(user_data);
  state.called = true;
  state.width = packet.width;
  output.needs_recreate = true;
  output.acquire_wait_ms = 2.0F;
  return granit::result::not_ready;
}

} // namespace

TEST_CASE("同步帧执行器完整转发帧包和执行结果") {
  callback_state state;
  granit::example::model_viewer::inline_frame_executor executor(execute_frame, &state);
  granit::example::model_viewer::frame_packet packet;
  packet.width = 1280;
  granit::example::model_viewer::frame_execution_result output;

  CHECK(executor.submit(std::move(packet), output) == granit::result::not_ready);
  CHECK(state.called);
  CHECK(state.width == 1280);
  CHECK(output.needs_recreate);
  CHECK(output.acquire_wait_ms == 2.0F);
  CHECK(executor.flush().ok());
}

TEST_CASE("同步帧执行器拒绝空回调") {
  granit::example::model_viewer::inline_frame_executor executor(nullptr, nullptr);
  granit::example::model_viewer::frame_execution_result output;
  output.needs_recreate = true;

  CHECK(executor.submit({}, output) == granit::result::invalid_argument);
  CHECK_FALSE(output.needs_recreate);
}
