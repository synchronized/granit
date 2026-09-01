// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "model_viewer/performance_history.h"

#include <catch2/catch_all.hpp>

TEST_CASE("性能历史限制为 240 帧并计算分位数", "[example][model-viewer][performance]") {
  granit::example::model_viewer::performance_history history;
  for (int value = 1; value <= 300; ++value)
    history.push({.frames_per_second = static_cast<float>(value),
                  .cpu_frame_ms = static_cast<float>(value),
                  .gpu_frame_ms = static_cast<float>(value),
                  .gpu_timing_available = value % 2 == 0});
  REQUIRE(history.size() == 240);
  const auto summary = history.summarize();
  CHECK(summary.cpu_frame_ms.sample_count == 240);
  CHECK(summary.cpu_frame_ms.maximum == 300.0F);
  CHECK(summary.cpu_frame_ms.p50 == 181.0F);
  CHECK(summary.gpu_frame_ms.sample_count == 120);
  CHECK(summary.gpu_frame_ms.maximum == 300.0F);
}

TEST_CASE("性能历史不使用不可用 GPU 零值", "[example][model-viewer][performance]") {
  granit::example::model_viewer::performance_history history;
  history.push({.cpu_frame_ms = 2.0F});
  const auto summary = history.summarize();
  CHECK(summary.cpu_frame_ms.sample_count == 1);
  CHECK(summary.gpu_frame_ms.sample_count == 0);
}
