// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_EXAMPLES_COMMON_TASKS_TASK_SYSTEM_H_
#define GRANIT_EXAMPLES_COMMON_TASKS_TASK_SYSTEM_H_

#include "tasks/executor.h"

#include <cstddef>
#include <cstdint>

namespace granit::example::tasks {

struct task_system_desc {
  /** 零表示在调用线程内执行工作；completion 仍等待 pump_main() 发布。 */
  std::uint32_t worker_count{1};
};

/** 示例拥有的任务系统；不接管业务对象生命周期。 */
class task_system final {
public:
  task_system() = default;
  ~task_system() { stop(); }
  task_system(const task_system&) = delete;
  task_system& operator=(const task_system&) = delete;

  [[nodiscard]] granit::result initialize(const task_system_desc& desc = {}) noexcept;
  [[nodiscard]] executor& worker() noexcept;
  [[nodiscard]] main_thread_executor& main() noexcept { return main_; }
  [[nodiscard]] std::size_t pump_main(
      std::size_t maximum = static_cast<std::size_t>(-1)) noexcept;
  [[nodiscard]] granit::result wait_idle() noexcept;
  void stop() noexcept;
  [[nodiscard]] bool running() const noexcept { return running_; }
  [[nodiscard]] bool threaded() const noexcept { return threaded_; }

private:
  inline_executor inline_;
  thread_pool_executor workers_;
  main_thread_executor main_;
  bool running_{};
  bool threaded_{};
};

} // namespace granit::example::tasks

#endif
