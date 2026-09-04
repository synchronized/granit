// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "model_viewer/performance_history.h"

#include <algorithm>
#include <cmath>

namespace granit::example::model_viewer {
namespace {

template <typename Selector, typename Filter>
metric_summary
summarize_metric(const std::array<performance_sample, performance_history::capacity>& samples,
                 std::size_t count, Selector selector, Filter filter) noexcept {
  std::array<float, performance_history::capacity> values{};
  std::size_t value_count = 0;
  for (std::size_t index = 0; index < count; ++index) {
    const auto& sample = samples[index];
    const auto value = selector(sample);
    if (filter(sample) && std::isfinite(value) && value >= 0.0F)
      values[value_count++] = value;
  }
  if (value_count == 0)
    return {};
  std::sort(values.begin(), values.begin() + static_cast<std::ptrdiff_t>(value_count));
  const auto percentile = [&](std::size_t numerator) {
    const auto index = ((value_count - 1) * numerator + 99) / 100;
    return values[index];
  };
  return {.p50 = percentile(50),
          .p95 = percentile(95),
          .maximum = values[value_count - 1],
          .sample_count = value_count};
}

} // namespace

void performance_history::push(performance_sample sample) noexcept {
  samples_[next_] = sample;
  next_ = (next_ + 1) % capacity;
  size_ = std::min(size_ + 1, capacity);
}

void performance_history::clear() noexcept {
  next_ = 0;
  size_ = 0;
}

performance_summary performance_history::summarize() const noexcept {
  const auto all = [](const performance_sample&) { return true; };
  const auto gpu = [](const performance_sample& sample) { return sample.gpu_timing_available; };
  return {
      .frames_per_second = summarize_metric(
          samples_, size_, [](const auto& s) { return s.frames_per_second; }, all),
      .cpu_frame_ms = summarize_metric(
          samples_, size_, [](const auto& s) { return s.cpu_frame_ms; }, all),
      .render_queue_wait_ms = summarize_metric(
          samples_, size_, [](const auto& s) { return s.render_queue_wait_ms; }, all),
      .frame_slot_wait_ms = summarize_metric(
          samples_, size_, [](const auto& s) { return s.frame_slot_wait_ms; }, all),
      .present_wait_ms = summarize_metric(
          samples_, size_, [](const auto& s) { return s.present_wait_ms; }, all),
      .gpu_frame_ms = summarize_metric(
          samples_, size_, [](const auto& s) { return s.gpu_frame_ms; }, gpu),
  };
}

} // namespace granit::example::model_viewer
