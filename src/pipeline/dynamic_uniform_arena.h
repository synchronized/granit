// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_PIPELINE_DYNAMIC_UNIFORM_ARENA_H_
#define GRANIT_PIPELINE_DYNAMIC_UNIFORM_ARENA_H_

#include "material/pbr_draw_inputs.h"
#include "pipeline/material_access.h"

#include <granit/renderer/buffer.hpp>
#include <granit/renderer/pipeline.hpp>

#include <cstdint>
#include <span>
#include <vector>

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

struct dynamic_uniform_binding {
  granit_bind_group frame_group{GRANIT_NULL_HANDLE};
  granit_bind_group object_group{GRANIT_NULL_HANDLE};
  std::uint32_t frame_offset{};
  std::uint32_t object_offset{};
};

/** 按真实 Frame Slot 复用 Buffer，并按材质布局缓存动态 Bind Group。 */
class dynamic_uniform_arena {
public:
  [[nodiscard]] granit_result initialize(granit_renderer renderer) noexcept;
  [[nodiscard]] granit_result begin_frame(std::uint32_t frame_slot,
                                          std::uint32_t frame_slot_count) noexcept;
  [[nodiscard]] granit_result prepare(const material_draw_state& material,
                                      std::span<const std::byte> frame,
                                      std::span<const std::byte> object,
                                      dynamic_uniform_binding& output) noexcept;
  [[nodiscard]] granit_result reset() noexcept;

private:
  struct group_pair {
    granit_bind_group_layout frame_layout{GRANIT_NULL_HANDLE};
    granit_bind_group_layout object_layout{GRANIT_NULL_HANDLE};
    granit::bind_group frame_group;
    granit::bind_group object_group;
  };

  struct frame_slot_state {
    dynamic_uniform_arena_plan plan;
    granit::buffer buffer;
    std::uint64_t buffer_capacity{};
    std::vector<group_pair> groups;
  };

  [[nodiscard]] granit_result ensure_buffer(frame_slot_state& slot) noexcept;
  [[nodiscard]] granit_result acquire_groups(frame_slot_state& slot,
                                             const material_draw_state& material,
                                             group_pair*& output) noexcept;

  granit_renderer renderer_{GRANIT_NULL_HANDLE};
  std::uint64_t alignment_{1};
  std::uint64_t max_binding_size_{};
  std::vector<frame_slot_state> slots_;
  frame_slot_state* current_slot_{};
};

} // namespace granit::pipeline::detail

#endif
