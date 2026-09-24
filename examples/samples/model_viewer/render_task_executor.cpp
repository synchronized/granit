// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "render_task_executor.h"

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <new>
#include <thread>
#include <utility>

namespace granit::example::model_viewer {

inline_render_task_executor::inline_render_task_executor(render_frame_callback callback) noexcept
    : callback_(std::move(callback)) {}

granit::result inline_render_task_executor::initialize(render_frame_callback callback) noexcept {
  if (!callback || callback_)
    return granit::result::invalid_argument;
  callback_ = std::move(callback);
  return granit::result::success;
}

granit::result inline_render_task_executor::submit(frame_packet packet,
                                                   frame_execution_result& output) {
  output = {};
  if (callback_ == nullptr)
    return granit::result::invalid_argument;
  return callback_(std::move(packet), output);
}

granit::result inline_render_task_executor::submit_frame(frame_packet packet,
                                                         std::uint64_t& sequence) noexcept {
  if (!callback_)
    return granit::result::not_ready;
  try {
    frame_completion completion;
    completion.sequence = next_sequence_++;
    completion.status = callback_(std::move(packet), completion.execution);
    sequence = completion.sequence;
    completed_frames_.push_back(std::move(completion));
    stats_.pending_high_watermark = std::max<std::size_t>(stats_.pending_high_watermark, 1);
    return granit::result::success;
  } catch (const std::bad_alloc&) {
    return granit::result::out_of_memory;
  } catch (...) {
    return granit::result::internal;
  }
}

granit::result inline_render_task_executor::submit_control(render_control_task task,
                                                           std::uint64_t& sequence) noexcept {
  if (!callback_)
    return granit::result::not_ready;
  if (!task)
    return granit::result::invalid_argument;
  try {
    render_task_completion completion;
    completion.sequence = next_sequence_++;
    try {
      completion.status = task();
    } catch (const std::bad_alloc&) {
      completion.status = granit::result::out_of_memory;
    } catch (...) {
      completion.status = granit::result::internal;
    }
    sequence = completion.sequence;
    completed_controls_.push_back(completion);
    stats_.pending_high_watermark = std::max<std::size_t>(stats_.pending_high_watermark, 1);
    return granit::result::success;
  } catch (const std::bad_alloc&) {
    return granit::result::out_of_memory;
  }
}

bool inline_render_task_executor::try_take_frame_completion(frame_completion& completion) noexcept {
  if (completed_frames_.empty())
    return false;
  completion = std::move(completed_frames_.front());
  completed_frames_.pop_front();
  return true;
}

bool inline_render_task_executor::try_take_control_completion(
    render_task_completion& completion) noexcept {
  if (completed_controls_.empty())
    return false;
  completion = std::move(completed_controls_.front());
  completed_controls_.pop_front();
  return true;
}

bool inline_render_task_executor::can_submit_frame() const noexcept {
  return static_cast<bool>(callback_);
}

void inline_render_task_executor::record_skipped_frame_build() noexcept {
  if (callback_)
    ++stats_.skipped_frame_builds;
}

render_task_queue_stats inline_render_task_executor::query_queue_stats() const noexcept {
  return callback_ ? stats_ : render_task_queue_stats{};
}

granit::result inline_render_task_executor::run_task(render_control_task task) noexcept {
  if (!task)
    return granit::result::invalid_argument;
  try {
    return task();
  } catch (const std::bad_alloc&) {
    return granit::result::out_of_memory;
  } catch (...) {
    return granit::result::internal;
  }
}

granit::result inline_render_task_executor::flush() noexcept { return granit::result::success; }

void inline_render_task_executor::stop() noexcept {
  callback_ = {};
  completed_frames_.clear();
  completed_controls_.clear();
  stats_ = {};
}

bool inline_render_task_executor::running() const noexcept { return static_cast<bool>(callback_); }

struct threaded_render_task_executor::state {
  enum class task_kind { frame, control };

  struct queued_task {
    task_kind kind{task_kind::frame};
    std::uint64_t sequence{};
    frame_packet packet;
    render_control_task control;
    std::chrono::steady_clock::time_point enqueued_at{};
  };

  std::mutex mutex;
  std::condition_variable work_ready;
  std::condition_variable idle;
  std::deque<queued_task> pending;
  std::deque<frame_completion> completed;
  std::deque<render_task_completion> completed_tasks;
  std::thread worker;
  render_frame_callback callback;
  std::size_t maximum_pending_frames{3};
  std::uint64_t next_sequence{1};
  bool executing{};
  bool stopping{};
  render_task_queue_stats stats;

  void run() noexcept;
};

void threaded_render_task_executor::state::run() noexcept {
  for (;;) {
    queued_task queued;
    {
      std::unique_lock lock(mutex);
      work_ready.wait(lock, [&] { return stopping || !pending.empty(); });
      if (stopping && pending.empty())
        break;
      queued = std::move(pending.front());
      pending.pop_front();
      executing = true;
    }

    frame_completion completion;
    render_task_completion task_completion;
    if (queued.kind == task_kind::frame) {
      completion.sequence = queued.sequence;
      completion.execution.queue_wait_ms =
          std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() -
                                                   queued.enqueued_at)
              .count();
      try {
        completion.status = callback(std::move(queued.packet), completion.execution);
      } catch (...) {
        completion.status = granit::result::internal;
      }
    } else {
      task_completion.sequence = queued.sequence;
      try {
        task_completion.status = queued.control();
      } catch (...) {
        task_completion.status = granit::result::internal;
      }
    }
    {
      std::lock_guard lock(mutex);
      if (queued.kind == task_kind::frame)
        completed.push_back(std::move(completion));
      else
        completed_tasks.push_back(std::move(task_completion));
      executing = false;
      if (pending.empty())
        idle.notify_all();
    }
  }
}

threaded_render_task_executor::threaded_render_task_executor() = default;

threaded_render_task_executor::~threaded_render_task_executor() { stop(); }

granit::result threaded_render_task_executor::initialize(render_frame_callback callback) noexcept {
  return initialize(std::move(callback), 3);
}

granit::result
threaded_render_task_executor::initialize(render_frame_callback callback,
                                          std::size_t maximum_pending_frames) noexcept {
  if (!callback || maximum_pending_frames == 0 || state_)
    return granit::result::invalid_argument;
  try {
    auto candidate = std::make_unique<state>();
    candidate->callback = std::move(callback);
    candidate->maximum_pending_frames = maximum_pending_frames;
    candidate->worker = std::thread(&state::run, candidate.get());
    state_ = std::move(candidate);
    return granit::result::success;
  } catch (const std::bad_alloc&) {
    return granit::result::out_of_memory;
  } catch (...) {
    return granit::result::initialization_failed;
  }
}

granit::result threaded_render_task_executor::submit_frame(frame_packet packet,
                                                           std::uint64_t& sequence) noexcept {
  if (!state_)
    return granit::result::not_ready;
  try {
    std::lock_guard lock(state_->mutex);
    if (state_->stopping)
      return granit::result::not_ready;
    sequence = state_->next_sequence++;
    const auto pending_frames = static_cast<std::size_t>(std::ranges::count_if(
        state_->pending, [](const auto& task) { return task.kind == state::task_kind::frame; }));
    if (pending_frames >= state_->maximum_pending_frames) {
      const auto replace = std::ranges::find_if(
          state_->pending, [](const auto& task) { return task.kind == state::task_kind::frame; });
      auto dropped = std::move(*replace);
      state_->pending.erase(replace);
      state_->completed.push_back({.sequence = dropped.sequence,
                                   .status = granit::result::not_ready,
                                   .execution = {},
                                   .dropped = true});
      ++state_->stats.replaced_frames;
      // 被替换帧尚未开始执行，队列滞留时长就是它等待渲染线程的空闲时间。
      state_->stats.render_lag_ms += std::chrono::duration<float, std::milli>(
                                         std::chrono::steady_clock::now() - dropped.enqueued_at)
                                         .count();
    }
    state_->pending.push_back({.kind = state::task_kind::frame,
                               .sequence = sequence,
                               .packet = std::move(packet),
                               .control = {},
                               .enqueued_at = std::chrono::steady_clock::now()});
    state_->stats.pending_high_watermark =
        std::max(state_->stats.pending_high_watermark, state_->pending.size());
    state_->work_ready.notify_one();
    return granit::result::success;
  } catch (const std::bad_alloc&) {
    return granit::result::out_of_memory;
  } catch (...) {
    return granit::result::internal;
  }
}

granit::result threaded_render_task_executor::submit(frame_packet packet,
                                                     frame_execution_result& output) {
  if (!state_)
    return granit::result::not_ready;
  try {
    auto owned = std::make_shared<frame_packet>(std::move(packet));
    return run_task([context = state_.get(), owned = std::move(owned), &output]() mutable {
      return context->callback(std::move(*owned), output);
    });
  } catch (const std::bad_alloc&) {
    return granit::result::out_of_memory;
  } catch (...) {
    return granit::result::internal;
  }
}

granit::result threaded_render_task_executor::submit_control(render_control_task task,
                                                             std::uint64_t& sequence) noexcept {
  if (!state_)
    return granit::result::not_ready;
  if (!task)
    return granit::result::invalid_argument;
  try {
    std::lock_guard lock(state_->mutex);
    if (state_->stopping)
      return granit::result::not_ready;
    // 命令不可替换；固定额外余量防止错误生产者无限占用内存。
    if (state_->pending.size() >= state_->maximum_pending_frames + 8)
      return granit::result::not_ready;
    sequence = state_->next_sequence++;
    state_->pending.push_back({.kind = state::task_kind::control,
                               .sequence = sequence,
                               .packet = {},
                               .control = std::move(task),
                               .enqueued_at = std::chrono::steady_clock::now()});
    state_->stats.pending_high_watermark =
        std::max(state_->stats.pending_high_watermark, state_->pending.size());
    state_->work_ready.notify_one();
    return granit::result::success;
  } catch (const std::bad_alloc&) {
    return granit::result::out_of_memory;
  } catch (...) {
    return granit::result::internal;
  }
}

bool threaded_render_task_executor::can_submit_frame() const noexcept {
  if (!state_)
    return false;
  std::lock_guard lock(state_->mutex);
  if (state_->stopping)
    return false;
  const auto pending_frames = std::ranges::count_if(
      state_->pending, [](const auto& task) { return task.kind == state::task_kind::frame; });
  return static_cast<std::size_t>(pending_frames) < state_->maximum_pending_frames;
}

void threaded_render_task_executor::record_skipped_frame_build() noexcept {
  if (!state_)
    return;
  std::lock_guard lock(state_->mutex);
  if (!state_->stopping)
    ++state_->stats.skipped_frame_builds;
}

bool threaded_render_task_executor::try_take_frame_completion(
    frame_completion& completion) noexcept {
  if (!state_)
    return false;
  std::lock_guard lock(state_->mutex);
  if (state_->completed.empty())
    return false;
  completion = std::move(state_->completed.front());
  state_->completed.pop_front();
  return true;
}

bool threaded_render_task_executor::try_take_control_completion(
    render_task_completion& completion) noexcept {
  if (!state_)
    return false;
  std::lock_guard lock(state_->mutex);
  if (state_->completed_tasks.empty())
    return false;
  completion = std::move(state_->completed_tasks.front());
  state_->completed_tasks.pop_front();
  return true;
}

render_task_queue_stats threaded_render_task_executor::query_queue_stats() const noexcept {
  if (!state_)
    return {};
  std::lock_guard lock(state_->mutex);
  return state_->stats;
}

granit::result threaded_render_task_executor::flush() noexcept {
  if (!state_)
    return granit::result::not_ready;
  std::unique_lock lock(state_->mutex);
  state_->idle.wait(lock, [&] { return state_->pending.empty() && !state_->executing; });
  return granit::result::success;
}

granit::result threaded_render_task_executor::run_task(render_control_task task) noexcept {
  std::uint64_t sequence{};
  auto result = submit_control(std::move(task), sequence);
  if (result.failed())
    return result;
  result = flush();
  if (result.failed())
    return result;
  std::lock_guard lock(state_->mutex);
  const auto completion =
      std::ranges::find_if(state_->completed_tasks,
                           [sequence](const auto& value) { return value.sequence == sequence; });
  if (completion == state_->completed_tasks.end())
    return granit::result::internal;
  const auto status = completion->status;
  state_->completed_tasks.erase(completion);
  return status;
}

void threaded_render_task_executor::stop() noexcept {
  if (!state_)
    return;
  static_cast<void>(flush());
  {
    std::lock_guard lock(state_->mutex);
    state_->stopping = true;
    state_->work_ready.notify_all();
  }
  if (state_->worker.joinable())
    state_->worker.join();
  state_.reset();
}

bool threaded_render_task_executor::running() const noexcept {
  if (!state_)
    return false;
  std::lock_guard lock(state_->mutex);
  return !state_->stopping;
}

} // namespace granit::example::model_viewer
