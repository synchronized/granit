// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/pipeline/canvas_draw_list.hpp>
#include <granit/renderer/renderer.hpp>

#include <catch2/catch_all.hpp>

#include <array>
#include <limits>

namespace {

bool environment_unavailable(granit::result result) {
  return result == granit::result::backend_unavailable || result == granit::result::not_ready;
}

constexpr std::array vertices{granit_canvas_vertex{0, 0, 0, 0, UINT32_C(0xffffffff)},
                              granit_canvas_vertex{1, 0, 1, 0, UINT32_C(0xffffffff)},
                              granit_canvas_vertex{0, 1, 0, 1, UINT32_C(0xffffffff)}};
constexpr std::array<uint32_t, 3> indices{0, 1, 2};

} // namespace

TEST_CASE("公共Canvas Draw List支持通用几何、矩形、合批与复用") {
  granit::renderer renderer;
  const auto initialized = renderer.initialize({.application_name = "granit-ui-list-api"});
  if (environment_unavailable(initialized))
    SKIP("当前运行环境没有满足要求的 Vulkan 设备");
  REQUIRE(initialized == granit::result::success);

  granit_canvas_draw_list_desc desc = GRANIT_CANVAS_DRAW_LIST_DESC_INIT;
  desc.initial_vertex_capacity = 16;
  desc.initial_index_capacity = 24;
  desc.initial_item_capacity = 4;
  granit::canvas_draw_list list;
  REQUIRE(list.initialize(renderer.native_handle(), desc) == granit::result::success);

  const granit_canvas_draw_state first{11, 21, {0, 0, 128, 128}};
  const granit_canvas_draw_state second{12, 21, {0, 0, 128, 128}};
  REQUIRE(list.append(vertices, indices, first) == granit::result::success);
  REQUIRE(list.append(vertices, indices, first) == granit::result::success);

  granit_canvas_rect_desc rect = GRANIT_CANVAS_RECT_DESC_INIT;
  rect.x = 10;
  rect.y = 20;
  rect.width = 30;
  rect.height = 40;
  rect.state = second;
  REQUIRE(list.append_rect(rect) == granit::result::success);

  granit_canvas_draw_list_stats stats = GRANIT_CANVAS_DRAW_LIST_STATS_INIT;
  REQUIRE(list.get_stats(stats) == granit::result::success);
  CHECK(stats.vertex_count == 10);
  CHECK(stats.index_count == 12);
  CHECK(stats.item_count == 3);
  CHECK(stats.batch_count == 2);

  REQUIRE(list.clear() == granit::result::success);
  stats = GRANIT_CANVAS_DRAW_LIST_STATS_INIT;
  REQUIRE(list.get_stats(stats) == granit::result::success);
  CHECK(stats.vertex_count == 0);
  CHECK(stats.index_count == 0);
  CHECK(stats.item_count == 0);
  CHECK(stats.batch_count == 0);
}

TEST_CASE("公共Canvas Draw List拒绝无效输入、跨Renderer与旧句柄") {
  granit::renderer first;
  granit::renderer second;
  const auto first_result = first.initialize({.application_name = "granit-ui-list-first"});
  const auto second_result = second.initialize({.application_name = "granit-ui-list-second"});
  if (environment_unavailable(first_result) || environment_unavailable(second_result))
    SKIP("当前运行环境没有满足要求的 Vulkan 设备");
  REQUIRE(first_result == granit::result::success);
  REQUIRE(second_result == granit::result::success);

  granit_canvas_draw_list list = GRANIT_NULL_HANDLE;
  granit_canvas_draw_list_desc desc = GRANIT_CANVAS_DRAW_LIST_DESC_INIT;
  REQUIRE(granit_canvas_draw_list_create(first.native_handle(), &desc, &list) == GRANIT_SUCCESS);
  const granit_canvas_draw_state state{11, 21, {}};
  CHECK(granit_canvas_draw_list_append(second.native_handle(), list, vertices.data(), 3,
                                       indices.data(), 3, &state) == GRANIT_ERROR_INVALID_HANDLE);
  CHECK(granit_canvas_draw_list_append(first.native_handle(), list, nullptr, 3, indices.data(), 3,
                                       &state) == GRANIT_ERROR_INVALID_ARGUMENT);
  const std::array<uint32_t, 2> non_triangles{0, 1};
  CHECK(granit_canvas_draw_list_append(first.native_handle(), list, vertices.data(), 3,
                                       non_triangles.data(), 2,
                                       &state) == GRANIT_ERROR_INVALID_ARGUMENT);
  auto invalid_vertices = vertices;
  invalid_vertices[0].x = std::numeric_limits<float>::infinity();
  CHECK(granit_canvas_draw_list_append(first.native_handle(), list, invalid_vertices.data(), 3,
                                       indices.data(), 3, &state) == GRANIT_ERROR_INVALID_ARGUMENT);
  granit_canvas_rect_desc rect = GRANIT_CANVAS_RECT_DESC_INIT;
  rect.state = state;
  CHECK(granit_canvas_draw_list_append_rect(first.native_handle(), list, &rect) ==
        GRANIT_ERROR_INVALID_ARGUMENT);

  REQUIRE(granit_canvas_draw_list_destroy(first.native_handle(), list) == GRANIT_SUCCESS);
  CHECK(granit_canvas_draw_list_destroy(first.native_handle(), list) ==
        GRANIT_ERROR_INVALID_HANDLE);
  granit_canvas_draw_list_stats stats = GRANIT_CANVAS_DRAW_LIST_STATS_INIT;
  CHECK(granit_canvas_draw_list_get_stats(first.native_handle(), list, &stats) ==
        GRANIT_ERROR_INVALID_HANDLE);
}
