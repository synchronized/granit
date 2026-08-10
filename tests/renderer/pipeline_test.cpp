// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/renderer/buffer.hpp>
#include <granit/renderer/command_recorder.hpp>
#include <granit/renderer/pipeline.hpp>
#include <granit/renderer/renderer.hpp>
#include <granit/renderer/sampler.hpp>
#include <granit/renderer/shader.hpp>
#include <granit/renderer/texture.h>

#include <catch2/catch_all.hpp>

#include <atomic>
#include <barrier>
#include <cstddef>
#include <fstream>
#include <iterator>
#include <span>
#include <string>
#include <thread>
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

TEST_CASE("不可变 Bind Group 保持 Buffer 与 Sampler 生命周期", "[pipeline][bind-group]") {
  granit::renderer renderer;
  const auto result =
      renderer.initialize({.application_name = "granit-bind-group", .enable_validation = true});
  if (environment_unavailable(result) || result == granit::result::unsupported)
    SKIP("当前运行环境不支持验证层或没有满足要求的 Vulkan 设备");
  REQUIRE(result == granit::result::success);

  const std::array declarations{
      granit::bind_group_layout_entry{.binding = 0,
                                      .type = granit::binding_type::uniform_buffer,
                                      .visibility = granit::shader_stage_flags::vertex},
      granit::bind_group_layout_entry{.binding = 1,
                                      .type = granit::binding_type::sampler,
                                      .visibility = granit::shader_stage_flags::fragment},
      granit::bind_group_layout_entry{.binding = 2,
                                      .type = granit::binding_type::sampled_texture,
                                      .visibility = granit::shader_stage_flags::fragment}};
  granit::bind_group_layout layout;
  REQUIRE(layout.initialize(renderer.native_handle(), declarations) == granit::result::success);
  const auto layout_handle = layout.native_handle();
  granit::pipeline_layout binding_pipeline_layout;
  REQUIRE(binding_pipeline_layout.initialize(
              renderer.native_handle(), std::span{&layout_handle, 1}) == granit::result::success);
  granit::buffer buffer;
  REQUIRE(buffer.initialize(renderer.native_handle(),
                            {.size = 256, .usage = granit::buffer_usage::uniform}) ==
          granit::result::success);
  granit::sampler sampler;
  REQUIRE(sampler.initialize(renderer.native_handle(), {}) == granit::result::success);
  granit_texture_desc texture_desc = GRANIT_TEXTURE_DESC_INIT;
  texture_desc.format = GRANIT_TEXTURE_FORMAT_RGBA8_UNORM;
  texture_desc.usage = GRANIT_TEXTURE_USAGE_SAMPLED_BIT;
  granit_texture texture = GRANIT_NULL_HANDLE;
  granit_texture_view view = GRANIT_NULL_HANDLE;
  REQUIRE(granit_texture_create_with_default_view(renderer.native_handle(), &texture_desc, &texture,
                                                  &view) == GRANIT_SUCCESS);
  const std::array entries{
      granit::bind_group_entry{.binding = 0, .resource = buffer.native_handle(), .size = 128},
      granit::bind_group_entry{.binding = 1, .resource = sampler.native_handle()},
      granit::bind_group_entry{.binding = 2, .resource = view}};
  granit::bind_group group;
  REQUIRE(group.initialize(renderer.native_handle(), layout.native_handle(), entries) ==
          granit::result::success);
  granit::command_recorder recorder;
  REQUIRE(recorder.initialize(renderer.native_handle()) == granit::result::success);
  REQUIRE(recorder.begin() == granit::result::success);
  const granit_bind_group group_handle = group.native_handle();
  granit::bind_group_layout incompatible_group_layout;
  REQUIRE(incompatible_group_layout.initialize(renderer.native_handle(), declarations) ==
          granit::result::success);
  const auto incompatible_group_layout_handle = incompatible_group_layout.native_handle();
  granit::pipeline_layout incompatible_pipeline_layout;
  REQUIRE(incompatible_pipeline_layout.initialize(
              renderer.native_handle(), std::span{&incompatible_group_layout_handle, 1}) ==
          granit::result::success);
  CHECK(recorder.bind_graphics_groups(incompatible_pipeline_layout.native_handle(), 0,
                                      std::span{&group_handle, 1}) ==
        granit::result::invalid_argument);
  REQUIRE(recorder.bind_graphics_groups(binding_pipeline_layout.native_handle(), 0,
                                        std::span{&group_handle, 1}) == granit::result::success);
  REQUIRE(buffer.reset() == granit::result::success);
  REQUIRE(sampler.reset() == granit::result::success);
  REQUIRE(granit_texture_destroy(renderer.native_handle(), texture) == GRANIT_SUCCESS);
  REQUIRE(layout.reset() == granit::result::success);
  REQUIRE(binding_pipeline_layout.reset() == granit::result::success);
  REQUIRE(group.reset() == granit::result::success);
  REQUIRE(incompatible_pipeline_layout.reset() == granit::result::success);
  REQUIRE(incompatible_group_layout.reset() == granit::result::success);
  REQUIRE(recorder.end() == granit::result::success);
  REQUIRE(recorder.submit() == granit::result::success);
  REQUIRE(recorder.reset() == granit::result::success);
}

TEST_CASE("Bind Group 校验完整性和资源类型", "[pipeline][bind-group][validation]") {
  granit_bind_group group = GRANIT_NULL_HANDLE;
  granit_bind_group_desc desc = GRANIT_BIND_GROUP_DESC_INIT;
  CHECK(granit_bind_group_create(UINT64_C(1), &desc, &group) == GRANIT_ERROR_INVALID_ARGUMENT);
  desc.layout = UINT64_C(1);
  granit_bind_group_entry entry{0, 0, GRANIT_NULL_HANDLE, 0, GRANIT_WHOLE_SIZE};
  desc.entry_count = 1;
  desc.entries = &entry;
  CHECK(granit_bind_group_create(UINT64_C(1), &desc, &group) == GRANIT_ERROR_INVALID_ARGUMENT);
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

  desc.sample_count = GRANIT_SAMPLE_COUNT_1;
  desc.primitive.topology = UINT32_C(99);
  CHECK(granit_graphics_pipeline_create(UINT64_C(1), &desc, &pipeline) ==
        GRANIT_ERROR_INVALID_ARGUMENT);
  desc.primitive.topology = GRANIT_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

  granit_depth_state depth = GRANIT_DEPTH_STATE_INIT;
  depth.test_enabled = 1;
  desc.depth_stencil_format = GRANIT_TEXTURE_FORMAT_UNDEFINED;
  desc.depth = &depth;
  CHECK(granit_graphics_pipeline_create(UINT64_C(1), &desc, &pipeline) ==
        GRANIT_ERROR_INVALID_ARGUMENT);
  desc.depth = nullptr;
  desc.depth_stencil_format = GRANIT_TEXTURE_FORMAT_D32_FLOAT;

  granit_color_blend_state blend = GRANIT_COLOR_BLEND_STATE_INIT;
  desc.color_blend_count = 1;
  desc.color_blends = &blend;
  CHECK(granit_graphics_pipeline_create(UINT64_C(1), &desc, &pipeline) ==
        GRANIT_ERROR_INVALID_ARGUMENT);
  desc.color_blend_count = 0;
  desc.color_blends = nullptr;

  const granit_vertex_attribute duplicate_locations[] = {{0, GRANIT_VERTEX_FORMAT_FLOAT32X2, 0, 0},
                                                         {0, GRANIT_VERTEX_FORMAT_FLOAT32X3, 8, 0}};
  const granit_vertex_buffer_layout vertex_layout{20, GRANIT_VERTEX_STEP_MODE_VERTEX, 2, 0,
                                                  duplicate_locations};
  desc.vertex_buffer_layout_count = 1;
  desc.vertex_buffer_layouts = &vertex_layout;
  CHECK(granit_graphics_pipeline_create(UINT64_C(1), &desc, &pipeline) ==
        GRANIT_ERROR_INVALID_ARGUMENT);
}

TEST_CASE("Graphics Pipeline 接受 Vertex Buffer Layout", "[pipeline][vertex-input]") {
  granit::renderer renderer;
  const auto result =
      renderer.initialize({.application_name = "granit-vertex-input", .enable_validation = true});
  if (environment_unavailable(result) || result == granit::result::unsupported)
    SKIP("当前运行环境不支持验证层或没有满足要求的 Vulkan 设备");
  REQUIRE(result == granit::result::success);

  const auto vertex_code = load_shader("vertex_input.vert.spv");
  const auto fragment_code = load_shader("vertex_input.frag.spv");
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
  const std::array attributes{
      granit::vertex_attribute{
          .location = 0, .format = granit::vertex_format::float32x2, .offset = 0, .reserved = 0},
      granit::vertex_attribute{
          .location = 1, .format = granit::vertex_format::float32x3, .offset = 8, .reserved = 0}};
  const granit::vertex_attribute instance_attribute{
      .location = 2, .format = granit::vertex_format::float32x2, .offset = 0, .reserved = 0};
  const std::array vertex_buffers{
      granit::vertex_buffer_layout{
          .stride = 20, .step_mode = granit::vertex_step_mode::vertex, .attributes = attributes},
      granit::vertex_buffer_layout{.stride = 8,
                                   .step_mode = granit::vertex_step_mode::instance,
                                   .attributes = std::span{&instance_attribute, 1}}};
  const granit::texture_format format = granit::texture_format::rgba8_unorm;
  const granit::color_blend_state blend{.enabled = true,
                                        .source_color_factor = granit::blend_factor::source_alpha,
                                        .destination_color_factor =
                                            granit::blend_factor::one_minus_source_alpha,
                                        .color_operation = granit::blend_operation::add,
                                        .source_alpha_factor = granit::blend_factor::one,
                                        .destination_alpha_factor = granit::blend_factor::zero,
                                        .alpha_operation = granit::blend_operation::add,
                                        .write_mask = granit::color_write_mask::all};
  granit::graphics_pipeline pipeline;
  REQUIRE(
      pipeline.initialize(renderer.native_handle(),
                          {.layout = layout.native_handle(),
                           .vertex_shader = vertex.native_handle(),
                           .fragment_shader = fragment.native_handle(),
                           .color_formats = std::span{&format, 1},
                           .depth_stencil_format = granit::texture_format::d16_unorm,
                           .vertex_buffers = vertex_buffers,
                           .primitive = {.topology = granit::primitive_topology::triangle_strip,
                                         .front = granit::front_face::clockwise,
                                         .cull = granit::cull_mode::back,
                                         .polygon = granit::polygon_mode::fill},
                           .depth = granit::depth_state{.test_enabled = true,
                                                        .write_enabled = true,
                                                        .compare = granit::compare_operation::less},
                           .color_blends = std::span{&blend, 1}}) == granit::result::success);
}

TEST_CASE("Graphics Pipeline 持有 Shader 与 Layout 依赖", "[pipeline][lifetime]") {
  granit::renderer renderer;
  const auto result = renderer.initialize(
      {.application_name = "granit-graphics-pipeline", .enable_validation = true});
  if (environment_unavailable(result) || result == granit::result::unsupported)
    SKIP("当前运行环境不支持验证层或没有满足要求的 Vulkan 设备");
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
                               .color_formats = std::span{&format, 1},
                               .vertex_buffers = {},
                               .primitive = {},
                               .depth = {},
                               .color_blends = {}}) == granit::result::success);
  granit_texture_desc texture_desc = GRANIT_TEXTURE_DESC_INIT;
  texture_desc.format = GRANIT_TEXTURE_FORMAT_RGBA8_UNORM;
  texture_desc.usage = GRANIT_TEXTURE_USAGE_COLOR_ATTACHMENT_BIT;
  texture_desc.width = 16;
  texture_desc.height = 16;
  granit_texture texture = GRANIT_NULL_HANDLE;
  granit_texture_view view = GRANIT_NULL_HANDLE;
  REQUIRE(granit_texture_create_with_default_view(renderer.native_handle(), &texture_desc, &texture,
                                                  &view) == GRANIT_SUCCESS);
  granit::buffer index_buffer;
  REQUIRE(index_buffer.initialize(renderer.native_handle(),
                                  {.size = 16, .usage = granit::buffer_usage::index}) ==
          granit::result::success);
  granit::buffer vertex_buffer;
  REQUIRE(vertex_buffer.initialize(renderer.native_handle(),
                                   {.size = 16, .usage = granit::buffer_usage::vertex}) ==
          granit::result::success);
  granit::command_recorder recorder;
  REQUIRE(recorder.initialize(renderer.native_handle()) == granit::result::success);
  REQUIRE(recorder.begin() == granit::result::success);
  REQUIRE(recorder.bind_graphics_pipeline(pipeline.native_handle()) == granit::result::success);
  REQUIRE(recorder.bind_index_buffer(index_buffer.native_handle(), 0, granit::index_type::uint16) ==
          granit::result::success);
  const granit::vertex_buffer_binding vertex_binding{vertex_buffer.native_handle(), 0};
  REQUIRE(recorder.bind_vertex_buffers(0, std::span{&vertex_binding, 1}) ==
          granit::result::success);
  const granit::viewport viewport{0, 0, 16, 16, 0, 1};
  const granit::scissor scissor{0, 0, 16, 16};
  REQUIRE(recorder.set_viewports(0, std::span{&viewport, 1}) == granit::result::success);
  REQUIRE(recorder.set_scissors(0, std::span{&scissor, 1}) == granit::result::success);
  const granit::color_attachment_desc color{.view = view};
  const granit::rendering_desc rendering{.color_attachments = std::span{&color, 1},
                                         .area = {0, 0, 16, 16}};
  REQUIRE(recorder.begin_rendering(rendering) == granit::result::success);
  REQUIRE(recorder.draw(3) == granit::result::success);
  REQUIRE(recorder.draw_indexed(3) == granit::result::success);
  REQUIRE(recorder.end_rendering() == granit::result::success);
  REQUIRE(index_buffer.reset() == granit::result::success);
  REQUIRE(vertex_buffer.reset() == granit::result::success);
  REQUIRE(pipeline.reset() == granit::result::success);
  REQUIRE(vertex.reset() == granit::result::success);
  REQUIRE(fragment.reset() == granit::result::success);
  REQUIRE(layout.reset() == granit::result::success);
  REQUIRE(recorder.end() == granit::result::success);
  REQUIRE(recorder.submit() == granit::result::success);
  REQUIRE(recorder.reset() == granit::result::success);
  REQUIRE(granit_texture_destroy(renderer.native_handle(), texture) == GRANIT_SUCCESS);
}

TEST_CASE("Compute Pipeline 校验阶段并持有 Shader 与 Layout", "[pipeline][compute][lifetime]") {
  granit::renderer renderer;
  const auto result = renderer.initialize(
      {.application_name = "granit-compute-pipeline", .enable_validation = true});
  if (environment_unavailable(result) || result == granit::result::unsupported)
    SKIP("当前运行环境不支持验证层或没有满足要求的 Vulkan 设备");
  REQUIRE(result == granit::result::success);

  const auto compute_code = load_shader("minimal.comp.spv");
  const auto vertex_code = load_shader("minimal.vert.spv");
  REQUIRE_FALSE(compute_code.empty());
  granit::shader compute;
  granit::shader vertex;
  REQUIRE(compute.initialize(renderer.native_handle(),
                             {.stage = granit::shader_stage::compute, .code = compute_code}) ==
          granit::result::success);
  REQUIRE(vertex.initialize(renderer.native_handle(),
                            {.stage = granit::shader_stage::vertex, .code = vertex_code}) ==
          granit::result::success);
  granit::pipeline_layout layout;
  REQUIRE(layout.initialize(renderer.native_handle()) == granit::result::success);

  granit::compute_pipeline wrong_stage;
  CHECK(wrong_stage.initialize(
            renderer.native_handle(),
            {.layout = layout.native_handle(), .compute_shader = vertex.native_handle()}) ==
        granit::result::invalid_handle);
  granit::compute_pipeline pipeline;
  REQUIRE(
      pipeline.initialize(renderer.native_handle(), {.layout = layout.native_handle(),
                                                     .compute_shader = compute.native_handle()}) ==
      granit::result::success);
  const auto handle = pipeline.native_handle();
  granit::command_recorder recorder;
  REQUIRE(recorder.initialize(renderer.native_handle()) == granit::result::success);
  REQUIRE(recorder.begin() == granit::result::success);
  CHECK(recorder.dispatch(1) == granit::result::invalid_argument);
  REQUIRE(recorder.bind_compute_pipeline(pipeline.native_handle()) == granit::result::success);
  CHECK(recorder.dispatch(0) == granit::result::invalid_argument);

  granit_texture_desc texture_desc = GRANIT_TEXTURE_DESC_INIT;
  texture_desc.format = GRANIT_TEXTURE_FORMAT_RGBA8_UNORM;
  texture_desc.usage = GRANIT_TEXTURE_USAGE_COLOR_ATTACHMENT_BIT;
  granit_texture texture = GRANIT_NULL_HANDLE;
  granit_texture_view view = GRANIT_NULL_HANDLE;
  REQUIRE(granit_texture_create_with_default_view(renderer.native_handle(), &texture_desc, &texture,
                                                  &view) == GRANIT_SUCCESS);
  const granit::color_attachment_desc color{.view = view};
  const granit::rendering_desc rendering{.color_attachments = std::span{&color, 1},
                                         .area = {0, 0, 1, 1}};
  REQUIRE(recorder.begin_rendering(rendering) == granit::result::success);
  CHECK(recorder.dispatch(1) == granit::result::invalid_argument);
  REQUIRE(recorder.end_rendering() == granit::result::success);
  REQUIRE(recorder.dispatch(1) == granit::result::success);
  REQUIRE(compute.reset() == granit::result::success);
  REQUIRE(layout.reset() == granit::result::success);
  REQUIRE(pipeline.reset() == granit::result::success);
  REQUIRE(recorder.end() == granit::result::success);
  REQUIRE(recorder.submit() == granit::result::success);
  REQUIRE(recorder.reset() == granit::result::success);
  REQUIRE(recorder.begin() == granit::result::success);
  CHECK(recorder.dispatch(1) == granit::result::invalid_argument);
  REQUIRE(recorder.end() == granit::result::success);
  REQUIRE(recorder.submit() == granit::result::success);
  REQUIRE(recorder.reset() == granit::result::success);
  REQUIRE(granit_texture_view_destroy(renderer.native_handle(), view) == GRANIT_SUCCESS);
  REQUIRE(granit_texture_destroy(renderer.native_handle(), texture) == GRANIT_SUCCESS);
  CHECK(granit_compute_pipeline_destroy(renderer.native_handle(), handle) ==
        GRANIT_ERROR_INVALID_HANDLE);
}

TEST_CASE("Graphics 与 Compute Pipeline 支持多线程并发创建", "[pipeline][concurrency]") {
  granit::renderer renderer;
  const auto result = renderer.initialize({.application_name = "granit-concurrent-pipelines"});
  if (environment_unavailable(result))
    SKIP("当前运行环境没有满足要求的 Vulkan 设备");
  REQUIRE(result == granit::result::success);

  const auto vertex_code = load_shader("minimal.vert.spv");
  const auto fragment_code = load_shader("minimal.frag.spv");
  const auto compute_code = load_shader("minimal.comp.spv");
  granit::shader vertex;
  granit::shader fragment;
  granit::shader compute;
  REQUIRE(vertex.initialize(renderer.native_handle(),
                            {.stage = granit::shader_stage::vertex, .code = vertex_code}) ==
          granit::result::success);
  REQUIRE(fragment.initialize(renderer.native_handle(),
                              {.stage = granit::shader_stage::fragment, .code = fragment_code}) ==
          granit::result::success);
  REQUIRE(compute.initialize(renderer.native_handle(),
                             {.stage = granit::shader_stage::compute, .code = compute_code}) ==
          granit::result::success);
  granit::pipeline_layout layout;
  REQUIRE(layout.initialize(renderer.native_handle()) == granit::result::success);

  const granit_texture_format format = GRANIT_TEXTURE_FORMAT_RGBA8_UNORM;
  granit_graphics_pipeline_desc graphics_desc = GRANIT_GRAPHICS_PIPELINE_DESC_INIT;
  graphics_desc.layout = layout.native_handle();
  graphics_desc.vertex_shader = vertex.native_handle();
  graphics_desc.fragment_shader = fragment.native_handle();
  graphics_desc.color_format_count = 1;
  graphics_desc.color_formats = &format;
  granit_compute_pipeline_desc compute_desc = GRANIT_COMPUTE_PIPELINE_DESC_INIT;
  compute_desc.layout = layout.native_handle();
  compute_desc.compute_shader = compute.native_handle();

  constexpr std::size_t thread_count = 6;
  constexpr std::size_t iterations = 4;
  std::barrier start{thread_count};
  std::atomic_uint32_t failures{};
  std::vector<std::thread> threads;
  threads.reserve(thread_count);
  for (std::size_t thread_index = 0; thread_index < thread_count; ++thread_index) {
    threads.emplace_back([&] {
      start.arrive_and_wait();
      for (std::size_t iteration = 0; iteration < iterations; ++iteration) {
        granit_graphics_pipeline graphics = GRANIT_NULL_HANDLE;
        granit_compute_pipeline compute_pipeline = GRANIT_NULL_HANDLE;
        if (granit_graphics_pipeline_create(renderer.native_handle(), &graphics_desc, &graphics) !=
            GRANIT_SUCCESS) {
          ++failures;
          continue;
        }
        if (granit_compute_pipeline_create(renderer.native_handle(), &compute_desc,
                                           &compute_pipeline) != GRANIT_SUCCESS)
          ++failures;
        if (compute_pipeline != GRANIT_NULL_HANDLE &&
            granit_compute_pipeline_destroy(renderer.native_handle(), compute_pipeline) !=
                GRANIT_SUCCESS)
          ++failures;
        if (granit_graphics_pipeline_destroy(renderer.native_handle(), graphics) != GRANIT_SUCCESS)
          ++failures;
      }
    });
  }
  for (auto& thread : threads)
    thread.join();
  CHECK(failures.load() == 0);
}

TEST_CASE("Compute Dispatch 写入 Storage Buffer 并自动同步 Copy", "[pipeline][compute][storage]") {
  granit::renderer renderer;
  const auto result = renderer.initialize(
      {.application_name = "granit-compute-storage", .enable_validation = true});
  if (environment_unavailable(result) || result == granit::result::unsupported)
    SKIP("当前运行环境不支持验证层或没有满足要求的 Vulkan 设备");
  REQUIRE(result == granit::result::success);

  constexpr std::uint64_t buffer_size = 16 * sizeof(std::uint32_t);
  granit::buffer storage;
  REQUIRE(storage.initialize(
              renderer.native_handle(),
              {.size = buffer_size,
               .usage = granit::buffer_usage::storage | granit::buffer_usage::transfer_source,
               .location = granit::memory_location::device}) == granit::result::success);
  granit::buffer readback;
  REQUIRE(readback.initialize(renderer.native_handle(),
                              {.size = buffer_size,
                               .usage = granit::buffer_usage::transfer_destination,
                               .location = granit::memory_location::readback}) ==
          granit::result::success);

  const granit::bind_group_layout_entry declaration{.binding = 0,
                                                    .type = granit::binding_type::storage_buffer,
                                                    .visibility =
                                                        granit::shader_stage_flags::compute};
  granit::bind_group_layout group_layout;
  REQUIRE(group_layout.initialize(renderer.native_handle(), std::span{&declaration, 1}) ==
          granit::result::success);
  const auto group_layout_handle = group_layout.native_handle();
  granit::pipeline_layout pipeline_layout;
  REQUIRE(
      pipeline_layout.initialize(renderer.native_handle(), std::span{&group_layout_handle, 1}) ==
      granit::result::success);
  const granit::bind_group_entry entry{
      .binding = 0, .resource = storage.native_handle(), .size = buffer_size};
  granit::bind_group group;
  REQUIRE(group.initialize(renderer.native_handle(), group_layout.native_handle(),
                           std::span{&entry, 1}) == granit::result::success);

  const auto code = load_shader("storage_buffer.comp.spv");
  granit::shader shader;
  REQUIRE(shader.initialize(renderer.native_handle(), {.stage = granit::shader_stage::compute,
                                                       .code = code}) == granit::result::success);
  granit::compute_pipeline pipeline;
  REQUIRE(
      pipeline.initialize(renderer.native_handle(), {.layout = pipeline_layout.native_handle(),
                                                     .compute_shader = shader.native_handle()}) ==
      granit::result::success);

  granit::command_recorder recorder;
  REQUIRE(recorder.initialize(renderer.native_handle()) == granit::result::success);
  REQUIRE(recorder.begin() == granit::result::success);
  REQUIRE(recorder.bind_compute_pipeline(pipeline.native_handle()) == granit::result::success);
  const auto group_handle = group.native_handle();
  REQUIRE(recorder.bind_compute_groups(pipeline_layout.native_handle(), 0,
                                       std::span{&group_handle, 1}) == granit::result::success);
  REQUIRE(recorder.dispatch(16) == granit::result::success);
  const granit::buffer_copy_region copy{
      .source_offset = 0, .destination_offset = 0, .size = buffer_size};
  REQUIRE(recorder.copy_buffer(storage.native_handle(), readback.native_handle(),
                               std::span{&copy, 1}) == granit::result::success);
  REQUIRE(recorder.end() == granit::result::success);
  REQUIRE(recorder.submit() == granit::result::success);
  REQUIRE(recorder.reset() == granit::result::success);

  void* mapped = nullptr;
  REQUIRE(readback.map(0, buffer_size, &mapped) == granit::result::success);
  const auto* values = static_cast<const std::uint32_t*>(mapped);
  for (std::uint32_t index = 0; index < 16; ++index)
    CHECK(values[index] == index * 3 + 7);
  REQUIRE(readback.unmap() == granit::result::success);
}

} // namespace
