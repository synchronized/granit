// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_PIPELINE_DYNAMIC_UNIFORM_ARENA_H_
#define GRANIT_PIPELINE_DYNAMIC_UNIFORM_ARENA_H_

#include <cstdint>

namespace granit::pipeline::detail {

enum class uniform_arena_error : std::uint8_t {
  none,
  invalid_alignment,
  binding_too_large,
  numeric_overflow,
};

struct uniform_arena_allocation {
  std::uint64_t offset{};
  std::uint64_t size{};
};

/** 只负责确定性 Offset 规划；GPU Buffer 和帧槽生命周期由 Render Pipeline 管理。 */
class dynamic_uniform_arena_plan {
public:
  [[nodiscard]] uniform_arena_error initialize(std::uint64_t alignment,
                                               std::uint64_t max_binding_size,
                                               std::uint64_t initial_capacity) noexcept;
  void rewind() noexcept { cursor_ = 0; }
  [[nodiscard]] uniform_arena_error allocate(std::uint64_t size,
                                             uniform_arena_allocation& output) noexcept;

  [[nodiscard]] std::uint64_t alignment() const noexcept { return alignment_; }
  [[nodiscard]] std::uint64_t capacity() const noexcept { return capacity_; }
  [[nodiscard]] std::uint64_t used() const noexcept { return cursor_; }

private:
  std::uint64_t alignment_{1};
  std::uint64_t max_binding_size_{};
  std::uint64_t capacity_{};
  std::uint64_t cursor_{};
};

} // namespace granit::pipeline::detail

#endif
