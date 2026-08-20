// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/granit.hpp>
#include <granit/pipeline/render_pipeline.h>
#ifdef GRANIT_RENDER_PIPELINE_CPU_BENCHMARK
#include "pipeline/render_pipeline_metrics.h"
#endif

#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <numeric>
#include <string_view>
#include <vector>

#ifndef GRANIT_BENCHMARK_REVISION
#define GRANIT_BENCHMARK_REVISION "unknown"
#define GRANIT_BENCHMARK_COMPILER "unknown"
#define GRANIT_BENCHMARK_SYSTEM "unknown"
#define GRANIT_BENCHMARK_LINK_MODE "unknown"
#endif

namespace {

granit_matrix4 identity() {
  granit_matrix4 value{};
  value.elements[0] = 1.0F;
  value.elements[5] = 1.0F;
  value.elements[10] = 1.0F;
  value.elements[15] = 1.0F;
  return value;
}

std::vector<char> load_package() {
  std::ifstream stream{GRANIT_RENDER_PIPELINE_EXAMPLE_PACKAGE, std::ios::binary};
  return {std::istreambuf_iterator<char>{stream}, {}};
}

bool check(granit_result result, const char* operation) {
  if (result == GRANIT_SUCCESS)
    return true;
  std::cerr << operation << "失败，结果码：" << result << '\n';
  return false;
}

#ifdef GRANIT_RENDER_PIPELINE_CPU_BENCHMARK
struct benchmark_options {
  std::uint32_t iterations = 20;
  std::uint32_t samples = 20;
  std::uint32_t warmup = 5;
  std::uint32_t draws = 100;
  std::uint32_t materials = 8;
  std::uint32_t texture_groups = 8;
  std::uint32_t point_lights = 16;
  std::string_view shadow_range = "medium";
  float shadow_half_extent = 20.0F;
};

bool parse_u32(std::string_view text, std::uint32_t& value) {
  const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
  return result.ec == std::errc{} && result.ptr == text.data() + text.size();
}

bool parse_options(int argc, char** argv, benchmark_options& options) {
  for (int index = 1; index < argc; ++index) {
    const std::string_view argument{argv[index]};
    if (argument == "--help") {
      std::cout << "用法：granit_render_pipeline_benchmarks [--iterations N] [--samples N] "
                   "[--warmup N] [--draws N] [--materials N] [--texture-groups N] "
                   "[--lights N] [--shadow-range near|medium|far]\n";
      return false;
    }
    if (index + 1 >= argc)
      return false;
    const std::string_view value{argv[++index]};
    if (argument == "--shadow-range") {
      options.shadow_range = value;
      options.shadow_half_extent = value == "near"     ? 5.0F
                                   : value == "medium" ? 20.0F
                                   : value == "far"    ? 80.0F
                                                       : 0.0F;
      if (options.shadow_half_extent == 0.0F)
        return false;
      continue;
    }
    auto* target = argument == "--iterations"       ? &options.iterations
                   : argument == "--samples"        ? &options.samples
                   : argument == "--warmup"         ? &options.warmup
                   : argument == "--draws"          ? &options.draws
                   : argument == "--materials"      ? &options.materials
                   : argument == "--texture-groups" ? &options.texture_groups
                   : argument == "--lights"         ? &options.point_lights
                                                    : nullptr;
    if (target == nullptr || !parse_u32(value, *target))
      return false;
  }
  return options.iterations != 0 && options.iterations <= 10'000 && options.samples != 0 &&
         options.samples <= 1'000 && options.warmup <= 1'000 && options.draws != 0 &&
         options.draws <= 10'000 && options.materials != 0 && options.materials <= 512 &&
         options.materials <= options.draws && options.texture_groups != 0 &&
         options.texture_groups <= options.materials && options.point_lights <= 128;
}

struct benchmark_summary {
  double mean = 0.0;
  double p50 = 0.0;
  double p95 = 0.0;
  double p99 = 0.0;
};

benchmark_summary summarize(std::vector<double> values) {
  std::ranges::sort(values);
  const auto percentile = [&](double fraction) {
    const auto rank =
        static_cast<std::size_t>(std::ceil(fraction * static_cast<double>(values.size())) - 1.0);
    return values[std::min(rank, values.size() - 1)];
  };
  return {.mean = std::accumulate(values.begin(), values.end(), 0.0) /
                  static_cast<double>(values.size()),
          .p50 = percentile(0.50),
          .p95 = percentile(0.95),
          .p99 = percentile(0.99)};
}

granit_result record_minimal_stage(const granit_render_pipeline_record_info* info,
                                   void* user_data) {
  if (info == nullptr || user_data == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const auto renderer = *static_cast<const granit_renderer*>(user_data);
  granit_color_attachment_desc color = GRANIT_COLOR_ATTACHMENT_DESC_INIT;
  granit_depth_stencil_attachment_desc depth = GRANIT_DEPTH_STENCIL_ATTACHMENT_DESC_INIT;
  depth.view = info->depth_output;
  granit_rendering_desc rendering = GRANIT_RENDERING_DESC_INIT;
  if (info->depth_output != GRANIT_NULL_HANDLE)
    rendering.depth_stencil_attachment = &depth;
  if (info->stage == GRANIT_RENDER_PIPELINE_STAGE_SHADOW) {
    rendering.area = {0, 0, 1024, 1024};
  } else {
    color.view = info->color_output;
    color.clear_value = {.red = 0.25F, .green = 0.5F, .blue = 1.0F, .alpha = 1.0F};
    rendering.color_attachment_count = 1;
    rendering.color_attachments = &color;
    rendering.area = {0, 0, static_cast<std::uint32_t>(info->view->viewport_width),
                      static_cast<std::uint32_t>(info->view->viewport_height)};
  }
  auto result = granit_command_recorder_begin_rendering(renderer, info->recorder, &rendering);
  if (result == GRANIT_SUCCESS)
    result = granit_command_recorder_end_rendering(renderer, info->recorder);
  return result;
}
#endif

} // namespace

int main(int argc, char** argv) {
#ifdef GRANIT_RENDER_PIPELINE_CPU_BENCHMARK
  benchmark_options benchmark;
  if (!parse_options(argc, argv, benchmark))
    return argc > 1 && std::string_view{argv[1]} == "--help" ? 0 : 2;
#else
  static_cast<void>(argc);
  static_cast<void>(argv);
#endif
  granit::renderer renderer;
  if (granit::failed(renderer.initialize({.application_name = "Granit Render Pipeline"}))) {
    std::cerr << "当前环境无法创建 Vulkan Renderer\n";
    return 1;
  }
  auto native_renderer = renderer.native_handle();
  constexpr std::uint32_t size = 64;

  granit_texture output = GRANIT_NULL_HANDLE;
  granit_texture_view output_view = GRANIT_NULL_HANDLE;
  granit_texture_desc output_desc = GRANIT_TEXTURE_DESC_INIT;
  output_desc.format = GRANIT_TEXTURE_FORMAT_RGBA8_UNORM;
  output_desc.usage =
      GRANIT_TEXTURE_USAGE_COLOR_ATTACHMENT_BIT | GRANIT_TEXTURE_USAGE_TRANSFER_SOURCE_BIT;
  output_desc.width = size;
  output_desc.height = size;
  if (!check(granit_texture_create_with_default_view(native_renderer, &output_desc, &output,
                                                     &output_view),
             "创建输出纹理")) {
    return 1;
  }

  constexpr std::array<float, 9> positions{-0.65F, -0.65F, 0.5F,  0.65F, -0.65F,
                                           0.5F,   0.0F,   0.65F, 0.5F};
  granit::buffer vertex_buffer;
  if (granit::failed(vertex_buffer.initialize(native_renderer,
                                              {.size = sizeof(positions),
                                               .usage = granit::buffer_usage::vertex,
                                               .location = granit::memory_location::device},
                                              std::as_bytes(std::span{positions})))) {
    std::cerr << "创建顶点缓冲失败\n";
    return 1;
  }
  const granit_vertex_attribute attribute{0, GRANIT_VERTEX_FORMAT_FLOAT32X3, 0, 0};
  const granit_mesh_vertex_buffer vertex{
      vertex_buffer.native_handle(), 0, {12, GRANIT_VERTEX_STEP_MODE_VERTEX, 1, 0, &attribute}};
  granit_mesh_desc mesh_desc = GRANIT_MESH_DESC_INIT;
  mesh_desc.vertex_buffers = &vertex;
  mesh_desc.vertex_buffer_count = 1;
  mesh_desc.vertex_count = 3;
  granit_mesh mesh = GRANIT_NULL_HANDLE;
  if (!check(granit_mesh_create(native_renderer, &mesh_desc, &mesh), "创建 Mesh"))
    return 1;

  const auto archive = load_package();
#ifdef GRANIT_RENDER_PIPELINE_CPU_BENCHMARK
  const auto material_count = benchmark.materials;
  const auto texture_group_count = benchmark.texture_groups;
#else
  constexpr std::uint32_t material_count = 1;
#endif
#ifdef GRANIT_RENDER_PIPELINE_CPU_BENCHMARK
  std::vector<granit::texture> workload_textures(texture_group_count);
  std::vector<granit::texture_view> workload_texture_views(texture_group_count);
  for (std::uint32_t index = 0; index < texture_group_count; ++index) {
    const granit::texture_desc texture_desc{.format = granit::texture_format::rgba8_unorm,
                                            .usage = granit::texture_usage::sampled |
                                                     granit::texture_usage::transfer_destination};
    if (granit::failed(workload_textures[index].initialize(native_renderer, texture_desc)) ||
        granit::failed(workload_texture_views[index].initialize(
            native_renderer, workload_textures[index].native_handle()))) {
      std::cerr << "创建纹理组失败\n";
      return 1;
    }
    const std::array<std::byte, 4> pixel{std::byte{static_cast<std::uint8_t>((index * 37U) % 256U)},
                                         std::byte{static_cast<std::uint8_t>((index * 53U) % 256U)},
                                         std::byte{static_cast<std::uint8_t>((index * 71U) % 256U)},
                                         std::byte{255}};
    if (granit::failed(workload_textures[index].write(pixel, {}, {}))) {
      std::cerr << "写入纹理组失败\n";
      return 1;
    }
  }
  granit::sampler workload_sampler;
  if (granit::failed(workload_sampler.initialize(native_renderer))) {
    std::cerr << "创建纹理组采样器失败\n";
    return 1;
  }
#endif
  granit_material_desc material_desc = GRANIT_MATERIAL_DESC_INIT;
  material_desc.archive_data = archive.data();
  material_desc.archive_size = archive.size();
#ifdef GRANIT_RENDER_PIPELINE_CPU_BENCHMARK
  material_desc.initial_update_count = 3;
#else
  material_desc.initial_update_count = 1;
#endif
  std::vector<granit_material> materials(material_count, GRANIT_NULL_HANDLE);
#ifdef GRANIT_RENDER_PIPELINE_CPU_BENCHMARK
  std::vector<double> material_create_samples;
  std::vector<double> material_update_samples;
  material_create_samples.reserve(material_count);
  material_update_samples.reserve(material_count);
#endif
  if (archive.empty())
    return 1;
  for (std::uint32_t index = 0; index < material_count; ++index) {
    const std::array<float, 4> base_color{
        0.2F + 0.6F * static_cast<float>((index * 37U) % 101U) / 100.0F,
        0.2F + 0.6F * static_cast<float>((index * 53U) % 101U) / 100.0F,
        0.2F + 0.6F * static_cast<float>((index * 71U) % 101U) / 100.0F, 1.0F};
#ifdef GRANIT_RENDER_PIPELINE_CPU_BENCHMARK
    const auto texture_group = index * texture_group_count / material_count;
    const std::array updates{
        granit_material_parameter_update{granit_material_parameter_id("base_color", 10),
                                         GRANIT_MATERIAL_PARAMETER_FLOAT4, 0, base_color.data(),
                                         sizeof(base_color), GRANIT_NULL_HANDLE},
        granit_material_parameter_update{granit_material_parameter_id("base_color_texture", 18),
                                         GRANIT_MATERIAL_PARAMETER_TEXTURE_VIEW, 0, nullptr, 0,
                                         workload_texture_views[texture_group].native_handle()},
        granit_material_parameter_update{granit_material_parameter_id("pbr_sampler", 11),
                                         GRANIT_MATERIAL_PARAMETER_SAMPLER, 0, nullptr, 0,
                                         workload_sampler.native_handle()}};
    material_desc.initial_updates = updates.data();
#else
    const granit_material_parameter_update update{granit_material_parameter_id("base_color", 10),
                                                  GRANIT_MATERIAL_PARAMETER_FLOAT4,
                                                  0,
                                                  base_color.data(),
                                                  sizeof(base_color),
                                                  GRANIT_NULL_HANDLE};
    material_desc.initial_updates = &update;
#endif
#ifdef GRANIT_RENDER_PIPELINE_CPU_BENCHMARK
    const auto create_begin = std::chrono::steady_clock::now();
#endif
    if (!check(granit_material_create(native_renderer, &material_desc, &materials[index]),
               "创建 Material"))
      return 1;
#ifdef GRANIT_RENDER_PIPELINE_CPU_BENCHMARK
    material_create_samples.push_back(
        std::chrono::duration<double, std::nano>(std::chrono::steady_clock::now() - create_begin)
            .count());
#endif
  }
#ifdef GRANIT_RENDER_PIPELINE_CPU_BENCHMARK
  for (std::uint32_t index = 0; index < material_count; ++index) {
    const auto texture_group = index * texture_group_count / material_count;
    const auto replacement_group = (texture_group + 1U) % texture_group_count;
    const granit_material_parameter_update update{
        granit_material_parameter_id("base_color_texture", 18),
        GRANIT_MATERIAL_PARAMETER_TEXTURE_VIEW,
        0,
        nullptr,
        0,
        workload_texture_views[replacement_group].native_handle()};
    const auto update_begin = std::chrono::steady_clock::now();
    if (!check(granit_material_update(native_renderer, materials[index], &update, 1),
               "更新 Material 纹理组"))
      return 1;
    material_update_samples.push_back(
        std::chrono::duration<double, std::nano>(std::chrono::steady_clock::now() - update_begin)
            .count());
  }
#endif

  granit_scene_view view{};
  view.view = identity();
  view.projection = identity();
  view.view_projection = identity();
  view.viewport_width = static_cast<float>(size);
  view.viewport_height = static_cast<float>(size);
  view.layer_mask = UINT64_MAX;
#ifdef GRANIT_RENDER_PIPELINE_CPU_BENCHMARK
  const auto draw_count = benchmark.draws;
  const auto point_light_count = benchmark.point_lights;
#else
  constexpr std::uint32_t draw_count = 1;
  constexpr std::uint32_t point_light_count = 0;
#endif
  std::vector<granit_scene_renderable> renderables(draw_count);
  const auto columns = static_cast<std::uint32_t>(std::ceil(std::sqrt(draw_count)));
  const auto cell_size = 1.6F / static_cast<float>(columns);
  for (std::uint32_t index = 0; index < draw_count; ++index) {
    auto& renderable = renderables[index];
    const auto column = index % columns;
    const auto row = index / columns;
    const auto x = -0.8F + (static_cast<float>(column) + 0.5F) * cell_size;
    const auto y = -0.8F + (static_cast<float>(row) + 0.5F) * cell_size;
    renderable.model = identity();
    renderable.model.elements[0] = cell_size;
    renderable.model.elements[5] = cell_size;
    renderable.model.elements[12] = x;
    renderable.model.elements[13] = y;
    renderable.normal_matrix = identity();
    renderable.bounds_center = {x, y, 0.5F};
    renderable.bounds_radius = cell_size;
    renderable.layer_mask = UINT64_MAX;
    renderable.sort_key = index % material_count;
    renderable.payload = static_cast<std::uint64_t>(index) + 1;
    renderable.object_id = index + 1;
  }
  std::vector<granit_scene_point_light> point_lights(point_light_count);
  for (std::uint32_t index = 0; index < point_light_count; ++index) {
    const auto angle = 6.283185307F * static_cast<float>(index) /
                       static_cast<float>(std::max(point_light_count, 1U));
    point_lights[index] = {.position = {0.65F * std::cos(angle), 0.65F * std::sin(angle), 0.2F},
                           .intensity = {0.2F, 0.2F, 0.2F},
                           .radius = 2.0F,
                           .layer_mask = UINT64_MAX};
  }
  const granit_scene_directional_light light{.direction_to_light = {0.0F, 0.0F, 1.0F},
                                             .radiance = {1.0F, 1.0F, 1.0F},
                                             .layer_mask = UINT64_MAX};
  granit_scene_snapshot_desc scene_desc = GRANIT_SCENE_SNAPSHOT_DESC_INIT;
  scene_desc.views = &view;
  scene_desc.view_count = 1;
  scene_desc.renderables = renderables.data();
  scene_desc.renderable_count = draw_count;
  scene_desc.directional_lights = &light;
  scene_desc.directional_light_count = 1;
  scene_desc.point_lights = point_lights.data();
  scene_desc.point_light_count = point_light_count;
  granit_scene_snapshot scene = GRANIT_NULL_HANDLE;
  if (!check(granit_scene_snapshot_create(native_renderer, &scene_desc, &scene), "创建 Scene"))
    return 1;
#ifdef GRANIT_RENDER_PIPELINE_CPU_BENCHMARK
  granit_scene_snapshot no_light_scene = GRANIT_NULL_HANDLE;
  scene_desc.directional_lights = nullptr;
  scene_desc.directional_light_count = 0;
  scene_desc.point_lights = nullptr;
  scene_desc.point_light_count = 0;
  if (!check(granit_scene_snapshot_create(native_renderer, &scene_desc, &no_light_scene),
             "创建无光照 Scene")) {
    return 1;
  }
#endif

  granit_render_pipeline pipeline = GRANIT_NULL_HANDLE;
  const granit_render_pipeline_desc pipeline_desc = GRANIT_RENDER_PIPELINE_DESC_INIT;
  if (!check(granit_render_pipeline_create(native_renderer, &pipeline_desc, &pipeline),
             "创建 Render Pipeline")) {
    return 1;
  }
#ifdef GRANIT_RENDER_PIPELINE_CPU_BENCHMARK
  if (!check(granit_render_pipeline_shadow_half_extent_set(native_renderer, pipeline,
                                                           benchmark.shadow_half_extent),
             "设置阴影覆盖范围")) {
    return 1;
  }
  granit_render_pipeline callback_pipeline = GRANIT_NULL_HANDLE;
  granit_render_pipeline_desc callback_pipeline_desc = GRANIT_RENDER_PIPELINE_DESC_INIT;
  callback_pipeline_desc.record = record_minimal_stage;
  callback_pipeline_desc.user_data = &native_renderer;
  if (!check(granit_render_pipeline_create(native_renderer, &callback_pipeline_desc,
                                           &callback_pipeline),
             "创建最小回调 Render Pipeline")) {
    return 1;
  }
  if (!check(granit_render_pipeline_shadow_half_extent_set(native_renderer, callback_pipeline,
                                                           benchmark.shadow_half_extent),
             "设置最小回调阴影覆盖范围")) {
    return 1;
  }
#endif
  std::vector<granit_render_pipeline_draw_binding> bindings(draw_count);
  for (std::uint32_t index = 0; index < draw_count; ++index) {
    bindings[index] = {static_cast<std::uint64_t>(index) + 1, mesh,
                       materials[index % material_count], 0};
  }
#ifdef GRANIT_RENDER_PIPELINE_CPU_BENCHMARK
  std::uint32_t material_switch_count = 0;
  std::uint32_t texture_group_switch_count = 0;
  for (std::uint32_t index = 1; index < material_count; ++index) {
    ++material_switch_count;
    const auto previous_group = (index - 1) * texture_group_count / material_count;
    const auto current_group = index * texture_group_count / material_count;
    texture_group_switch_count += previous_group != current_group ? 1U : 0U;
  }
#endif
  granit_render_pipeline_render_desc render_desc = GRANIT_RENDER_PIPELINE_RENDER_DESC_INIT;
  render_desc.scene = scene;
  render_desc.output = output_view;
  render_desc.output_format = GRANIT_TEXTURE_FORMAT_RGBA8_UNORM;
  render_desc.width = size;
  render_desc.height = size;
  render_desc.draw_binding_count = draw_count;
  render_desc.draw_bindings = bindings.data();
  const auto render_once = [&](granit_render_pipeline selected,
                               granit_scene_snapshot selected_scene = GRANIT_NULL_HANDLE) {
    render_desc.scene = selected_scene == GRANIT_NULL_HANDLE ? scene : selected_scene;
    return granit_render_pipeline_render(native_renderer, selected, &render_desc);
  };

#ifdef GRANIT_RENDER_PIPELINE_CPU_BENCHMARK
  const auto run_benchmark = [&](std::string_view name, granit_render_pipeline selected,
                                 granit_scene_snapshot selected_scene) {
    for (std::uint32_t sample = 0; sample < benchmark.warmup; ++sample) {
      for (std::uint32_t iteration = 0; iteration < benchmark.iterations; ++iteration) {
        if (!check(render_once(selected, selected_scene), "预热渲染"))
          return false;
      }
    }
    std::vector<double> samples;
    samples.reserve(benchmark.samples);
    for (std::uint32_t sample = 0; sample < benchmark.samples; ++sample) {
      const auto begin = std::chrono::steady_clock::now();
      for (std::uint32_t iteration = 0; iteration < benchmark.iterations; ++iteration) {
        if (!check(render_once(selected, selected_scene), "基准渲染"))
          return false;
      }
      const auto elapsed =
          std::chrono::duration<double, std::nano>(std::chrono::steady_clock::now() - begin);
      samples.push_back(elapsed.count() / static_cast<double>(benchmark.iterations));
    }
    const auto summary = summarize(std::move(samples));
    const auto automatic = selected == pipeline;
    std::cout << "5," << name << ',' << draw_count << ',' << material_count << ','
              << texture_group_count << ',' << material_switch_count << ','
              << texture_group_switch_count << ',' << material_count << ','
              << (automatic ? draw_count * 2U : 0U) << ',' << (automatic ? draw_count : 0U) << ','
              << point_light_count << ',' << benchmark.shadow_range << ','
              << benchmark.shadow_half_extent * 2.0F << ',' << std::setprecision(8)
              << benchmark.shadow_half_extent * 2.0F / 1024.0F << ',' << std::setprecision(2)
              << draw_count << ',' << benchmark.iterations << ',' << benchmark.samples << ','
              << summary.mean << ',' << summary.p50 << ',' << summary.p95 << ',' << summary.p99
              << '\n';
    return true;
  };
  std::cout << std::fixed << std::setprecision(2) << "# revision=" << GRANIT_BENCHMARK_REVISION
            << ",compiler=" << GRANIT_BENCHMARK_COMPILER << ",system=" << GRANIT_BENCHMARK_SYSTEM
            << ",link=" << GRANIT_BENCHMARK_LINK_MODE << '\n'
            << "schema,name,draws,materials,texture_groups,material_switches,"
               "texture_group_switches,material_bind_groups,pipeline_cache_entries,"
               "opaque_batches,point_lights,shadow_range,shadow_span,"
               "shadow_world_units_per_texel,shadow_casters,iterations,samples,mean_ns,p50_ns,"
               "p95_ns,p99_ns\n";
  const auto print_material_operation = [&](std::string_view name, std::vector<double> samples) {
    const auto summary = summarize(std::move(samples));
    std::cout << "5," << name << ',' << draw_count << ',' << material_count << ','
              << texture_group_count << ',' << material_switch_count << ','
              << texture_group_switch_count << ',' << material_count << ",0,0," << point_light_count
              << ',' << benchmark.shadow_range << ',' << benchmark.shadow_half_extent * 2.0F << ','
              << std::setprecision(8) << benchmark.shadow_half_extent * 2.0F / 1024.0F << ','
              << std::setprecision(2) << draw_count << ",1," << material_count << ','
              << summary.mean << ',' << summary.p50 << ',' << summary.p95 << ',' << summary.p99
              << '\n';
  };
  print_material_operation("material_create_with_bind_group", std::move(material_create_samples));
  print_material_operation("material_bind_group_resource_update",
                           std::move(material_update_samples));
  if (!run_benchmark("automatic_directional_end_to_end", pipeline, scene) ||
      !run_benchmark("minimal_directional_end_to_end", callback_pipeline, scene) ||
      !run_benchmark("automatic_no_light_end_to_end", pipeline, no_light_scene) ||
      !run_benchmark("minimal_no_light_end_to_end", callback_pipeline, no_light_scene)) {
    return 1;
  }
  if (!check(granit_render_pipeline_gpu_metrics_enable(native_renderer, pipeline),
             "启用 GPU 阶段测量"))
    return 1;
  std::array<std::vector<double>, 3> gpu_samples;
  for (auto& values : gpu_samples)
    values.reserve(benchmark.samples);
  for (std::uint32_t sample = 0; sample < benchmark.samples; ++sample) {
    std::array<double, 3> totals{};
    for (std::uint32_t iteration = 0; iteration < benchmark.iterations; ++iteration) {
      if (!check(render_once(pipeline, scene), "GPU 基准渲染"))
        return 1;
      granit_render_pipeline_gpu_metrics metrics{};
      if (!check(granit_render_pipeline_gpu_metrics_get(native_renderer, pipeline, &metrics),
                 "读取 GPU 阶段测量"))
        return 1;
      totals[0] += static_cast<double>(metrics.shadow_ns);
      totals[1] += static_cast<double>(metrics.opaque_ns);
      totals[2] += static_cast<double>(metrics.tone_mapping_ns);
    }
    for (std::size_t index = 0; index < totals.size(); ++index)
      gpu_samples[index].push_back(totals[index] / benchmark.iterations);
  }
  constexpr std::array names{"gpu_shadow", "gpu_opaque", "gpu_tone_mapping"};
  for (std::size_t index = 0; index < names.size(); ++index) {
    const auto summary = summarize(std::move(gpu_samples[index]));
    std::cout << "5," << names[index] << ',' << draw_count << ',' << material_count << ','
              << texture_group_count << ',' << material_switch_count << ','
              << texture_group_switch_count << ',' << material_count << ',' << draw_count * 2U
              << ',' << draw_count << ',' << point_light_count << ',' << benchmark.shadow_range
              << ',' << benchmark.shadow_half_extent * 2.0F << ',' << std::setprecision(8)
              << benchmark.shadow_half_extent * 2.0F / 1024.0F << ',' << std::setprecision(2)
              << draw_count << ',' << benchmark.iterations << ',' << benchmark.samples << ','
              << summary.mean << ',' << summary.p50 << ',' << summary.p95 << ',' << summary.p99
              << '\n';
  }
#else
  if (!check(render_once(pipeline), "渲染"))
    return 1;
#endif

  granit::buffer readback;
  granit::command_recorder recorder;
  const char* readback_stage = "创建 Readback Buffer";
  auto result =
      readback.initialize(native_renderer, {.size = size * size * 4,
                                            .usage = granit::buffer_usage::transfer_destination,
                                            .location = granit::memory_location::readback});
  if (granit::succeeded(result)) {
    readback_stage = "创建 Command Recorder";
    result = recorder.initialize(native_renderer);
  }
  if (granit::succeeded(result)) {
    readback_stage = "开始回读命令";
    result = recorder.begin();
  }
  if (granit::succeeded(result)) {
    readback_stage = "录制纹理回读";
    const granit_texture_data_layout layout{};
    const granit_texture_write_region region{.mip_level = 0,
                                             .base_array_layer = 0,
                                             .array_layer_count = 1,
                                             .aspect = GRANIT_TEXTURE_ASPECT_COLOR_BIT,
                                             .x = 0,
                                             .y = 0,
                                             .z = 0,
                                             .width = size,
                                             .height = size,
                                             .depth = 1};
    result = recorder.copy_texture_to_buffer(output, readback.native_handle(), layout, region);
  }
  if (granit::succeeded(result)) {
    readback_stage = "结束回读命令";
    result = recorder.end();
  }
  if (granit::succeeded(result)) {
    readback_stage = "提交回读命令";
    result = recorder.submit();
  }
  if (granit::succeeded(result)) {
    readback_stage = "重置回读命令";
    result = recorder.reset();
  }
  if (granit::failed(result)) {
    std::cerr << readback_stage << "失败：" << static_cast<granit_result>(result) << '\n';
    return 1;
  }
  void* mapped = nullptr;
  result = readback.map(0, size * size * 4, &mapped);
  const auto* pixel = static_cast<const std::uint8_t*>(mapped) + (size / 2 * size + size / 2) * 4;
  const std::array<std::uint8_t, 3> center{granit::succeeded(result) ? pixel[0] : std::uint8_t{0},
                                           granit::succeeded(result) ? pixel[1] : std::uint8_t{0},
                                           granit::succeeded(result) ? pixel[2] : std::uint8_t{0}};
  const bool rendered = center[0] != 0 || center[1] != 0 || center[2] != 0;
  if (granit::succeeded(result))
    result = readback.unmap();

#ifdef GRANIT_RENDER_PIPELINE_CPU_BENCHMARK
  static_cast<void>(granit_render_pipeline_destroy(native_renderer, callback_pipeline));
  static_cast<void>(granit_scene_snapshot_destroy(native_renderer, no_light_scene));
#endif
  static_cast<void>(granit_render_pipeline_destroy(native_renderer, pipeline));
  static_cast<void>(granit_scene_snapshot_destroy(native_renderer, scene));
  for (const auto material : materials)
    static_cast<void>(granit_material_destroy(native_renderer, material));
  static_cast<void>(granit_mesh_destroy(native_renderer, mesh));
  static_cast<void>(granit_texture_view_destroy(native_renderer, output_view));
  static_cast<void>(granit_texture_destroy(native_renderer, output));
  if (!rendered || granit::failed(result)) {
    std::cerr << "中心像素未被自动渲染路径覆盖\n";
    return 1;
  }
#ifndef GRANIT_RENDER_PIPELINE_CPU_BENCHMARK
  std::cout << "Render Pipeline 离屏渲染成功，中心像素：" << static_cast<int>(center[0]) << ", "
            << static_cast<int>(center[1]) << ", " << static_cast<int>(center[2]) << '\n';
#endif
  return 0;
}
