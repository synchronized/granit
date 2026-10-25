// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "core/async_operation_state.h"

#include <catch2/catch_all.hpp>

TEST_CASE("异步操作从等待进入成功终态") {
  granit::detail::async_operation_state_machine operation;
  auto status = operation.status();
  CHECK(status.state == GRANIT_ASYNC_OPERATION_STATE_PENDING);
  CHECK(status.result == GRANIT_ERROR_NOT_READY);
  CHECK(status.cancel_requested == 0);

  REQUIRE(operation.begin());
  status = operation.status();
  CHECK(status.state == GRANIT_ASYNC_OPERATION_STATE_RUNNING);
  operation.complete(GRANIT_SUCCESS);
  status = operation.status();
  CHECK(status.state == GRANIT_ASYNC_OPERATION_STATE_SUCCEEDED);
  CHECK(status.result == GRANIT_SUCCESS);
}

TEST_CASE("等待中的异步操作可立即取消") {
  granit::detail::async_operation_state_machine operation;
  REQUIRE(operation.request_cancel());
  const auto status = operation.status();
  CHECK(status.state == GRANIT_ASYNC_OPERATION_STATE_CANCELLED);
  CHECK(status.result == GRANIT_ERROR_CANCELLED);
  CHECK(status.cancel_requested == 1);
  CHECK_FALSE(operation.begin());
  operation.complete(GRANIT_SUCCESS);
  CHECK(operation.status().state == GRANIT_ASYNC_OPERATION_STATE_CANCELLED);
}

TEST_CASE("运行中的取消请求不伪装为 GPU 工作已撤销") {
  granit::detail::async_operation_state_machine operation;
  REQUIRE(operation.begin());
  REQUIRE(operation.request_cancel());
  auto status = operation.status();
  CHECK(status.state == GRANIT_ASYNC_OPERATION_STATE_RUNNING);
  CHECK(status.cancel_requested == 1);

  operation.complete(GRANIT_ERROR_DEVICE_LOST);
  status = operation.status();
  CHECK(status.state == GRANIT_ASYNC_OPERATION_STATE_FAILED);
  CHECK(status.result == GRANIT_ERROR_DEVICE_LOST);
  CHECK_FALSE(operation.request_cancel());
}

TEST_CASE("运行中的可中止工作可以确认取消") {
  granit::detail::async_operation_state_machine operation;
  REQUIRE(operation.begin());
  REQUIRE(operation.request_cancel());
  operation.acknowledge_cancel();
  const auto status = operation.status();
  CHECK(status.state == GRANIT_ASYNC_OPERATION_STATE_CANCELLED);
  CHECK(status.result == GRANIT_ERROR_CANCELLED);
}
