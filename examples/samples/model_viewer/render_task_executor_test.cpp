// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "render_task_executor.h"

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
  state.width = packet.viewer.width;
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
  state.executed_widths.push_back(packet.viewer.width);
  if (packet.viewer.width == 1) {
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
  granit::example::model_viewer::inline_render_task_executor executor(
      [&state](auto&& packet, auto& output) {
        return execute_frame(std::move(packet), output, &state);
      });
  granit::example::model_viewer::frame_packet packet;
  packet.viewer.width = 1280;
  granit::example::model_viewer::frame_execution_result output;

  CHECK(executor.submit(std::move(packet), output) == granit::result::not_ready);
  CHECK(state.called);
  CHECK(state.width == 1280);
  CHECK(output.needs_recreate);
  CHECK(output.acquire_wait_ms == 2.0F);
  CHECK(executor.flush().ok());
}

TEST_CASE("同步帧执行器拒绝空回调") {
  granit::example::model_viewer::inline_render_task_executor executor;
  granit::example::model_viewer::frame_execution_result output;
  output.needs_recreate = true;

  CHECK(executor.submit({}, output) == granit::result::invalid_argument);
  CHECK_FALSE(output.needs_recreate);
}

TEST_CASE("同步渲染任务执行器持久初始化并执行控制任务") {
  using namespace granit::example::model_viewer;
  inline_render_task_executor executor;
  callback_state state;
  REQUIRE(executor
              .initialize([&state](auto&& packet, auto& output) {
                return execute_frame(std::move(packet), output, &state);
              })
              .ok());
  CHECK(executor.initialize({}) == granit::result::invalid_argument);
  CHECK(executor
            .run_task([&state] {
              state.width = 99;
              return granit::result::success;
            })
            .ok());
  CHECK(state.width == 99);
  CHECK(executor.run_task({}) == granit::result::invalid_argument);
}

TEST_CASE("帧执行策略通过统一接口保持相同行为") {
  using namespace granit::example::model_viewer;
  callback_state state;

  const auto verify = [&state](render_task_executor& executor) {
    REQUIRE(executor
                .initialize([&state](auto&& packet, auto& output) {
                  return execute_frame(std::move(packet), output, &state);
                })
                .ok());
    frame_packet packet;
    packet.viewer.width = 640;
    frame_execution_result output;
    CHECK(executor.submit(std::move(packet), output) == granit::result::not_ready);
    CHECK(state.called);
    CHECK(state.width == 640);
    CHECK(output.needs_recreate);
    frame_packet queued;
    queued.viewer.width = 800;
    std::uint64_t frame_sequence{};
    REQUIRE(executor.submit_frame(std::move(queued), frame_sequence).ok());
    std::uint64_t control_sequence{};
    REQUIRE(executor
                .submit_control(
                    [&state] {
                      state.width = 900;
                      return granit::result::success;
                    },
                    control_sequence)
                .ok());
    REQUIRE(executor.flush().ok());
    frame_completion frame_result;
    REQUIRE(executor.try_take_frame_completion(frame_result));
    CHECK(frame_result.sequence == frame_sequence);
    CHECK(frame_result.status == granit::result::not_ready);
    CHECK(frame_result.execution.needs_recreate);
    render_task_completion control_result;
    REQUIRE(executor.try_take_control_completion(control_result));
    CHECK(control_result.sequence == control_sequence);
    CHECK(control_result.status.ok());
    CHECK(state.width == 900);
    CHECK(executor.can_submit_frame());
    CHECK(executor.query_queue_stats().pending_high_watermark >= 1);
    CHECK(executor.running());
    executor.stop();
    CHECK_FALSE(executor.running());
  };

  SECTION("调用线程执行") {
    inline_render_task_executor executor;
    verify(executor);
  }
  SECTION("专用线程执行") {
    threaded_render_task_executor executor;
    verify(executor);
  }
}

TEST_CASE("线程帧执行器限制待处理队列并回报被替换帧") {
  using namespace granit::example::model_viewer;
  blocking_callback_state state;
  threaded_render_task_executor executor;
  REQUIRE(executor
              .initialize(
                  [&state](auto&& packet, auto& output) {
                    return execute_blocking_frame(std::move(packet), output, &state);
                  },
                  2)
              .ok());
  CHECK(executor.running());

  std::uint64_t first{};
  frame_packet packet;
  packet.viewer.width = 1;
  REQUIRE(executor.submit_frame(std::move(packet), first).ok());
  {
    std::unique_lock lock(state.mutex);
    state.condition.wait(lock, [&] { return state.first_started; });
  }

  std::uint64_t second{};
  std::uint64_t third{};
  std::uint64_t fourth{};
  std::uint64_t command{};
  packet.viewer.width = 2;
  REQUIRE(executor.submit_frame(std::move(packet), second).ok());
  packet.viewer.width = 3;
  REQUIRE(executor.submit_frame(std::move(packet), third).ok());
  CHECK_FALSE(executor.can_submit_frame());
  executor.record_skipped_frame_build();
  REQUIRE(executor.submit_control([&state] { return execute_command(&state); }, command).ok());
  packet.viewer.width = 4;
  REQUIRE(executor.submit_frame(std::move(packet), fourth).ok());
  {
    std::lock_guard lock(state.mutex);
    state.release_first = true;
    state.condition.notify_all();
  }
  REQUIRE(executor.flush().ok());

  std::vector<frame_completion> completions;
  frame_completion completion;
  while (executor.try_take_frame_completion(completion))
    completions.push_back(completion);
  REQUIRE(completions.size() == 4);
  CHECK(std::ranges::count_if(completions, [](const auto& value) { return value.dropped; }) == 1);
  const auto dropped =
      std::ranges::find_if(completions, [](const auto& value) { return value.dropped; });
  REQUIRE(dropped != completions.end());
  CHECK(dropped->sequence == second);
  CHECK(state.executed_widths == std::vector<std::uint32_t>{1, 3, 99, 4});
  const auto queue_stats = executor.query_queue_stats();
  CHECK(queue_stats.pending_high_watermark == 3);
  CHECK(queue_stats.replaced_frames == 1);
  CHECK(queue_stats.skipped_frame_builds == 1);
  CHECK(queue_stats.render_lag_ms >= 0.0F);
  CHECK(completions.front().execution.queue_wait_ms >= 0.0F);

  render_task_completion command_completion;
  REQUIRE(executor.try_take_control_completion(command_completion));
  CHECK(command_completion.sequence == command);
  CHECK(command_completion.status.ok());
  CHECK_FALSE(executor.try_take_control_completion(command_completion));

  executor.stop();
  CHECK_FALSE(executor.running());
  CHECK_FALSE(executor.can_submit_frame());
  CHECK(executor.query_queue_stats().pending_high_watermark == 0);
  CHECK(executor.submit_frame({}, first) == granit::result::not_ready);
  CHECK(executor.submit_control([&state] { return execute_command(&state); }, command) ==
        granit::result::not_ready);
}

TEST_CASE("线程帧执行器拒绝空命令") {
  using namespace granit::example::model_viewer;
  threaded_render_task_executor executor;
  REQUIRE(executor
              .initialize([](auto&& packet, auto& output) {
                return execute_frame(std::move(packet), output, nullptr);
              })
              .ok());
  std::uint64_t sequence{};
  CHECK(executor.submit_control({}, sequence) == granit::result::invalid_argument);
  CHECK(executor.run_task({}) == granit::result::invalid_argument);
}

TEST_CASE("线程帧执行器同步等待不可丢弃命令") {
  using namespace granit::example::model_viewer;
  blocking_callback_state state;
  threaded_render_task_executor executor;
  REQUIRE(executor
              .initialize([](auto&& packet, auto& output) {
                return execute_frame(std::move(packet), output, nullptr);
              })
              .ok());

  std::uint64_t earlier_sequence{};
  REQUIRE(
      executor.submit_control([&state] { return execute_command(&state); }, earlier_sequence).ok());
  CHECK(executor.run_task([&state] { return execute_command(&state); }).ok());
  CHECK(state.executed_widths == std::vector<std::uint32_t>{99, 99});
  render_task_completion completion;
  REQUIRE(executor.try_take_control_completion(completion));
  CHECK(completion.sequence == earlier_sequence);
  CHECK(completion.status.ok());
  CHECK_FALSE(executor.try_take_control_completion(completion));
}
