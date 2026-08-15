// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "pipeline/ui_draw_list.h"

#include <catch2/catch_all.hpp>

#include <array>

namespace {

constexpr std::array vertices{granit::pipeline::detail::ui_vertex{0, 0, 0, 0, UINT32_MAX},
                              granit::pipeline::detail::ui_vertex{1, 0, 1, 0, UINT32_MAX},
                              granit::pipeline::detail::ui_vertex{0, 1, 0, 1, UINT32_MAX}};
constexpr std::array<std::uint32_t, 3> indices{0, 1, 2};

} // namespace

TEST_CASE("UI Draw List保持顺序并只合并相邻兼容项") {
  using namespace granit::pipeline::detail;
  ui_draw_list list;
  const ui_draw_state first{.texture = 11, .sampler = 21, .scissor = {0, 0, 100, 100}, .layer = 2};
  const ui_draw_state second{.texture = 12, .sampler = 21, .scissor = {0, 0, 100, 100}, .layer = 2};

  REQUIRE(list.append(vertices, indices, first) == GRANIT_SUCCESS);
  REQUIRE(list.append(vertices, indices, first) == GRANIT_SUCCESS);
  REQUIRE(list.append(vertices, indices, second) == GRANIT_SUCCESS);
  REQUIRE(list.append(vertices, indices, first) == GRANIT_SUCCESS);

  REQUIRE(list.vertices().size() == 12);
  REQUIRE(list.indices().size() == 12);
  CHECK(list.indices()[3] == 3);
  CHECK(list.indices()[6] == 6);
  const auto batches = list.batches();
  REQUIRE(batches.size() == 3);
  CHECK(batches[0].first_index == 0);
  CHECK(batches[0].index_count == 6);
  CHECK(batches[1].first_index == 6);
  CHECK(batches[1].state.texture == 12);
  CHECK(batches[2].first_index == 9);
  CHECK(batches[2].state.texture == 11);
}

TEST_CASE("UI Draw List拒绝无效几何且可以复用") {
  using namespace granit::pipeline::detail;
  ui_draw_list list;
  constexpr std::array<std::uint32_t, 3> invalid_indices{0, 1, 3};
  CHECK(list.append({}, indices, {}) == GRANIT_ERROR_INVALID_ARGUMENT);
  CHECK(list.append(vertices, {}, {}) == GRANIT_ERROR_INVALID_ARGUMENT);
  CHECK(list.append(vertices, invalid_indices, {}) == GRANIT_ERROR_INVALID_ARGUMENT);
  CHECK(list.vertices().empty());
  CHECK(list.indices().empty());
  CHECK(list.items().empty());

  REQUIRE(list.append(vertices, indices, {}) == GRANIT_SUCCESS);
  list.clear();
  CHECK(list.vertices().empty());
  CHECK(list.indices().empty());
  CHECK(list.items().empty());
  CHECK(list.batches().empty());
}
