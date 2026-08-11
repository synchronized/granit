// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "material/material_gpu_instance.h"

#include <granit/renderer/renderer.hpp>
#include <granit/renderer/sampler.h>
#include <granit/renderer/texture.h>

#include <catch2/catch_all.hpp>

#include <array>
#include <bit>
#include <cstddef>

namespace {

bool environment_unavailable(granit::result value) {
  return value == granit::result::backend_unavailable ||
         value == granit::result::incompatible_driver ||
         value == granit::result::no_suitable_device;
}

} // namespace

TEST_CASE("材质 GPU 实例批量上传参数并事务式替换 Bind Group") {
  granit::renderer renderer;
  const auto initialize = renderer.initialize({.application_name = "granit-material-gpu-tests"});
  if (environment_unavailable(initialize)) {
    SKIP("当前运行环境没有满足要求的 Vulkan 设备");
  }
  REQUIRE(initialize == granit::result::success);

  const std::array layout_entries{
      granit_bind_group_layout_entry{0, GRANIT_BINDING_TYPE_UNIFORM_BUFFER, 1,
                                     GRANIT_SHADER_STAGE_FRAGMENT_BIT},
      granit_bind_group_layout_entry{1, GRANIT_BINDING_TYPE_SAMPLED_TEXTURE, 1,
                                     GRANIT_SHADER_STAGE_FRAGMENT_BIT},
      granit_bind_group_layout_entry{2, GRANIT_BINDING_TYPE_SAMPLER, 1,
                                     GRANIT_SHADER_STAGE_FRAGMENT_BIT},
  };
  granit_bind_group_layout_desc layout_desc = GRANIT_BIND_GROUP_LAYOUT_DESC_INIT;
  layout_desc.entry_count = static_cast<std::uint32_t>(layout_entries.size());
  layout_desc.entries = layout_entries.data();
  granit_bind_group_layout layout = GRANIT_NULL_HANDLE;
  REQUIRE(granit_bind_group_layout_create(renderer.native_handle(), &layout_desc, &layout) ==
          GRANIT_SUCCESS);

  granit::material::material_metadata metadata;
  granit::material::metadata_desc metadata_desc;
  metadata_desc.constant_buffer_size = 16;
  metadata_desc.parameters = {
      {.name = "color", .type = granit::material::parameter_type::float4, .default_value = {}},
      {.name = "albedo",
       .type = granit::material::parameter_type::texture_view,
       .binding = 1,
       .default_value = {}},
      {.name = "linear_sampler",
       .type = granit::material::parameter_type::sampler,
       .binding = 2,
       .default_value = {}},
  };
  REQUIRE(granit::material::material_metadata::build(std::move(metadata_desc), metadata) ==
          granit::material::metadata_error::none);

  granit_texture_desc texture_desc = GRANIT_TEXTURE_DESC_INIT;
  texture_desc.format = GRANIT_TEXTURE_FORMAT_RGBA8_UNORM;
  texture_desc.usage = GRANIT_TEXTURE_USAGE_SAMPLED_BIT;
  texture_desc.width = 4;
  texture_desc.height = 4;
  granit_texture texture = GRANIT_NULL_HANDLE;
  granit_texture_view view = GRANIT_NULL_HANDLE;
  REQUIRE(granit_texture_create_with_default_view(renderer.native_handle(), &texture_desc, &texture,
                                                  &view) == GRANIT_SUCCESS);
  granit_sampler_desc sampler_desc = GRANIT_SAMPLER_DESC_INIT;
  granit_sampler sampler = GRANIT_NULL_HANDLE;
  granit_sampler replacement_sampler = GRANIT_NULL_HANDLE;
  REQUIRE(granit_sampler_create(renderer.native_handle(), &sampler_desc, &sampler) ==
          GRANIT_SUCCESS);
  REQUIRE(granit_sampler_create(renderer.native_handle(), &sampler_desc, &replacement_sampler) ==
          GRANIT_SUCCESS);

  granit::material::material_gpu_instance instance;
  REQUIRE(instance.initialize(renderer.native_handle(), layout, metadata) == GRANIT_SUCCESS);
  REQUIRE(instance.uniform_buffer() != GRANIT_NULL_HANDLE);
  CHECK(instance.flush() == GRANIT_ERROR_NOT_READY);
  REQUIRE(instance.set_resource(granit::material::make_parameter_id("albedo"),
                                granit::material::parameter_type::texture_view,
                                view) == granit::material::metadata_error::none);
  REQUIRE(instance.set_resource(granit::material::make_parameter_id("linear_sampler"),
                                granit::material::parameter_type::sampler,
                                sampler) == granit::material::metadata_error::none);
  REQUIRE(instance.flush() == GRANIT_SUCCESS);
  const auto first_group = instance.bind_group();
  REQUIRE(first_group != GRANIT_NULL_HANDLE);

  const auto color =
      std::bit_cast<std::array<std::byte, 16>>(std::array<float, 4>{1.0F, 0.0F, 0.0F, 1.0F});
  REQUIRE(instance.set(granit::material::make_parameter_id("color"),
                       granit::material::parameter_type::float4,
                       color) == granit::material::metadata_error::none);
  REQUIRE(instance.flush() == GRANIT_SUCCESS);
  CHECK(instance.bind_group() == first_group);

  REQUIRE(instance.set_resource(granit::material::make_parameter_id("linear_sampler"),
                                granit::material::parameter_type::sampler,
                                replacement_sampler) == granit::material::metadata_error::none);
  REQUIRE(instance.flush() == GRANIT_SUCCESS);
  CHECK(instance.bind_group() != first_group);
  CHECK(granit_bind_group_destroy(renderer.native_handle(), first_group) ==
        GRANIT_ERROR_INVALID_HANDLE);

  granit::material::material_gpu_instance replacement;
  granit::material::migration_report migration;
  REQUIRE(instance.prepare_migration(layout, metadata, replacement, migration) == GRANIT_SUCCESS);
  CHECK(migration.copied_constant_parameters == 1);
  CHECK(migration.copied_resource_parameters == 2);
  CHECK(migration.pending_resource_parameters == 0);
  REQUIRE(replacement.flush() == GRANIT_SUCCESS);
  const auto replacement_group = replacement.bind_group();
  REQUIRE(replacement_group != GRANIT_NULL_HANDLE);
  CHECK(replacement_group != instance.bind_group());

  instance.swap(replacement);
  CHECK(instance.bind_group() == replacement_group);
  REQUIRE(replacement.reset() == GRANIT_SUCCESS);

  REQUIRE(instance.reset() == GRANIT_SUCCESS);
  CHECK(granit_sampler_destroy(renderer.native_handle(), replacement_sampler) == GRANIT_SUCCESS);
  CHECK(granit_sampler_destroy(renderer.native_handle(), sampler) == GRANIT_SUCCESS);
  CHECK(granit_texture_view_destroy(renderer.native_handle(), view) == GRANIT_SUCCESS);
  CHECK(granit_texture_destroy(renderer.native_handle(), texture) == GRANIT_SUCCESS);
  CHECK(granit_bind_group_layout_destroy(renderer.native_handle(), layout) == GRANIT_SUCCESS);
}
