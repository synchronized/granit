// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "core/device_status.h"

#include <catch2/catch_all.hpp>

TEST_CASE("Device Lost 状态一旦发生便保持终止", "[renderer][device-lost]") {
  granit::detail::device_status status;
  CHECK(status.gate() == GRANIT_SUCCESS);
  CHECK(status.observe(GRANIT_ERROR_OUT_OF_MEMORY) == GRANIT_ERROR_OUT_OF_MEMORY);
  CHECK(status.gate() == GRANIT_SUCCESS);
  bool first_loss = false;
  CHECK(status.observe(GRANIT_ERROR_DEVICE_LOST, &first_loss) == GRANIT_ERROR_DEVICE_LOST);
  CHECK(first_loss);
  CHECK(status.gate() == GRANIT_ERROR_DEVICE_LOST);
  CHECK(status.observe(GRANIT_SUCCESS, &first_loss) == GRANIT_ERROR_DEVICE_LOST);
  CHECK_FALSE(first_loss);
  CHECK(status.observe(GRANIT_ERROR_DEVICE_LOST, &first_loss) == GRANIT_ERROR_DEVICE_LOST);
  CHECK_FALSE(first_loss);
}
