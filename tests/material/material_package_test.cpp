// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "material/material_package.h"

#include <catch2/catch_all.hpp>

#include <array>

namespace {

using namespace granit::material;

material_shader_code shader(package_shader_stage stage) {
  return {.stage = stage,
          .entry_point = "main",
          .spirv = {UINT32_C(0x07230203), UINT32_C(0x00010600), 0, 1, 0}};
}

material_variant_desc variant(std::initializer_list<material_feature_value> features) {
  return {
      .pass = make_feature_id("opaque"),
      .features = features,
      .shaders = {shader(package_shader_stage::vertex), shader(package_shader_stage::fragment)}};
}

} // namespace

TEST_CASE("材质包规范化变体并按稳定键查找") {
  const auto normal_map = make_feature_id("normal_map");
  const auto alpha_mode = make_feature_id("alpha_mode");
  material_package_desc desc;
  desc.metadata.constant_buffer_size = 16;
  desc.variants.push_back(variant({{normal_map, 1}, {alpha_mode, 2}}));

  material_package package;
  REQUIRE(material_package::build(std::move(desc), package) == package_error::none);
  REQUIRE(package.format_version() == material_package_format_version);
  REQUIRE(package.binding_model() == package_binding_model::bind_group);

  const std::array<material_feature_value, 2> canonical{
      {{alpha_mode, UINT32_C(2)}, {normal_map, UINT32_C(1)}}};
  const auto* found = package.find(make_feature_id("opaque"), make_variant_key(canonical));
  REQUIRE(found != nullptr);
  CHECK(std::ranges::equal(found->features, canonical));
  CHECK(package.find(make_feature_id("shadow"), found->key) == nullptr);
}

TEST_CASE("材质包拒绝不兼容版本与未实现绑定模型") {
  material_package package;
  material_package_desc desc;
  desc.format_version = material_package_format_version + 1;
  CHECK(material_package::build(std::move(desc), package) == package_error::unsupported_version);

  desc = {};
  desc.binding_model = package_binding_model::bindless;
  CHECK(material_package::build(std::move(desc), package) ==
        package_error::unsupported_binding_model);

  desc = {};
  desc.required_renderer_features = package_feature_bindless_resource_table;
  CHECK(material_package::build(std::move(desc), package) ==
        package_error::unsupported_renderer_features);
}

TEST_CASE("材质包拒绝重复特性、重复变体和无效 Shader") {
  const auto feature = make_feature_id("normal_map");
  material_package package;
  material_package_desc desc;
  desc.variants.push_back(variant({{feature, 1}, {feature, 0}}));
  CHECK(material_package::build(std::move(desc), package) == package_error::duplicate_feature);

  desc = {};
  desc.variants.push_back(variant({{feature, 1}}));
  desc.variants.push_back(variant({{feature, 1}}));
  CHECK(material_package::build(std::move(desc), package) == package_error::duplicate_variant);

  desc = {};
  auto invalid = variant({});
  invalid.shaders.front().spirv.clear();
  desc.variants.push_back(std::move(invalid));
  CHECK(material_package::build(std::move(desc), package) == package_error::invalid_shader);
}

TEST_CASE("材质包拒绝没有任何变体") {
  material_package package;
  material_package_desc desc;
  CHECK(material_package::build(std::move(desc), package) == package_error::missing_variant);
}
