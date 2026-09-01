// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_EXAMPLES_COMMON_MODEL_VIEWER_PERFORMANCE_HISTORY_H_
#define GRANIT_EXAMPLES_COMMON_MODEL_VIEWER_PERFORMANCE_HISTORY_H_

#include <array>
#include <cstddef>

namespace granit::example::model_viewer {

struct performance_sample {
  float frames_per_second{};
  float cpu_frame_ms{};
  float frame_slot_wait_ms{};
  float present_wait_ms{};
  float gpu_frame_ms{};
  bool gpu_timing_available{};
};

struct metric_summary {
  float p50{};
  float p95{};
  float maximum{};
  std::size_t sample_count{};
};

struct performance_summary {
  metric_summary frames_per_second;
  metric_summary cpu_frame_ms;
  metric_summary frame_slot_wait_ms;
  metric_summary present_wait_ms;
  metric_summary gpu_frame_ms;
};

class performance_history {
public:
  static constexpr std::size_t capacity = 240;

  void push(performance_sample sample) noexcept;
  void clear() noexcept;
  [[nodiscard]] std::size_t size() const noexcept { return size_; }
  [[nodiscard]] performance_summary summarize() const noexcept;

private:
  std::array<performance_sample, capacity> samples_{};
  std::size_t next_{};
  std::size_t size_{};
};

} // namespace granit::example::model_viewer

#endif
