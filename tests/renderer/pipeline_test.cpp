// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/pipeline.hpp>
#include <granit/renderer.hpp>
#include <granit/shader.hpp>

#include <catch2/catch_all.hpp>

#include <cstddef>
#include <fstream>
#include <iterator>
#include <span>
#include <string>
#include <vector>

namespace {

bool environment_unavailable(granit::result value) {
  return value == granit::result::backend_unavailable ||
         value == granit::result::incompatible_driver ||
         value == granit::result::no_suitable_device;
}

std::vector<std::byte> load_shader(const char* name) {
  std::ifstream stream{std::string{GRANIT_TEST_ASSET_DIR} + "/" + name, std::ios::binary};
  const std::vector<char> bytes{std::istreambuf_iterator<char>{stream}, {}};
  std::vector<std::byte> result(bytes.size());
  for (std::size_t index = 0; index < bytes.size(); ++index)
    result[index] = static_cast<std::byte>(bytes[index]);
  return result;
}

TEST_CASE("空 Pipeline Layout 具有独立生命周期和 Renderer domain", "[pipeline]") {
  granit::renderer first;
  const auto result = first.initialize({.application_name = "granit-pipeline-first"});
  if (environment_unavailable(result))
    SKIP("当前运行环境没有满足要求的 Vulkan 设备");
  REQUIRE(result == granit::result::success);
  granit::renderer second;
  REQUIRE(second.initialize({.application_name = "granit-pipeline-second"}) ==
          granit::result::success);

  granit::pipeline_layout layout;
  REQUIRE(layout.initialize(first.native_handle()) == granit::result::success);
  const auto handle = layout.native_handle();
  CHECK(granit_pipeline_layout_destroy(second.native_handle(), handle) ==
        GRANIT_ERROR_INVALID_HANDLE);
  REQUIRE(layout.reset() == granit::result::success);
  CHECK(granit_pipeline_layout_destroy(first.native_handle(), handle) ==
        GRANIT_ERROR_INVALID_HANDLE);
}

TEST_CASE("Pipeline Layout 保持 Bind Group Layout 依赖", "[pipeline][bind-group-layout]") {
  granit::renderer renderer;
  const auto result = renderer.initialize({.application_name = "granit-bind-group-layout"});
  if (environment_unavailable(result))
    SKIP("当前运行环境没有满足要求的 Vulkan 设备");
  REQUIRE(result == granit::result::success);

  const std::array entries{
      granit::bind_group_layout_entry{.binding = 0,
                                      .type = granit::binding_type::uniform_buffer,
                                      .visibility = granit::shader_stage_flags::vertex},
      granit::bind_group_layout_entry{.binding = 1,
                                      .type = granit::binding_type::sampled_texture,
                                      .visibility = granit::shader_stage_flags::fragment},
      granit::bind_group_layout_entry{.binding = 2,
                                      .type = granit::binding_type::sampler,
                                      .visibility = granit::shader_stage_flags::fragment}};
  granit::bind_group_layout group_layout;
  REQUIRE(group_layout.initialize(renderer.native_handle(), entries) == granit::result::success);
  const granit_bind_group_layout handle = group_layout.native_handle();
  granit::pipeline_layout pipeline_layout;
  REQUIRE(pipeline_layout.initialize(renderer.native_handle(), std::span{&handle, 1}) ==
          granit::result::success);
  REQUIRE(group_layout.reset() == granit::result::success);
  REQUIRE(pipeline_layout.reset() == granit::result::success);
}

TEST_CASE("Bind Group Layout 拒绝重复 binding 和非法可见阶段", "[pipeline][validation]") {
  granit_bind_group_layout layout = GRANIT_NULL_HANDLE;
  const granit_bind_group_layout_entry duplicate[] = {
      {0, GRANIT_BINDING_TYPE_UNIFORM_BUFFER, 1, GRANIT_SHADER_STAGE_VERTEX_BIT},
      {0, GRANIT_BINDING_TYPE_SAMPLER, 1, GRANIT_SHADER_STAGE_FRAGMENT_BIT}};
  granit_bind_group_layout_desc desc = GRANIT_BIND_GROUP_LAYOUT_DESC_INIT;
  desc.entry_count = 2;
  desc.entries = duplicate;
  CHECK(granit_bind_group_layout_create(UINT64_C(1), &desc, &layout) ==
        GRANIT_ERROR_INVALID_ARGUMENT);
  granit_bind_group_layout_entry invalid = duplicate[0];
  invalid.visibility = 0;
  desc.entry_count = 1;
  desc.entries = &invalid;
  CHECK(granit_bind_group_layout_create(UINT64_C(1), &desc, &layout) ==
        GRANIT_ERROR_INVALID_ARGUMENT);
}

TEST_CASE("Graphics Pipeline 在进入后端前校验描述", "[pipeline][validation]") {
  granit_graphics_pipeline pipeline = GRANIT_NULL_HANDLE;
  granit_graphics_pipeline_desc desc = GRANIT_GRAPHICS_PIPELINE_DESC_INIT;
  CHECK(granit_graphics_pipeline_create(UINT64_C(1), &desc, &pipeline) ==
        GRANIT_ERROR_INVALID_ARGUMENT);
  desc.layout = UINT64_C(1);
  desc.vertex_shader = UINT64_C(2);
  desc.fragment_shader = UINT64_C(3);
  desc.depth_stencil_format = GRANIT_TEXTURE_FORMAT_D32_FLOAT;
  desc.sample_count = UINT32_C(3);
  CHECK(granit_graphics_pipeline_create(UINT64_C(1), &desc, &pipeline) ==
        GRANIT_ERROR_INVALID_ARGUMENT);
}

TEST_CASE("Graphics Pipeline 持有 Shader 与 Layout 依赖", "[pipeline][lifetime]") {
  granit::renderer renderer;
  const auto result = renderer.initialize({.application_name = "granit-graphics-pipeline"});
  if (environment_unavailable(result))
    SKIP("当前运行环境没有满足要求的 Vulkan 设备");
  REQUIRE(result == granit::result::success);

  const auto vertex_code = load_shader("minimal.vert.spv");
  const auto fragment_code = load_shader("minimal.frag.spv");
  REQUIRE_FALSE(vertex_code.empty());
  REQUIRE_FALSE(fragment_code.empty());
  granit::shader vertex;
  granit::shader fragment;
  REQUIRE(vertex.initialize(renderer.native_handle(),
                            {.stage = granit::shader_stage::vertex, .code = vertex_code}) ==
          granit::result::success);
  REQUIRE(fragment.initialize(renderer.native_handle(),
                              {.stage = granit::shader_stage::fragment, .code = fragment_code}) ==
          granit::result::success);
  granit::pipeline_layout layout;
  REQUIRE(layout.initialize(renderer.native_handle()) == granit::result::success);

  const granit::texture_format format = granit::texture_format::rgba8_unorm;
  granit::graphics_pipeline pipeline;
  REQUIRE(pipeline.initialize(renderer.native_handle(),
                              {.layout = layout.native_handle(),
                               .vertex_shader = vertex.native_handle(),
                               .fragment_shader = fragment.native_handle(),
                               .color_formats = std::span{&format, 1}}) == granit::result::success);
  REQUIRE(vertex.reset() == granit::result::success);
  REQUIRE(fragment.reset() == granit::result::success);
  REQUIRE(layout.reset() == granit::result::success);
  REQUIRE(pipeline.reset() == granit::result::success);
}

} // namespace
