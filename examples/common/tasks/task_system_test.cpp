// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "tasks/task_system.h"

#include <catch2/catch_all.hpp>

#include <stdexcept>
#include <vector>

TEST_CASE("main executor defers work and preserves FIFO order") {
  granit::example::tasks::main_thread_executor executor;
  std::vector<int> order;
  REQUIRE(executor.post([&order] { order.push_back(1); }).ok());
  REQUIRE(executor.post([&order] { order.push_back(2); }).ok());
  REQUIRE(order.empty());
  REQUIRE(executor.pump(1) == 1);
  REQUIRE(order == std::vector{1});
  REQUIRE(executor.pump() == 1);
  REQUIRE(order == std::vector{1, 2});
}

TEST_CASE("inline work still publishes completion later") {
  granit::example::tasks::task_system tasks;
  REQUIRE(tasks.initialize({.worker_count = 0}).ok());
  bool work_ran = false;
  bool completion_ran = false;
  const auto submitted = granit::example::tasks::submit(
      tasks.worker(), tasks.main(),
      [&work_ran] {
        work_ran = true;
        return granit::result::success;
      },
      [&completion_ran](granit::result result) {
        REQUIRE(result.ok());
        completion_ran = true;
      });
  REQUIRE(submitted.ok());
  REQUIRE(work_ran);
  REQUIRE_FALSE(completion_ran);
  REQUIRE(tasks.pump_main() == 1);
  REQUIRE(completion_ran);
}

TEST_CASE("task exceptions become result values") {
  granit::example::tasks::task_system tasks;
  REQUIRE(tasks.initialize({.worker_count = 0}).ok());
  granit::result completion_result;
  REQUIRE(granit::example::tasks::submit(
              tasks.worker(), tasks.main(),
              []() -> granit::result { throw std::runtime_error{"failure"}; },
              [&completion_result](granit::result result) { completion_result = result; })
              .ok());
  REQUIRE(tasks.pump_main() == 1);
  REQUIRE(completion_result == granit::result::internal);
}

#if !defined(__EMSCRIPTEN__) || defined(__EMSCRIPTEN_PTHREADS__)
TEST_CASE("thread pool executes work before main completion") {
  granit::example::tasks::task_system tasks;
  REQUIRE(tasks.initialize({.worker_count = 2}).ok());
  bool completion_ran = false;
  REQUIRE(granit::example::tasks::submit(
              tasks.worker(), tasks.main(), [] { return granit::result::success; },
              [&completion_ran](granit::result result) {
                REQUIRE(result.ok());
                completion_ran = true;
              })
              .ok());
  REQUIRE(tasks.wait_idle().ok());
  REQUIRE_FALSE(completion_ran);
  REQUIRE(tasks.pump_main() == 1);
  REQUIRE(completion_ran);
}
#endif

TEST_CASE("stopped executors reject new tasks") {
  granit::example::tasks::task_system tasks;
  REQUIRE(tasks.initialize({.worker_count = 0}).ok());
  tasks.stop();
  REQUIRE(tasks.worker().post([] {}) == granit::result::cancelled);
  REQUIRE(tasks.main().post([] {}) == granit::result::cancelled);
  REQUIRE(tasks.pump_main() == 0);
}
