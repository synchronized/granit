// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "core/async_operation_state.h"

#include <atomic>
#include <catch2/catch_all.hpp>
#include <thread>

TEST_CASE("异步状态允许完成线程与取消查询线程竞争", "[async][concurrency]") {
  for (int iteration = 0; iteration < 100; ++iteration) {
    granit::detail::async_operation_state_machine operation;
    REQUIRE(operation.begin());
    std::atomic<bool> start{false};
    std::jthread worker{[&] {
      while (!start.load())
        std::this_thread::yield();
      operation.complete(GRANIT_SUCCESS);
    }};
    start.store(true);
    static_cast<void>(operation.request_cancel());
    static_cast<void>(operation.status());
    worker.join();
    const auto status = operation.status();
    CHECK(status.state == GRANIT_ASYNC_OPERATION_STATE_SUCCEEDED);
    CHECK(status.result == GRANIT_SUCCESS);
  }
}
