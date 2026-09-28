// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_EXAMPLES_COMMON_TASKS_EXECUTOR_H_
#define GRANIT_EXAMPLES_COMMON_TASKS_EXECUTOR_H_

#include <granit/core/result.hpp>

#include <cstddef>
#include <functional>
#include <memory>

namespace granit::example::tasks {

using task = std::function<void()>;
using task_work = std::function<granit::result()>;
using task_completion = std::function<void(granit::result)>;

/** 示例私有任务执行入口；实现必须截断异常，且停止后拒绝新任务。 */
class executor {
public:
  virtual ~executor() = default;

  [[nodiscard]] virtual granit::result post(task work) noexcept = 0;
};

/** 在 work executor 执行工作，并把结果延迟发布到 completion executor。 */
[[nodiscard]] granit::result submit(executor& work_executor, executor& completion_executor,
                                    task_work work, task_completion completion) noexcept;

/** 立即执行任务；主要供无 worker 的平台和确定性测试使用。 */
class inline_executor final : public executor {
public:
  [[nodiscard]] granit::result post(task work) noexcept override;
  void stop() noexcept { accepting_ = false; }
  [[nodiscard]] bool accepting() const noexcept { return accepting_; }

private:
  bool accepting_{true};
};

/** 由拥有线程显式泵取的 FIFO completion 队列。 */
class main_thread_executor final : public executor {
public:
  main_thread_executor();
  ~main_thread_executor() override;
  main_thread_executor(const main_thread_executor&) = delete;
  main_thread_executor& operator=(const main_thread_executor&) = delete;

  [[nodiscard]] granit::result post(task work) noexcept override;
  [[nodiscard]] std::size_t pump(std::size_t maximum = static_cast<std::size_t>(-1)) noexcept;
  void stop() noexcept;
  [[nodiscard]] std::size_t pending() const noexcept;
  [[nodiscard]] bool accepting() const noexcept;

private:
  struct implementation;
  std::unique_ptr<implementation> implementation_;
};

/** 固定 worker 数量的 FIFO 后台执行器。 */
class thread_pool_executor final : public executor {
public:
  thread_pool_executor();
  ~thread_pool_executor() override;
  thread_pool_executor(const thread_pool_executor&) = delete;
  thread_pool_executor& operator=(const thread_pool_executor&) = delete;

  [[nodiscard]] granit::result initialize(std::size_t worker_count) noexcept;
  [[nodiscard]] granit::result post(task work) noexcept override;
  [[nodiscard]] granit::result wait_idle() noexcept;
  void stop() noexcept;
  [[nodiscard]] std::size_t pending() const noexcept;
  [[nodiscard]] bool accepting() const noexcept;

private:
  struct implementation;
  std::unique_ptr<implementation> implementation_;
};

} // namespace granit::example::tasks

#endif
