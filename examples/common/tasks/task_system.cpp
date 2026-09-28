// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "tasks/task_system.h"

namespace granit::example::tasks {

granit::result task_system::initialize(const task_system_desc& desc) noexcept {
  if (running_)
    return granit::result::invalid_argument;
  if (desc.worker_count > 0) {
    const auto result = workers_.initialize(desc.worker_count);
    if (result.failed())
      return result;
    threaded_ = true;
  }
  running_ = true;
  return granit::result::success;
}

executor& task_system::worker() noexcept {
  return threaded_ ? static_cast<executor&>(workers_) : static_cast<executor&>(inline_);
}

std::size_t task_system::pump_main(std::size_t maximum) noexcept {
  return running_ ? main_.pump(maximum) : 0;
}

granit::result task_system::wait_idle() noexcept {
  if (!running_)
    return granit::result::cancelled;
  return threaded_ ? workers_.wait_idle() : granit::result::success;
}

void task_system::stop() noexcept {
  if (!running_)
    return;
  if (threaded_)
    workers_.stop();
  inline_.stop();
  main_.stop();
  running_ = false;
}

} // namespace granit::example::tasks
