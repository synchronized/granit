// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "material/material_migration.h"

#include <catch2/catch_all.hpp>

#include <algorithm>
#include <array>
#include <bit>
#include <memory>

namespace {

granit::material::material_metadata make_source_metadata() {
  granit::material::metadata_desc desc;
  desc.constant_buffer_size = 32;
  desc.parameters = {
      {.name = "color",
       .type = granit::material::parameter_type::float4,
       .offset = 0,
       .default_value = {}},
      {.name = "mode",
       .type = granit::material::parameter_type::uint32,
       .offset = 16,
       .default_value = {}},
      {.name = "weights",
       .type = granit::material::parameter_type::float32,
       .offset = 20,
       .array_count = 2,
       .array_stride = 8,
       .default_value = {}},
  };
  granit::material::material_metadata metadata;
  REQUIRE(granit::material::material_metadata::build(std::move(desc), metadata) ==
          granit::material::metadata_error::none);
  return metadata;
}

granit::material::material_metadata make_target_metadata() {
  granit::material::metadata_desc desc;
  desc.constant_buffer_size = 48;
  desc.parameters = {
      {.name = "color",
       .type = granit::material::parameter_type::float4,
       .offset = 16,
       .default_value = {}},
      {.name = "mode",
       .type = granit::material::parameter_type::float32,
       .offset = 32,
       .default_value = std::vector<std::byte>(4, std::byte{0x2a})},
      {.name = "new_value",
       .type = granit::material::parameter_type::uint32,
       .offset = 36,
       .default_value = std::vector<std::byte>(4, std::byte{0x11})},
      {.name = "weights",
       .type = granit::material::parameter_type::float32,
       .offset = 0,
       .array_count = 2,
       .array_stride = 4,
       .default_value = {}},
      {.name = "albedo",
       .type = granit::material::parameter_type::texture_view,
       .binding = 1,
       .default_value = {}},
  };
  granit::material::material_metadata metadata;
  REQUIRE(granit::material::material_metadata::build(std::move(desc), metadata) ==
          granit::material::metadata_error::none);
  return metadata;
}

} // namespace

TEST_CASE("材质实例迁移匹配参数并保留新模板默认值") {
  const auto source_metadata = make_source_metadata();
  granit::material::material_instance_data source{source_metadata};
  const auto color =
      std::bit_cast<std::array<std::byte, 16>>(std::array<float, 4>{1.0F, 0.5F, 0.25F, 1.0F});
  const auto weights =
      std::bit_cast<std::array<std::byte, 12>>(std::array<std::uint32_t, 3>{1, 0, 2});
  REQUIRE(source.set(granit::material::make_parameter_id("color"),
                     granit::material::parameter_type::float4,
                     color) == granit::material::metadata_error::none);
  REQUIRE(source.set(granit::material::make_parameter_id("weights"),
                     granit::material::parameter_type::float32,
                     weights) == granit::material::metadata_error::none);

  const auto target_metadata = make_target_metadata();
  std::unique_ptr<granit::material::material_instance_data> migrated;
  granit::material::migration_report report;
  REQUIRE(granit::material::migrate_material_instance_data(source_metadata, source, target_metadata,
                                                           migrated, report) ==
          granit::material::migration_error::none);
  REQUIRE(migrated != nullptr);
  CHECK(report.copied_constant_parameters == 2);
  CHECK(report.defaulted_constant_parameters == 2);
  CHECK(report.pending_resource_parameters == 1);
  CHECK(report.issues.size() == 2);
  CHECK(std::ranges::equal(migrated->bytes().subspan(16, color.size()), color));
  CHECK(migrated->bytes()[32] == std::byte{0x2a});
  CHECK(migrated->bytes()[36] == std::byte{0x11});
  CHECK(migrated->bytes()[0] == std::byte{1});
  CHECK(migrated->bytes()[4] == std::byte{2});
}

TEST_CASE("材质实例迁移失败不覆盖已有输出") {
  const auto source_metadata = make_source_metadata();
  granit::material::metadata_desc unrelated_desc;
  unrelated_desc.constant_buffer_size = 4;
  unrelated_desc.parameters = {{.name = "value",
                                .type = granit::material::parameter_type::uint32,
                                .offset = 0,
                                .default_value = {}}};
  granit::material::material_metadata unrelated_metadata;
  REQUIRE(
      granit::material::material_metadata::build(std::move(unrelated_desc), unrelated_metadata) ==
      granit::material::metadata_error::none);
  granit::material::material_instance_data unrelated{unrelated_metadata};
  auto output = std::make_unique<granit::material::material_instance_data>(source_metadata);
  auto* original = output.get();
  granit::material::migration_report report;
  CHECK(granit::material::migrate_material_instance_data(source_metadata, unrelated,
                                                         source_metadata, output, report) ==
        granit::material::migration_error::invalid_source);
  CHECK(output.get() == original);
}
