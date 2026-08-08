// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_CORE_LIFECYCLE_VALIDATION_H_
#define GRANIT_CORE_LIFECYCLE_VALIDATION_H_

#include <array>
#include <cstddef>
#include <cstdint>

#include <granit/renderer.h>

namespace granit::detail {

enum class lifecycle_resource_type : std::uint8_t {
  buffer,
  texture,
  texture_view,
  sampler,
  surface,
  swapchain,
  count,
};

struct lifecycle_resource_sample {
  granit_handle handle{GRANIT_NULL_HANDLE};
  std::uint64_t creation_sequence{};
};

struct lifecycle_resource_summary {
  static constexpr std::size_t maximum_samples = 8;

  std::uint64_t count{};
  std::size_t sample_count{};
  std::array<lifecycle_resource_sample, maximum_samples> samples{};
};

/** Renderer 销毁时收集的固定容量资源快照。 */
class lifecycle_snapshot {
public:
  void add(lifecycle_resource_type type, granit_handle handle,
           std::uint64_t creation_sequence) noexcept;

  [[nodiscard]] bool empty() const noexcept { return total_count_ == 0; }
  [[nodiscard]] std::uint64_t total_count() const noexcept { return total_count_; }
  [[nodiscard]] const lifecycle_resource_summary&
  summary(lifecycle_resource_type type) const noexcept;

private:
  std::array<lifecycle_resource_summary, static_cast<std::size_t>(lifecycle_resource_type::count)>
      summaries_{};
  std::uint64_t total_count_{};
};

/** 在 Registry 锁外输出有界的生命周期诊断。 */
void write_lifecycle_diagnostic(granit_renderer renderer, std::uint32_t domain,
                                const lifecycle_snapshot& snapshot) noexcept;

} // namespace granit::detail

#endif
