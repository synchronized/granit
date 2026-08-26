// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/pipeline/scene.hpp>
#include <granit/renderer/renderer.hpp>

#include <catch2/catch_all.hpp>

#include <array>

namespace {

granit_matrix4 identity() { return {{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}}; }

granit_scene_snapshot_desc valid_desc(std::array<granit_scene_view, 1>& views) {
  views[0].view = identity();
  views[0].projection = identity();
  views[0].view_projection = identity();
  views[0].viewport_width = 32;
  views[0].viewport_height = 32;
  views[0].layer_mask = UINT64_MAX;
  granit_scene_snapshot_desc desc = GRANIT_SCENE_SNAPSHOT_DESC_INIT;
  desc.views = views.data();
  desc.view_count = static_cast<uint32_t>(views.size());
  return desc;
}

bool environment_unavailable(granit::result value) {
  return value == granit::result::backend_unavailable ||
         value == granit::result::incompatible_driver ||
         value == granit::result::no_suitable_device;
}

} // namespace

TEST_CASE("公共Scene Snapshot把空Renderer归类为无效句柄") {
  std::array<granit_scene_view, 1> views{};
  const auto desc = valid_desc(views);
  granit_scene_snapshot snapshot = UINT64_C(1);
  CHECK(granit_scene_snapshot_create(GRANIT_NULL_HANDLE, &desc, &snapshot) ==
        GRANIT_ERROR_INVALID_HANDLE);
  CHECK(snapshot == GRANIT_NULL_HANDLE);

  granit::scene_snapshot cpp_snapshot;
  CHECK(cpp_snapshot.initialize(GRANIT_NULL_HANDLE, desc) == granit::result::invalid_handle);
  CHECK_FALSE(cpp_snapshot.valid());
}

TEST_CASE("公共Scene Snapshot复制输入并使旧句柄失效") {
  granit::renderer renderer;
  const auto initialized = renderer.initialize({.application_name = "granit-scene-abi"});
  if (environment_unavailable(initialized))
    SKIP("当前运行环境没有满足要求的 Vulkan 设备");
  REQUIRE(initialized == granit::result::success);

  std::array<granit_scene_view, 1> views{};
  auto desc = valid_desc(views);
  granit_scene_snapshot first = GRANIT_NULL_HANDLE;
  REQUIRE(granit_scene_snapshot_create(renderer.native_handle(), &desc, &first) == GRANIT_SUCCESS);
  REQUIRE(first != GRANIT_NULL_HANDLE);
  views[0].layer_mask = 0;
  REQUIRE(granit_scene_snapshot_destroy(renderer.native_handle(), first) == GRANIT_SUCCESS);
  CHECK(granit_scene_snapshot_destroy(renderer.native_handle(), first) ==
        GRANIT_ERROR_INVALID_HANDLE);

  views[0].layer_mask = UINT64_MAX;
  granit_scene_snapshot second = GRANIT_NULL_HANDLE;
  REQUIRE(granit_scene_snapshot_create(renderer.native_handle(), &desc, &second) == GRANIT_SUCCESS);
  CHECK(second != first);
  REQUIRE(granit_scene_snapshot_destroy(renderer.native_handle(), second) == GRANIT_SUCCESS);
}

TEST_CASE("公共Scene Snapshot拒绝非法输入与跨Renderer销毁") {
  granit::renderer first_renderer;
  granit::renderer second_renderer;
  const auto first_result = first_renderer.initialize({.application_name = "granit-scene-first"});
  const auto second_result =
      second_renderer.initialize({.application_name = "granit-scene-second"});
  if (environment_unavailable(first_result) || environment_unavailable(second_result))
    SKIP("当前运行环境没有满足要求的 Vulkan 设备");
  REQUIRE(first_result == granit::result::success);
  REQUIRE(second_result == granit::result::success);

  std::array<granit_scene_view, 1> views{};
  auto desc = valid_desc(views);
  granit_scene_snapshot snapshot = GRANIT_NULL_HANDLE;
  REQUIRE(granit_scene_snapshot_create(first_renderer.native_handle(), &desc, &snapshot) ==
          GRANIT_SUCCESS);
  CHECK(granit_scene_snapshot_destroy(second_renderer.native_handle(), snapshot) ==
        GRANIT_ERROR_INVALID_HANDLE);
  REQUIRE(granit_scene_snapshot_destroy(first_renderer.native_handle(), snapshot) ==
          GRANIT_SUCCESS);

  desc.views = nullptr;
  CHECK(granit_scene_snapshot_create(first_renderer.native_handle(), &desc, &snapshot) ==
        GRANIT_ERROR_INVALID_ARGUMENT);
  CHECK(snapshot == GRANIT_NULL_HANDLE);
}

TEST_CASE("公共Scene Snapshot Cpp包装提供移动所有权") {
  granit::renderer renderer;
  const auto initialized = renderer.initialize({.application_name = "granit-scene-cpp"});
  if (environment_unavailable(initialized))
    SKIP("当前运行环境没有满足要求的 Vulkan 设备");
  REQUIRE(initialized == granit::result::success);
  std::array<granit_scene_view, 1> views{};
  const auto desc = valid_desc(views);
  granit::scene_snapshot first;
  REQUIRE(first.initialize(renderer.native_handle(), desc) == granit::result::success);
  const auto handle = first.native_handle();
  granit::scene_snapshot second{std::move(first)};
  CHECK_FALSE(first.valid());
  CHECK(second.native_handle() == handle);
}
