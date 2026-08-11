// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "material/material_metadata.h"

#include <catch2/catch_all.hpp>

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>

using granit::material::make_parameter_id;
using granit::material::material_instance_data;
using granit::material::material_metadata;
using granit::material::metadata_desc;
using granit::material::metadata_error;
using granit::material::parameter_desc;
using granit::material::parameter_type;

TEST_CASE("材质参数 ID 使用固定 FNV-1a 64") {
  CHECK(make_parameter_id("hello") == UINT64_C(0xa430d84680aabd0b));
  CHECK(make_parameter_id("base_color") == make_parameter_id("base_color"));
  CHECK(make_parameter_id("base_color") != make_parameter_id("roughness"));
}

TEST_CASE("材质元数据验证布局并补全参数 ID") {
  material_metadata metadata;
  metadata_desc desc;
  desc.constant_buffer_size = 32;
  desc.parameters = {
      {.name = "color", .type = parameter_type::float4, .offset = 0, .default_value = {}},
      {.name = "roughness", .type = parameter_type::float32, .offset = 16, .default_value = {}},
      {.name = "albedo", .type = parameter_type::texture_view, .binding = 1, .default_value = {}},
      {.name = "linear_sampler",
       .type = parameter_type::sampler,
       .binding = 2,
       .default_value = {}},
  };

  REQUIRE(material_metadata::build(std::move(desc), metadata) == metadata_error::none);
  CHECK(metadata.constant_buffer_size() == 32);
  REQUIRE(metadata.find(make_parameter_id("roughness")) != nullptr);
  CHECK(metadata.find(make_parameter_id("roughness"))->offset == 16);
}

TEST_CASE("材质元数据拒绝重复、重叠和越界参数") {
  SECTION("重复名称") {
    material_metadata metadata;
    metadata_desc desc{
        .constant_buffer_size = 16,
        .parameters = {
            {.name = "value", .type = parameter_type::float32, .default_value = {}},
            {.name = "value", .type = parameter_type::float32, .offset = 4, .default_value = {}}}};
    CHECK(material_metadata::build(std::move(desc), metadata) == metadata_error::duplicate_name);
  }
  SECTION("范围重叠") {
    material_metadata metadata;
    metadata_desc desc{
        .constant_buffer_size = 32,
        .parameters = {
            {.name = "first", .type = parameter_type::float4, .default_value = {}},
            {.name = "second", .type = parameter_type::float4, .offset = 8, .default_value = {}}}};
    CHECK(material_metadata::build(std::move(desc), metadata) ==
          metadata_error::overlapping_parameters);
  }
  SECTION("超出常量块") {
    material_metadata metadata;
    metadata_desc desc{
        .constant_buffer_size = 16,
        .parameters = {{.name = "matrix", .type = parameter_type::matrix4, .default_value = {}}}};
    CHECK(material_metadata::build(std::move(desc), metadata) == metadata_error::invalid_layout);
  }
}

TEST_CASE("材质实例应用默认值并合并 dirty 区间") {
  const auto default_color =
      std::bit_cast<std::array<std::byte, 16>>(std::array<float, 4>{1.0F, 0.5F, 0.25F, 1.0F});
  material_metadata metadata;
  metadata_desc desc;
  desc.constant_buffer_size = 32;
  desc.parameters = {
      {.name = "color",
       .type = parameter_type::float4,
       .offset = 0,
       .default_value = std::vector<std::byte>{default_color.begin(), default_color.end()}},
      {.name = "roughness", .type = parameter_type::float32, .offset = 16, .default_value = {}},
      {.name = "metallic", .type = parameter_type::float32, .offset = 24, .default_value = {}},
  };
  REQUIRE(material_metadata::build(std::move(desc), metadata) == metadata_error::none);

  material_instance_data instance(metadata);
  CHECK(instance.dirty().offset == 0);
  CHECK(instance.dirty().size == 32);
  CHECK(std::ranges::equal(instance.bytes().first(16), default_color));
  instance.clear_dirty();

  const auto roughness = std::bit_cast<std::array<std::byte, 4>>(0.75F);
  const auto metallic = std::bit_cast<std::array<std::byte, 4>>(1.0F);
  REQUIRE(instance.set(make_parameter_id("roughness"), parameter_type::float32, roughness) ==
          metadata_error::none);
  REQUIRE(instance.set(make_parameter_id("metallic"), parameter_type::float32, metallic) ==
          metadata_error::none);
  CHECK(instance.dirty().offset == 16);
  CHECK(instance.dirty().size == 12);

  instance.clear_dirty();
  CHECK(instance.set(make_parameter_id("metallic"), parameter_type::float32, metallic) ==
        metadata_error::none);
  CHECK(instance.dirty().empty());
}

TEST_CASE("材质实例拒绝错误参数、类型和大小") {
  material_metadata metadata;
  metadata_desc desc{
      .constant_buffer_size = 16,
      .parameters = {{.name = "value", .type = parameter_type::float4, .default_value = {}}}};
  REQUIRE(material_metadata::build(std::move(desc), metadata) == metadata_error::none);
  material_instance_data instance(metadata);
  const std::array<std::byte, 4> scalar{};
  CHECK(instance.set(make_parameter_id("missing"), parameter_type::float32, scalar) ==
        metadata_error::parameter_not_found);
  CHECK(instance.set(make_parameter_id("value"), parameter_type::float32, scalar) ==
        metadata_error::type_mismatch);
  CHECK(instance.set(make_parameter_id("value"), parameter_type::float4, scalar) ==
        metadata_error::size_mismatch);
}
