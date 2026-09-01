// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "pipeline/dynamic_uniform_arena.h"

#include <catch2/catch_all.hpp>

#include <granit/renderer/renderer.hpp>

#include <array>
#include <cstdint>
#include <limits>
#include <span>
#include <vector>

using granit::pipeline::detail::dynamic_uniform_arena;
using granit::pipeline::detail::dynamic_uniform_arena_plan;
using granit::pipeline::detail::dynamic_uniform_binding;
using granit::pipeline::detail::dynamic_uniform_request;
using granit::pipeline::detail::material_draw_state;
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

TEST_CASE("动态 Uniform Arena 隔离帧槽并按批次增长", "[pipeline][uniform-arena][gpu]") {
  granit::renderer renderer;
  REQUIRE(renderer.initialize({.application_name = "Granit Uniform Arena Test"}) ==
          granit::result::success);

  const std::array frame_entries{granit::bind_group_layout_entry{
      .binding = 0,
      .type = granit::binding_type::dynamic_uniform_buffer,
      .array_count = 1,
      .visibility = granit::shader_stage_flags::vertex | granit::shader_stage_flags::fragment}};
  const std::array object_entries{
      granit::bind_group_layout_entry{.binding = 0,
                                      .type = granit::binding_type::dynamic_uniform_buffer,
                                      .array_count = 1,
                                      .visibility = granit::shader_stage_flags::vertex}};
  granit::bind_group_layout frame_layout;
  granit::bind_group_layout object_layout;
  REQUIRE(frame_layout.initialize(renderer.native_handle(), frame_entries) ==
          granit::result::success);
  REQUIRE(object_layout.initialize(renderer.native_handle(), object_entries) ==
          granit::result::success);
  material_draw_state material;
  material.frame_layout = frame_layout.native_handle();
  material.object_layout = object_layout.native_handle();
  const granit::material::pbr_frame_constants frame{};
  const granit::material::pbr_object_constants object{};

  dynamic_uniform_arena arena;
  REQUIRE(arena.initialize(renderer.native_handle()) == GRANIT_SUCCESS);
  REQUIRE(arena.begin_frame(0, 2) == GRANIT_SUCCESS);
  std::vector<dynamic_uniform_request> requests(300);
  std::vector<dynamic_uniform_binding> bindings(requests.size());
  for (auto& request : requests) {
    request = {.material = &material,
               .frame = std::as_bytes(std::span{&frame, 1}),
               .object = std::as_bytes(std::span{&object, 1})};
  }
  REQUIRE(arena.prepare_batch(requests, bindings) == GRANIT_SUCCESS);
  CHECK(bindings.front().frame_offset == 0);
  CHECK(bindings.back().object_offset > 64 * 1024);

  granit::renderer_resource_stats first_stats;
  REQUIRE(renderer.get_resource_stats(first_stats) == granit::result::success);
  CHECK(first_stats.buffer_count == 1);
  CHECK(first_stats.bind_group_count == 2);

  REQUIRE(arena.begin_frame(1, 2) == GRANIT_SUCCESS);
  dynamic_uniform_binding second_slot;
  REQUIRE(arena.prepare(material, std::as_bytes(std::span{&frame, 1}),
                        std::as_bytes(std::span{&object, 1}), second_slot) == GRANIT_SUCCESS);
  CHECK(second_slot.frame_offset == 0);
  granit::renderer_resource_stats second_stats;
  REQUIRE(renderer.get_resource_stats(second_stats) == granit::result::success);
  CHECK(second_stats.buffer_count == 2);
  CHECK(second_stats.bind_group_count == 4);

  REQUIRE(arena.begin_frame(0, 2) == GRANIT_SUCCESS);
  dynamic_uniform_binding reused;
  REQUIRE(arena.prepare(material, std::as_bytes(std::span{&frame, 1}),
                        std::as_bytes(std::span{&object, 1}), reused) == GRANIT_SUCCESS);
  CHECK(reused.frame_offset == 0);
  CHECK(reused.frame_group == bindings.front().frame_group);

  REQUIRE(arena.begin_frame(0, 2) == GRANIT_SUCCESS);
  const dynamic_uniform_request invalid_request{.material = nullptr,
                                                .frame = std::as_bytes(std::span{&frame, 1}),
                                                .object = std::as_bytes(std::span{&object, 1})};
  dynamic_uniform_binding unchanged{
      .frame_group = 7, .object_group = 8, .frame_offset = 9, .object_offset = 10};
  CHECK(arena.prepare_batch(std::span{&invalid_request, 1}, std::span{&unchanged, 1}) ==
        GRANIT_ERROR_INVALID_ARGUMENT);
  CHECK(unchanged.frame_group == 7);
  CHECK(unchanged.object_group == 8);
  CHECK(unchanged.frame_offset == 9);
  CHECK(unchanged.object_offset == 10);
  REQUIRE(arena.prepare(material, std::as_bytes(std::span{&frame, 1}),
                        std::as_bytes(std::span{&object, 1}), reused) == GRANIT_SUCCESS);
  CHECK(reused.frame_offset == 0);

  REQUIRE(arena.reset() == GRANIT_SUCCESS);
  granit::renderer_resource_stats reset_stats;
  REQUIRE(renderer.get_resource_stats(reset_stats) == granit::result::success);
  CHECK(reset_stats.buffer_count == 0);
  CHECK(reset_stats.bind_group_count == 0);
}
