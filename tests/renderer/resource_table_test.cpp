// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "renderer/resource_table.hpp"

#include <catch2/catch_all.hpp>

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
