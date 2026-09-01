// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "pipeline/dynamic_uniform_arena.h"

#include <catch2/catch_all.hpp>

#include <cstdint>
#include <limits>

using granit::pipeline::detail::dynamic_uniform_arena_plan;
using granit::pipeline::detail::uniform_arena_allocation;
using granit::pipeline::detail::uniform_arena_error;

TEST_CASE("动态 Uniform Arena 按设备限制对齐并增长", "[pipeline][uniform-arena]") {
  dynamic_uniform_arena_plan arena;
  REQUIRE(arena.initialize(256, 64 * 1024, 256) == uniform_arena_error::none);

  uniform_arena_allocation frame;
  uniform_arena_allocation first_object;
  uniform_arena_allocation second_object;
  REQUIRE(arena.allocate(112, frame) == uniform_arena_error::none);
  REQUIRE(arena.allocate(144, first_object) == uniform_arena_error::none);
  REQUIRE(arena.allocate(144, second_object) == uniform_arena_error::none);

  CHECK(frame.offset == 0);
  CHECK(first_object.offset == 256);
  CHECK(second_object.offset == 512);
  CHECK(arena.used() == 656);
  CHECK(arena.capacity() >= arena.used());
}

TEST_CASE("动态 Uniform Arena 回卷帧槽但保留容量", "[pipeline][uniform-arena]") {
  dynamic_uniform_arena_plan arena;
  REQUIRE(arena.initialize(64, 1024, 64) == uniform_arena_error::none);
  uniform_arena_allocation allocation;
  REQUIRE(arena.allocate(65, allocation) == uniform_arena_error::none);
  const auto grown_capacity = arena.capacity();

  arena.rewind();
  REQUIRE(arena.allocate(32, allocation) == uniform_arena_error::none);
  CHECK(allocation.offset == 0);
  CHECK(arena.capacity() == grown_capacity);
}

TEST_CASE("动态 Uniform Arena 拒绝非法限制和溢出", "[pipeline][uniform-arena]") {
  dynamic_uniform_arena_plan arena;
  CHECK(arena.initialize(0, 1024, 0) == uniform_arena_error::invalid_alignment);
  CHECK(arena.initialize(3, 1024, 0) == uniform_arena_error::invalid_alignment);
  REQUIRE(arena.initialize(256, 128, 0) == uniform_arena_error::none);

  uniform_arena_allocation unchanged{7, 9};
  CHECK(arena.allocate(0, unchanged) == uniform_arena_error::binding_too_large);
  CHECK(arena.allocate(129, unchanged) == uniform_arena_error::binding_too_large);
  CHECK(unchanged.offset == 7);
  CHECK(unchanged.size == 9);

  REQUIRE(arena.initialize(std::uint64_t{1} << 63, std::numeric_limits<std::uint64_t>::max(), 0) ==
          uniform_arena_error::none);
  REQUIRE(arena.allocate(1, unchanged) == uniform_arena_error::none);
  REQUIRE(arena.allocate(1, unchanged) == uniform_arena_error::none);
  CHECK(arena.allocate(1, unchanged) == uniform_arena_error::numeric_overflow);
}
