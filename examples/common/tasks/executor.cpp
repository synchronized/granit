// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "tasks/executor.h"

#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <new>
#include <thread>
#include <utility>
#include <vector>

namespace granit::example::tasks {
namespace {

granit::result run(task& work) noexcept {
  try {
    work();
    return granit::result::success;
  } catch (const std::bad_alloc&) {
    return granit::result::out_of_memory;
  } catch (...) {
    return granit::result::internal;
  }
}

granit::result run(task_work& work) noexcept {
  try {
    return work();
  } catch (const std::bad_alloc&) {
    return granit::result::out_of_memory;
  } catch (...) {
    return granit::result::internal;
  }
}

} // namespace

granit::result submit(executor& work_executor, executor& completion_executor, task_work work,
                      task_completion completion) noexcept {
  if (!work || !completion)
    return granit::result::invalid_argument;
  try {
    return work_executor.post(
        [work = std::move(work), completion = std::move(completion), &completion_executor]() mutable {
          const auto result = run(work);
          static_cast<void>(completion_executor.post(
              [completion = std::move(completion), result]() mutable { completion(result); }));
        });
  } catch (const std::bad_alloc&) {
    return granit::result::out_of_memory;
  } catch (...) {
    return granit::result::internal;
  }
}

granit::result inline_executor::post(task work) noexcept {
  if (!accepting_)
    return granit::result::cancelled;
  if (!work)
    return granit::result::invalid_argument;
  return run(work);
}

struct main_thread_executor::implementation {
  mutable std::mutex mutex;
  std::deque<task> tasks;
  bool accepting{true};
};

main_thread_executor::main_thread_executor() : implementation_(std::make_unique<implementation>()) {}

main_thread_executor::~main_thread_executor() { stop(); }

granit::result main_thread_executor::post(task work) noexcept {
  if (!work)
    return granit::result::invalid_argument;
  try {
    std::scoped_lock lock{implementation_->mutex};
    if (!implementation_->accepting)
      return granit::result::cancelled;
    implementation_->tasks.push_back(std::move(work));
    return granit::result::success;
  } catch (const std::bad_alloc&) {
    return granit::result::out_of_memory;
  } catch (...) {
    return granit::result::internal;
  }
}

std::size_t main_thread_executor::pump(std::size_t maximum) noexcept {
  std::size_t completed = 0;
  while (completed < maximum) {
    task work;
    {
      std::scoped_lock lock{implementation_->mutex};
      if (implementation_->tasks.empty())
        break;
      work = std::move(implementation_->tasks.front());
      implementation_->tasks.pop_front();
    }
    static_cast<void>(run(work));
    ++completed;
  }
  return completed;
}

void main_thread_executor::stop() noexcept {
  std::scoped_lock lock{implementation_->mutex};
  implementation_->accepting = false;
  implementation_->tasks.clear();
}

std::size_t main_thread_executor::pending() const noexcept {
  std::scoped_lock lock{implementation_->mutex};
  return implementation_->tasks.size();
}

bool main_thread_executor::accepting() const noexcept {
  std::scoped_lock lock{implementation_->mutex};
  return implementation_->accepting;
}

struct thread_pool_executor::implementation {
  mutable std::mutex mutex;
  std::condition_variable ready;
  std::condition_variable idle;
  std::deque<task> tasks;
  std::vector<std::thread> workers;
  std::size_t active{};
  bool accepting{};
  bool stopping{};

  void worker() noexcept {
    for (;;) {
      task work;
      {
        std::unique_lock lock{mutex};
        ready.wait(lock, [this] { return stopping || !tasks.empty(); });
        if (stopping && tasks.empty())
          return;
        work = std::move(tasks.front());
        tasks.pop_front();
        ++active;
      }
      static_cast<void>(run(work));
      {
        std::scoped_lock lock{mutex};
        --active;
        if (tasks.empty() && active == 0)
          idle.notify_all();
      }
    }
  }
};

thread_pool_executor::thread_pool_executor() : implementation_(std::make_unique<implementation>()) {}

thread_pool_executor::~thread_pool_executor() { stop(); }

granit::result thread_pool_executor::initialize(std::size_t worker_count) noexcept {
  if (worker_count == 0)
    return granit::result::invalid_argument;
#if defined(__EMSCRIPTEN__) && !defined(__EMSCRIPTEN_PTHREADS__)
  return granit::result::unsupported;
#else
  try {
    std::scoped_lock lock{implementation_->mutex};
    if (implementation_->accepting || !implementation_->workers.empty())
      return granit::result::invalid_argument;
    implementation_->accepting = true;
    implementation_->workers.reserve(worker_count);
    for (std::size_t index = 0; index < worker_count; ++index) {
      implementation_->workers.emplace_back(
          [state = implementation_.get()] { state->worker(); });
    }
    return granit::result::success;
  } catch (const std::bad_alloc&) {
    stop();
    return granit::result::out_of_memory;
  } catch (...) {
    stop();
    return granit::result::internal;
  }
#endif
}

granit::result thread_pool_executor::post(task work) noexcept {
  if (!work)
    return granit::result::invalid_argument;
  try {
    {
      std::scoped_lock lock{implementation_->mutex};
      if (!implementation_->accepting)
        return granit::result::cancelled;
      implementation_->tasks.push_back(std::move(work));
    }
    implementation_->ready.notify_one();
    return granit::result::success;
  } catch (const std::bad_alloc&) {
    return granit::result::out_of_memory;
  } catch (...) {
    return granit::result::internal;
  }
}

granit::result thread_pool_executor::wait_idle() noexcept {
  std::unique_lock lock{implementation_->mutex};
  if (!implementation_->accepting)
    return granit::result::cancelled;
  implementation_->idle.wait(
      lock, [this] { return implementation_->tasks.empty() && implementation_->active == 0; });
  return granit::result::success;
}

void thread_pool_executor::stop() noexcept {
  std::vector<std::thread> workers;
  {
    std::scoped_lock lock{implementation_->mutex};
    implementation_->accepting = false;
    implementation_->stopping = true;
    implementation_->tasks.clear();
    workers.swap(implementation_->workers);
  }
  implementation_->ready.notify_all();
  for (auto& worker : workers) {
    if (worker.joinable())
      worker.join();
  }
}

std::size_t thread_pool_executor::pending() const noexcept {
  std::scoped_lock lock{implementation_->mutex};
  return implementation_->tasks.size() + implementation_->active;
}

bool thread_pool_executor::accepting() const noexcept {
  std::scoped_lock lock{implementation_->mutex};
  return implementation_->accepting;
}

} // namespace granit::example::tasks
