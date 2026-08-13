// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/pipeline/mesh.hpp>
#include <granit/renderer/buffer.hpp>
#include <granit/renderer/renderer.hpp>

#include <catch2/catch_all.hpp>

#include <array>

namespace {

bool environment_unavailable(granit::result value) {
  return value == granit::result::backend_unavailable ||
         value == granit::result::incompatible_driver ||
         value == granit::result::no_suitable_device;
}

} // namespace

TEST_CASE("公共Mesh复制一次Draw描述并校验Buffer用途") {
  granit::renderer renderer;
  const auto initialized = renderer.initialize({.application_name = "granit-public-mesh"});
  if (environment_unavailable(initialized))
    SKIP("当前运行环境没有满足要求的 Vulkan 设备");
  REQUIRE(initialized == granit::result::success);

  granit::buffer vertices;
  REQUIRE(vertices.initialize(renderer.native_handle(),
                              {.size = 3 * 12,
                               .usage = granit::buffer_usage::vertex,
                               .location = granit::memory_location::upload}) ==
          granit::result::success);
  const granit_vertex_attribute position{0, GRANIT_VERTEX_FORMAT_FLOAT32X3, 0, 0};
  const granit_mesh_vertex_buffer vertex{
      vertices.native_handle(), 0, {12, GRANIT_VERTEX_STEP_MODE_VERTEX, 1, 0, &position}};
  granit_mesh_desc desc = GRANIT_MESH_DESC_INIT;
  desc.vertex_buffers = &vertex;
  desc.vertex_buffer_count = 1;
  desc.vertex_count = 3;
  granit::mesh mesh;
  REQUIRE(mesh.initialize(renderer.native_handle(), desc) == granit::result::success);

  const auto old = mesh.native_handle();
  REQUIRE(mesh.reset() == granit::result::success);
  CHECK(granit_mesh_destroy(renderer.native_handle(), old) == GRANIT_ERROR_INVALID_HANDLE);

  granit::buffer wrong_usage;
  REQUIRE(wrong_usage.initialize(renderer.native_handle(),
                                 {.size = 36,
                                  .usage = granit::buffer_usage::uniform,
                                  .location = granit::memory_location::upload}) ==
          granit::result::success);
  auto invalid_vertex = vertex;
  invalid_vertex.buffer = wrong_usage.native_handle();
  desc.vertex_buffers = &invalid_vertex;
  CHECK(mesh.initialize(renderer.native_handle(), desc) == granit::result::invalid_argument);
}

TEST_CASE("公共Mesh拒绝跨Renderer Buffer与重复Attribute位置") {
  granit::renderer first;
  granit::renderer second;
  const auto first_result = first.initialize({.application_name = "granit-mesh-first"});
  const auto second_result = second.initialize({.application_name = "granit-mesh-second"});
  if (environment_unavailable(first_result) || environment_unavailable(second_result))
    SKIP("当前运行环境没有满足要求的 Vulkan 设备");
  REQUIRE(first_result == granit::result::success);
  REQUIRE(second_result == granit::result::success);

  granit::buffer vertices;
  REQUIRE(
      vertices.initialize(first.native_handle(), {.size = 64,
                                                  .usage = granit::buffer_usage::vertex,
                                                  .location = granit::memory_location::upload}) ==
      granit::result::success);
  const std::array attributes{granit_vertex_attribute{0, GRANIT_VERTEX_FORMAT_FLOAT32X2, 0, 0},
                              granit_vertex_attribute{0, GRANIT_VERTEX_FORMAT_FLOAT32X2, 8, 0}};
  granit_mesh_vertex_buffer vertex{
      vertices.native_handle(), 0, {16, GRANIT_VERTEX_STEP_MODE_VERTEX, 1, 0, attributes.data()}};
  granit_mesh_desc desc = GRANIT_MESH_DESC_INIT;
  desc.vertex_buffers = &vertex;
  desc.vertex_buffer_count = 1;
  desc.vertex_count = 3;
  granit_mesh mesh = GRANIT_NULL_HANDLE;
  CHECK(granit_mesh_create(second.native_handle(), &desc, &mesh) == GRANIT_ERROR_INVALID_HANDLE);
  vertex.layout.attribute_count = 2;
  CHECK(granit_mesh_create(first.native_handle(), &desc, &mesh) == GRANIT_ERROR_INVALID_ARGUMENT);
  CHECK(mesh == GRANIT_NULL_HANDLE);
}
