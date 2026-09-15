// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "core/handle_table.h"
#include "core/shared_handle_table.h"

#include <catch2/catch_all.hpp>

#include <memory>

namespace {

using granit::detail::handle_table;
using granit::detail::resource_type;

constexpr std::uint32_t first_domain = 7;
constexpr std::uint32_t second_domain = 11;

TEST_CASE("句柄表注册并查找资源", "[handle_table]") {
  handle_table table;
  int resource = 42;

  const auto handle = table.insert(&resource, resource_type::buffer, first_domain);

  REQUIRE(handle != GRANIT_NULL_HANDLE);
  CHECK(table.size() == 1);
  CHECK(table.find(handle, resource_type::buffer, first_domain) == &resource);
}

TEST_CASE("句柄表拒绝无效资源和未知类型", "[handle_table]") {
  handle_table table;
  int resource = 42;

  CHECK(table.insert(nullptr, resource_type::buffer, first_domain) == GRANIT_NULL_HANDLE);
  CHECK(table.insert(&resource, resource_type::unknown, first_domain) == GRANIT_NULL_HANDLE);
  CHECK(table.empty());
}

TEST_CASE("句柄表校验类型和所属 domain", "[handle_table]") {
  handle_table table;
  int resource = 42;
  const auto handle = table.insert(&resource, resource_type::texture, first_domain);

  CHECK(table.find(handle, resource_type::buffer, first_domain) == nullptr);
  CHECK(table.find(handle, resource_type::texture, second_domain) == nullptr);
  CHECK(table.erase(handle, resource_type::buffer, first_domain) == GRANIT_ERROR_INVALID_HANDLE);
  CHECK(table.erase(handle, resource_type::texture, second_domain) == GRANIT_ERROR_INVALID_HANDLE);
  CHECK(table.size() == 1);
}

TEST_CASE("资源销毁后旧句柄失效", "[handle_table]") {
  handle_table table;
  int first_resource = 1;
  int second_resource = 2;
  void* erased_resource = nullptr;

  const auto old_handle = table.insert(&first_resource, resource_type::buffer, first_domain);
  REQUIRE(table.erase(old_handle, resource_type::buffer, first_domain, &erased_resource) ==
          GRANIT_SUCCESS);
  CHECK(erased_resource == &first_resource);
  CHECK(table.empty());
  CHECK(table.find(old_handle, resource_type::buffer, first_domain) == nullptr);
  CHECK(table.erase(old_handle, resource_type::buffer, first_domain) ==
        GRANIT_ERROR_INVALID_HANDLE);

  const auto new_handle = table.insert(&second_resource, resource_type::buffer, first_domain);
  CHECK(new_handle != old_handle);
  CHECK(table.find(old_handle, resource_type::buffer, first_domain) == nullptr);
  CHECK(table.find(new_handle, resource_type::buffer, first_domain) == &second_resource);
}

TEST_CASE("不同资源类型不能因槽位复用而混淆", "[handle_table]") {
  handle_table table;
  int buffer = 1;
  int texture = 2;

  const auto buffer_handle = table.insert(&buffer, resource_type::buffer, first_domain);
  REQUIRE(table.erase(buffer_handle, resource_type::buffer, first_domain) == GRANIT_SUCCESS);

  const auto texture_handle = table.insert(&texture, resource_type::texture, first_domain);
  CHECK(texture_handle != buffer_handle);
  CHECK(table.find(buffer_handle, resource_type::buffer, first_domain) == nullptr);
  CHECK(table.find(texture_handle, resource_type::texture, first_domain) == &texture);
}

TEST_CASE("共享句柄表拒绝跨类型与重复销毁", "[handle_table]") {
  granit::detail::shared_handle_table<int, granit::detail::handle_type::asset_tools_material_result>
      materials;
  granit::detail::shared_handle_table<int, granit::detail::handle_type::asset_tools_texture_result>
      textures;

  const auto material = materials.insert(std::make_shared<const int>(7));
  const auto texture = textures.insert(std::make_shared<const int>(11));
  REQUIRE(material != GRANIT_NULL_HANDLE);
  REQUIRE(texture != GRANIT_NULL_HANDLE);
  CHECK(material != texture);
  CHECK(materials.find(texture) == nullptr);
  CHECK(textures.find(material) == nullptr);
  CHECK(materials.erase(texture) == GRANIT_ERROR_INVALID_HANDLE);
  CHECK(textures.erase(material) == GRANIT_ERROR_INVALID_HANDLE);

  const auto borrowed = materials.find(material);
  REQUIRE(borrowed != nullptr);
  CHECK(materials.erase(material) == GRANIT_SUCCESS);
  CHECK(*borrowed == 7);
  CHECK(materials.erase(material) == GRANIT_ERROR_INVALID_HANDLE);
  CHECK(materials.find(material) == nullptr);

  const auto replacement = materials.insert(std::make_shared<const int>(13));
  CHECK(replacement != material);
  CHECK(materials.find(material) == nullptr);
  REQUIRE(materials.find(replacement) != nullptr);
  CHECK(*materials.find(replacement) == 13);
}

} // namespace
