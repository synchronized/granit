// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "model_viewer/frame_executor.h"

#include <algorithm>
#include <condition_variable>
#include <deque>
#include <iterator>
#include <mutex>
#include <new>
#include <thread>
#include <utility>

namespace granit::example::model_viewer {

inline_frame_executor::inline_frame_executor(frame_execute_callback callback,
                                             void* user_data) noexcept
    : callback_(callback), user_data_(user_data) {}

granit::result inline_frame_executor::submit(frame_packet packet, frame_execution_result& output) {
  output = {};
  if (callback_ == nullptr)
    return granit::result::invalid_argument;
  return callback_(std::move(packet), output, user_data_);
}

granit::result inline_frame_executor::flush() noexcept { return granit::result::success; }

struct threaded_frame_executor::state {
  enum class task_kind { frame, command };

  struct queued_task {
    task_kind kind{task_kind::frame};
    std::uint64_t sequence{};
    frame_packet packet;
    render_command_callback command{};
    void* command_user_data{};
  };

  std::mutex mutex;
  std::condition_variable work_ready;
  std::condition_variable idle;
  std::deque<queued_task> pending;
  std::deque<frame_completion> completed;
  std::deque<render_command_completion> completed_commands;
  std::thread worker;
  frame_execute_callback callback{};
  void* user_data{};
  std::size_t maximum_pending_frames{3};
  std::uint64_t next_sequence{1};
  bool executing{};
  bool stopping{};

  void run() noexcept;
};

void threaded_frame_executor::state::run() noexcept {
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
    render_command_completion command_completion;
    if (queued.kind == task_kind::frame) {
      completion.sequence = queued.sequence;
      try {
        completion.status = callback(std::move(queued.packet), completion.execution, user_data);
      } catch (...) {
        completion.status = granit::result::internal;
      }
    } else {
      command_completion.sequence = queued.sequence;
      try {
        command_completion.status = queued.command(queued.command_user_data);
      } catch (...) {
        command_completion.status = granit::result::internal;
      }
    }
    {
      std::lock_guard lock(mutex);
      if (queued.kind == task_kind::frame)
        completed.push_back(std::move(completion));
      else
        completed_commands.push_back(std::move(command_completion));
      executing = false;
      if (pending.empty())
        idle.notify_all();
    }
  }
}

threaded_frame_executor::threaded_frame_executor() = default;

threaded_frame_executor::~threaded_frame_executor() { stop(); }

granit::result threaded_frame_executor::initialize(frame_execute_callback callback, void* user_data,
                                                   std::size_t maximum_pending_frames) noexcept {
  if (callback == nullptr || maximum_pending_frames == 0 || state_)
    return granit::result::invalid_argument;
  try {
    auto candidate = std::make_unique<state>();
    candidate->callback = callback;
    candidate->user_data = user_data;
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

granit::result threaded_frame_executor::submit(frame_packet packet,
                                               std::uint64_t& sequence) noexcept {
  if (!state_)
    return granit::result::not_ready;
  try {
    std::lock_guard lock(state_->mutex);
    if (state_->stopping)
      return granit::result::not_ready;
    sequence = state_->next_sequence++;
    const auto pending_frames = static_cast<std::size_t>(
        std::ranges::count_if(state_->pending, [](const auto& task) {
          return task.kind == state::task_kind::frame;
        }));
    if (pending_frames >= state_->maximum_pending_frames) {
      const auto replace = std::ranges::find_if(state_->pending.rbegin(), state_->pending.rend(),
                                                [](const auto& task) {
                                                  return task.kind == state::task_kind::frame;
                                                });
      auto dropped = std::move(*replace);
      state_->pending.erase(std::next(replace).base());
      state_->completed.push_back({.sequence = dropped.sequence,
                                   .status = granit::result::not_ready,
                                   .execution = {},
                                   .dropped = true});
    }
    state_->pending.push_back({.kind = state::task_kind::frame,
                               .sequence = sequence,
                               .packet = std::move(packet),
                               .command = nullptr,
                               .command_user_data = nullptr});
    state_->work_ready.notify_one();
    return granit::result::success;
  } catch (const std::bad_alloc&) {
    return granit::result::out_of_memory;
  } catch (...) {
    return granit::result::internal;
  }
}

granit::result threaded_frame_executor::submit_command(render_command_callback callback,
                                                       void* user_data,
                                                       std::uint64_t& sequence) noexcept {
  if (!state_)
    return granit::result::not_ready;
  if (callback == nullptr)
    return granit::result::invalid_argument;
  try {
    std::lock_guard lock(state_->mutex);
    if (state_->stopping)
      return granit::result::not_ready;
    // 命令不可替换；固定额外余量防止错误生产者无限占用内存。
    if (state_->pending.size() >= state_->maximum_pending_frames + 8)
      return granit::result::not_ready;
    sequence = state_->next_sequence++;
    state_->pending.push_back({.kind = state::task_kind::command,
                               .sequence = sequence,
                               .packet = {},
                               .command = callback,
                               .command_user_data = user_data});
    state_->work_ready.notify_one();
    return granit::result::success;
  } catch (const std::bad_alloc&) {
    return granit::result::out_of_memory;
  } catch (...) {
    return granit::result::internal;
  }
}

bool threaded_frame_executor::try_take_completion(frame_completion& completion) noexcept {
  if (!state_)
    return false;
  std::lock_guard lock(state_->mutex);
  if (state_->completed.empty())
    return false;
  completion = std::move(state_->completed.front());
  state_->completed.pop_front();
  return true;
}

bool threaded_frame_executor::try_take_command_completion(
    render_command_completion& completion) noexcept {
  if (!state_)
    return false;
  std::lock_guard lock(state_->mutex);
  if (state_->completed_commands.empty())
    return false;
  completion = std::move(state_->completed_commands.front());
  state_->completed_commands.pop_front();
  return true;
}

granit::result threaded_frame_executor::flush() noexcept {
  if (!state_)
    return granit::result::not_ready;
  std::unique_lock lock(state_->mutex);
  state_->idle.wait(lock, [&] { return state_->pending.empty() && !state_->executing; });
  return granit::result::success;
}

void threaded_frame_executor::stop() noexcept {
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

bool threaded_frame_executor::running() const noexcept {
  if (!state_)
    return false;
  std::lock_guard lock(state_->mutex);
  return !state_->stopping;
}

} // namespace granit::example::model_viewer
