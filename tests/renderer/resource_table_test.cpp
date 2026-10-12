// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "renderer/resource_table.hpp"

#include <array>
#include <catch2/catch_all.hpp>
#include <granit/renderer/renderer.hpp>
#include <granit/renderer/pipeline.hpp>
#include <granit/renderer/resource_table.hpp>
#include <granit/renderer/sampler.hpp>

namespace {
using granit::internal::resource_table;
using granit::internal::resource_table_entry;
using granit::internal::resource_table_result;
using granit::internal::resource_table_type;
} // namespace

TEST_CASE("Resource Table 校验类型和 Renderer 归属", "[resource_table][lifecycle]") {
  resource_table table{2};
  const auto texture = table.allocate(11, resource_table_type::texture_view, 101);
  REQUIRE(texture.has_value());

  resource_table_entry entry;
  CHECK(table.resolve(*texture, 11, resource_table_type::texture_view, entry) ==
        resource_table_result::success);
  CHECK(entry.resource == 101);
  CHECK(table.resolve(*texture, 12, resource_table_type::texture_view, entry) ==
        resource_table_result::owner_mismatch);
  CHECK(table.resolve(*texture, 11, resource_table_type::sampler, entry) ==
        resource_table_result::type_mismatch);
}

TEST_CASE("Resource Table 延迟回收并拒绝旧 generation", "[resource_table][lifecycle]") {
  resource_table table{1};
  const auto old_handle = table.allocate(11, resource_table_type::texture_view, 101);
  REQUIRE(old_handle.has_value());
  REQUIRE(table.release(*old_handle, 5) == resource_table_result::success);

  CHECK(!table.allocate(11, resource_table_type::texture_view, 202).has_value());
  table.collect(4);
  CHECK(!table.allocate(11, resource_table_type::texture_view, 202).has_value());
  table.collect(5);

  const auto new_handle = table.allocate(11, resource_table_type::texture_view, 202);
  REQUIRE(new_handle.has_value());
  CHECK(*new_handle != *old_handle);
  resource_table_entry entry;
  CHECK(table.resolve(*old_handle, 11, resource_table_type::texture_view, entry) ==
        resource_table_result::invalid_handle);
  CHECK(table.release(*old_handle, 5) == resource_table_result::invalid_handle);
}

TEST_CASE("Resource Table 拒绝容量溢出和重复释放", "[resource_table][lifecycle]") {
  resource_table table{1};
  const auto handle = table.allocate(11, resource_table_type::sampler, 303);
  REQUIRE(handle.has_value());
  CHECK(!table.allocate(11, resource_table_type::sampler, 304).has_value());
  CHECK(table.release(*handle, 1) == resource_table_result::success);
  CHECK(table.release(*handle, 1) == resource_table_result::invalid_handle);
}

TEST_CASE("公共 Resource Table 校验能力、槽位和 Renderer 归属",
          "[resource_table][public][validation]") {
  granit::renderer first;
  const auto first_result = first.initialize({.application_name = "granit-resource-table-first"});
  if (first_result == granit::result::backend_unavailable ||
      first_result == granit::result::incompatible_driver ||
      first_result == granit::result::no_suitable_device)
    SKIP("当前运行环境没有满足要求的 Vulkan 设备");
  REQUIRE(first_result == granit::result::success);

  granit_resource_table_capabilities capabilities = GRANIT_RESOURCE_TABLE_CAPABILITIES_INIT;
  REQUIRE(granit_renderer_get_resource_table_capabilities(first.native_handle(), &capabilities) ==
          GRANIT_SUCCESS);
  CHECK(capabilities.resource_types != 0);
  CHECK(capabilities.max_samplers > 0);

  granit::sampler sampler;
  REQUIRE(sampler.initialize(first) == granit::result::success);
  granit::resource_table table;
  REQUIRE(table.initialize(first, {.resource_types = GRANIT_RESOURCE_TABLE_RESOURCE_SAMPLER_BIT,
                                   .sampler_capacity = 1}) == granit::result::success);

  CHECK(table.update({.type = granit::resource_table_resource_type::sampler,
                     .slot = 0,
                     .resource = sampler.native_handle()}) == granit::result::success);
  CHECK(table.update({.type = granit::resource_table_resource_type::sampler,
                     .slot = 1,
                     .resource = sampler.native_handle()}) == granit::result::invalid_argument);
  CHECK(table.update({.type = granit::resource_table_resource_type::sampled_texture_view,
                     .slot = 0,
                     .resource = sampler.native_handle()}) == granit::result::invalid_argument);

  granit::renderer second;
  REQUIRE(second.initialize({.application_name = "granit-resource-table-second"}) ==
          granit::result::success);
  granit::sampler foreign_sampler;
  REQUIRE(foreign_sampler.initialize(second) == granit::result::success);
  CHECK(table.update({.type = granit::resource_table_resource_type::sampler,
                     .slot = 0,
                     .resource = foreign_sampler.native_handle()}) ==
        granit::result::invalid_handle);
}

TEST_CASE("公共 Resource Table 批量更新失败时不部分写入",
          "[resource_table][public][validation]") {
  granit::renderer renderer;
  const auto result = renderer.initialize({.application_name = "granit-resource-table-batch"});
  if (result == granit::result::backend_unavailable ||
      result == granit::result::incompatible_driver ||
      result == granit::result::no_suitable_device)
    SKIP("当前运行环境没有满足要求的 Vulkan 设备");
  REQUIRE(result == granit::result::success);

  granit::sampler sampler;
  REQUIRE(sampler.initialize(renderer) == granit::result::success);
  granit::resource_table table;
  REQUIRE(table.initialize(renderer, {.resource_types = GRANIT_RESOURCE_TABLE_RESOURCE_SAMPLER_BIT,
                                      .sampler_capacity = 1}) == granit::result::success);
  const std::array updates{
      granit::resource_table_update{.type = granit::resource_table_resource_type::sampler,
                                    .slot = 0,
                                    .resource = sampler.native_handle()},
      granit::resource_table_update{.type = granit::resource_table_resource_type::sampler,
                                    .slot = 2,
                                    .resource = sampler.native_handle()}};
  CHECK(table.update_batch(updates) == granit::result::invalid_argument);
}

TEST_CASE("Pipeline Layout 可以声明 Resource Table", "[resource_table][pipeline]") {
  granit::renderer renderer;
  const auto result = renderer.initialize({.application_name = "granit-resource-table-layout"});
  if (result == granit::result::backend_unavailable ||
      result == granit::result::incompatible_driver ||
      result == granit::result::no_suitable_device)
    SKIP("当前运行环境没有满足要求的 Vulkan 设备");
  REQUIRE(result == granit::result::success);

  granit::resource_table table;
  REQUIRE(table.initialize(renderer, {.resource_types = GRANIT_RESOURCE_TABLE_RESOURCE_SAMPLER_BIT,
                                      .sampler_capacity = 1}) == granit::result::success);
  granit::pipeline_layout layout;
  const auto layout_result = layout.initialize(renderer.ref(), {}, table.ref());
  if (layout_result == granit::result::unsupported)
    SKIP("当前后端不支持将 Resource Table 接入 Pipeline Layout");
  REQUIRE(layout_result == granit::result::success);
  REQUIRE(layout.reset() == granit::result::success);
  REQUIRE(table.reset() == granit::result::success);
}
