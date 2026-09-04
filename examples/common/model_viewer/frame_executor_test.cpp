// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "model_viewer/frame_executor.h"

#include <catch2/catch_all.hpp>

#include <condition_variable>
#include <mutex>
#include <vector>

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

struct blocking_callback_state {
  std::mutex mutex;
  std::condition_variable condition;
  std::vector<std::uint32_t> executed_widths;
  bool first_started{};
  bool release_first{};
};

granit::result execute_blocking_frame(granit::example::model_viewer::frame_packet&& packet,
                                      granit::example::model_viewer::frame_execution_result&,
                                      void* user_data) {
  auto& state = *static_cast<blocking_callback_state*>(user_data);
  std::unique_lock lock(state.mutex);
  state.executed_widths.push_back(packet.width);
  if (packet.width == 1) {
    state.first_started = true;
    state.condition.notify_all();
    state.condition.wait(lock, [&] { return state.release_first; });
  }
  return granit::result::success;
}

granit::result execute_command(void* user_data) {
  auto& state = *static_cast<blocking_callback_state*>(user_data);
  std::lock_guard lock(state.mutex);
  state.executed_widths.push_back(99);
  return granit::result::success;
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

TEST_CASE("线程帧执行器限制待处理队列并回报被替换帧") {
  using namespace granit::example::model_viewer;
  blocking_callback_state state;
  threaded_frame_executor executor;
  REQUIRE(executor.initialize(execute_blocking_frame, &state, 2).ok());
  CHECK(executor.running());

  std::uint64_t first{};
  frame_packet packet;
  packet.width = 1;
  REQUIRE(executor.submit(std::move(packet), first).ok());
  {
    std::unique_lock lock(state.mutex);
    state.condition.wait(lock, [&] { return state.first_started; });
  }

  std::uint64_t second{};
  std::uint64_t third{};
  std::uint64_t fourth{};
  std::uint64_t command{};
  packet.width = 2;
  REQUIRE(executor.submit(std::move(packet), second).ok());
  packet.width = 3;
  REQUIRE(executor.submit(std::move(packet), third).ok());
  REQUIRE(executor.submit_command(execute_command, &state, command).ok());
  packet.width = 4;
  REQUIRE(executor.submit(std::move(packet), fourth).ok());
  {
    std::lock_guard lock(state.mutex);
    state.release_first = true;
    state.condition.notify_all();
  }
  REQUIRE(executor.flush().ok());

  std::vector<frame_completion> completions;
  frame_completion completion;
  while (executor.try_take_completion(completion))
    completions.push_back(completion);
  REQUIRE(completions.size() == 4);
  CHECK(std::ranges::count_if(completions, [](const auto& value) { return value.dropped; }) == 1);
  const auto dropped =
      std::ranges::find_if(completions, [](const auto& value) { return value.dropped; });
  REQUIRE(dropped != completions.end());
  CHECK(dropped->sequence == third);
  CHECK(state.executed_widths == std::vector<std::uint32_t>{1, 2, 99, 4});

  render_command_completion command_completion;
  REQUIRE(executor.try_take_command_completion(command_completion));
  CHECK(command_completion.sequence == command);
  CHECK(command_completion.status.ok());
  CHECK_FALSE(executor.try_take_command_completion(command_completion));

  executor.stop();
  CHECK_FALSE(executor.running());
  CHECK(executor.submit({}, first) == granit::result::not_ready);
  CHECK(executor.submit_command(execute_command, &state, command) == granit::result::not_ready);
}

TEST_CASE("线程帧执行器拒绝空命令") {
  using namespace granit::example::model_viewer;
  threaded_frame_executor executor;
  REQUIRE(executor.initialize(execute_frame, nullptr).ok());
  std::uint64_t sequence{};
  CHECK(executor.submit_command(nullptr, nullptr, sequence) == granit::result::invalid_argument);
}
