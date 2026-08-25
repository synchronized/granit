// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "pipeline/canvas_geometry_upload.h"

#include <granit/granit.hpp>

#include <catch2/catch_all.hpp>

#include <array>
#include <cstring>

TEST_CASE("UI几何上传复用容量并保留顶点索引内容") {
  using namespace granit::pipeline::detail;
  granit::renderer renderer;
  const auto initialized = renderer.initialize({.application_name = "granit-ui-upload"});
  if (initialized == granit::result::backend_unavailable ||
      initialized == granit::result::incompatible_driver ||
      initialized == granit::result::no_suitable_device) {
    SKIP("当前运行环境没有满足要求的 Vulkan 设备");
  }
  REQUIRE(initialized == granit::result::success);

  constexpr std::array vertices{canvas_vertex{0, 0, 0, 0, 0x01020304},
                                canvas_vertex{1, 0, 1, 0, 0x11121314},
                                canvas_vertex{0, 1, 0, 1, 0x21222324}};
  constexpr std::array<std::uint32_t, 3> indices{0, 1, 2};
  canvas_draw_list list;
  REQUIRE(list.append(vertices, indices, {}) == GRANIT_SUCCESS);

  canvas_geometry_upload upload;
  REQUIRE(upload.upload(renderer.native_handle(), list) == GRANIT_SUCCESS);
  REQUIRE(upload.vertex_buffer() != GRANIT_NULL_HANDLE);
  REQUIRE(upload.index_buffer() != GRANIT_NULL_HANDLE);
  CHECK(upload.vertex_count() == vertices.size());
  CHECK(upload.index_count() == indices.size());
  CHECK(upload.vertex_capacity() >= sizeof(vertices));
  CHECK(upload.index_capacity() >= sizeof(indices));

  CHECK(std::memcmp(upload.vertex_data(), vertices.data(), sizeof(vertices)) == 0);
  CHECK(std::memcmp(upload.index_data(), indices.data(), sizeof(indices)) == 0);

  std::array<granit_buffer, GRANIT_DEFAULT_FRAMES_IN_FLIGHT> vertex_buffers{upload.vertex_buffer()};
  std::array<granit_buffer, GRANIT_DEFAULT_FRAMES_IN_FLIGHT> index_buffers{upload.index_buffer()};
  for (std::size_t slot = 1; slot < vertex_buffers.size(); ++slot) {
    REQUIRE(upload.upload(renderer.native_handle(), list) == GRANIT_SUCCESS);
    vertex_buffers[slot] = upload.vertex_buffer();
    index_buffers[slot] = upload.index_buffer();
    CHECK(vertex_buffers[slot] != vertex_buffers[slot - 1]);
    CHECK(index_buffers[slot] != index_buffers[slot - 1]);
  }
  REQUIRE(upload.upload(renderer.native_handle(), list) == GRANIT_SUCCESS);
  CHECK(upload.vertex_buffer() == vertex_buffers.front());
  CHECK(upload.index_buffer() == index_buffers.front());

  REQUIRE(upload.upload(renderer.native_handle(), list, 1) == GRANIT_SUCCESS);
  CHECK(upload.vertex_buffer() == vertex_buffers[1]);
  CHECK(upload.index_buffer() == index_buffers[1]);
  CHECK(upload.upload(renderer.native_handle(), list, GRANIT_DEFAULT_FRAMES_IN_FLIGHT) ==
        GRANIT_ERROR_INVALID_ARGUMENT);

  canvas_draw_list empty;
  REQUIRE(upload.upload(renderer.native_handle(), empty) == GRANIT_SUCCESS);
  CHECK(upload.vertex_count() == 0);
  CHECK(upload.index_count() == 0);
  CHECK(upload.vertex_buffer() == vertex_buffers[1]);
  CHECK(upload.index_buffer() == index_buffers[1]);
}

TEST_CASE("UI几何上传支持一到四个动态帧槽") {
  using namespace granit::pipeline::detail;
  granit::renderer renderer;
  const auto initialized = renderer.initialize({.application_name = "granit-ui-upload-slots"});
  if (initialized == granit::result::backend_unavailable ||
      initialized == granit::result::incompatible_driver ||
      initialized == granit::result::no_suitable_device) {
    SKIP("当前运行环境没有满足要求的 Vulkan 设备");
  }
  REQUIRE(initialized == granit::result::success);

  constexpr std::array vertices{canvas_vertex{0, 0, 0, 0, 0xffffffff},
                                canvas_vertex{1, 0, 1, 0, 0xffffffff},
                                canvas_vertex{0, 1, 0, 1, 0xffffffff}};
  constexpr std::array<std::uint32_t, 3> indices{0, 1, 2};
  canvas_draw_list list;
  REQUIRE(list.append(vertices, indices, {}) == GRANIT_SUCCESS);

  for (std::uint32_t slot_count = 1; slot_count <= GRANIT_MAX_FRAMES_IN_FLIGHT; ++slot_count) {
    canvas_geometry_upload upload{slot_count};
    CHECK(upload.frame_slot_count() == slot_count);
    for (std::uint32_t slot = 0; slot < slot_count; ++slot)
      REQUIRE(upload.upload(renderer.native_handle(), list, slot) == GRANIT_SUCCESS);
    CHECK(upload.upload(renderer.native_handle(), list, slot_count) ==
          GRANIT_ERROR_INVALID_ARGUMENT);
  }
}
