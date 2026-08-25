// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "material/pbr_draw_inputs.h"
#include "pipeline/canvas_draw_list.h"
#include "pipeline/canvas_geometry_upload.h"
#include "pipeline/canvas_pass.h"

#include <granit/granit.hpp>
#include <granit/pipeline/material.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <numeric>
#include <span>
#include <vector>

namespace {

using granit::pipeline::detail::canvas_draw_list;
using granit::pipeline::detail::canvas_geometry_upload;
using granit::pipeline::detail::canvas_vertex;

granit::math::matrix4 identity() { return {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}; }

double percentile(std::vector<double> values, double fraction) {
  std::ranges::sort(values);
  return values[static_cast<std::size_t>(fraction * static_cast<double>(values.size() - 1))];
}

std::vector<char> load_package() {
  std::ifstream stream{GRANIT_CANVAS_BENCHMARK_PACKAGE, std::ios::binary};
  return {std::istreambuf_iterator<char>{stream}, {}};
}

canvas_draw_list make_list(std::uint32_t rectangles, granit_texture_view first,
                           granit_texture_view second, granit_sampler sampler, bool alternating) {
  constexpr std::array vertices{canvas_vertex{-0.06F, -0.06F, 0, 0, UINT32_MAX},
                                canvas_vertex{0.06F, -0.06F, 1, 0, UINT32_MAX},
                                canvas_vertex{0.06F, 0.06F, 1, 1, UINT32_MAX},
                                canvas_vertex{-0.06F, 0.06F, 0, 1, UINT32_MAX}};
  constexpr std::array<std::uint32_t, 6> indices{0, 1, 2, 0, 2, 3};
  canvas_draw_list list;
  for (std::uint32_t index = 0; index < rectangles; ++index) {
    const bool alternate = alternating && index % 2 != 0;
    const auto result = list.append(
        vertices, indices,
        {.texture = alternate ? second : first,
         .sampler = sampler,
         .scissor = alternate ? granit_scissor{1, 0, 63, 64} : granit_scissor{0, 0, 64, 64}});
    if (result != GRANIT_SUCCESS)
      return {};
  }
  return list;
}

void print_result(std::string_view name, std::uint32_t items, std::uint32_t batches,
                  std::uint32_t coverage_layers, const std::vector<double>& samples) {
  const auto mean = std::accumulate(samples.begin(), samples.end(), 0.0) / samples.size();
  std::cout << "2," << name << ',' << items << ',' << batches << ',' << coverage_layers << ','
            << samples.size() << ',' << mean << ',' << percentile(samples, 0.50) << ','
            << percentile(samples, 0.95) << ',' << percentile(samples, 0.99) << '\n';
}

void print_cpu_result(std::string_view phase, std::string_view name, std::uint32_t items,
                      std::uint32_t batches, const std::vector<double>& samples) {
  const auto mean = std::accumulate(samples.begin(), samples.end(), 0.0) / samples.size();
  std::cout << "3," << phase << '.' << name << ',' << items << ',' << batches << ",0,"
            << samples.size() << ',' << mean << ',' << percentile(samples, 0.50) << ','
            << percentile(samples, 0.95) << ',' << percentile(samples, 0.99) << '\n';
}

granit::result record_clear(granit_renderer renderer, granit_command_recorder recorder,
                            granit_texture_view output, std::uint32_t size) {
  granit_color_attachment_desc color = GRANIT_COLOR_ATTACHMENT_DESC_INIT;
  color.view = output;
  color.clear_value.alpha = 0.0F;
  granit_rendering_desc rendering = GRANIT_RENDERING_DESC_INIT;
  rendering.color_attachment_count = 1;
  rendering.color_attachments = &color;
  rendering.area = {0, 0, size, size};
  auto result = granit_command_recorder_begin_rendering(renderer, recorder, &rendering);
  if (result == GRANIT_SUCCESS)
    result = granit_command_recorder_end_rendering(renderer, recorder);
  return granit::from_native(result);
}

} // namespace

int main() {
  constexpr std::uint32_t size = 64;
  granit::renderer renderer;
  if (renderer.initialize({.application_name = "granit-ui-gpu-benchmark"}) !=
      granit::result::success) {
    std::cerr << "无法创建 Vulkan Renderer\n";
    return 1;
  }
  const auto native = renderer.native_handle();
  granit::texture output;
  granit::texture_view output_view;
  if (output.initialize(native, {.format = granit::texture_format::rgba8_unorm,
                                 .usage = granit::texture_usage::color_attachment,
                                 .width = size,
                                 .height = size}) != granit::result::success ||
      output_view.initialize(native, output.native_handle()) != granit::result::success) {
    return 1;
  }
  const granit::texture_desc sampled_desc{.format = granit::texture_format::rgba8_unorm,
                                          .usage = granit::texture_usage::sampled |
                                                   granit::texture_usage::transfer_destination};
  granit::texture first_texture;
  granit::texture second_texture;
  granit::texture_view first_view;
  granit::texture_view second_view;
  granit::sampler sampler;
  constexpr std::array<std::uint8_t, 4> white{255, 255, 255, 32};
  if (first_texture.initialize(native, sampled_desc) != granit::result::success ||
      second_texture.initialize(native, sampled_desc) != granit::result::success ||
      first_texture.write(std::as_bytes(std::span{white}), {}, {}) != granit::result::success ||
      second_texture.write(std::as_bytes(std::span{white}), {}, {}) != granit::result::success ||
      first_view.initialize(native, first_texture.native_handle()) != granit::result::success ||
      second_view.initialize(native, second_texture.native_handle()) != granit::result::success ||
      sampler.initialize(
          native, {.mag_filter = granit::filter::nearest, .min_filter = granit::filter::nearest}) !=
          granit::result::success) {
    return 1;
  }

  const auto archive = load_package();
  const std::array<float, 4> color{0.125F, 0.125F, 0.125F, 1.0F};
  const std::array updates{
      granit_material_parameter_update{granit_material_parameter_id("base_color", 10),
                                       GRANIT_MATERIAL_PARAMETER_FLOAT4, 0, color.data(),
                                       sizeof(color), GRANIT_NULL_HANDLE},
      granit_material_parameter_update{granit_material_parameter_id("base_color_texture", 18),
                                       GRANIT_MATERIAL_PARAMETER_TEXTURE_VIEW, 0, nullptr, 0,
                                       first_view.native_handle()},
      granit_material_parameter_update{granit_material_parameter_id("unlit_sampler", 13),
                                       GRANIT_MATERIAL_PARAMETER_SAMPLER, 0, nullptr, 0,
                                       sampler.native_handle()}};
  granit_material_desc material_desc = GRANIT_MATERIAL_DESC_INIT;
  material_desc.archive_data = archive.data();
  material_desc.archive_size = archive.size();
  material_desc.initial_updates = updates.data();
  material_desc.initial_update_count = static_cast<std::uint32_t>(updates.size());
  granit_material material = GRANIT_NULL_HANDLE;
  if (archive.empty() ||
      granit_material_create(native, &material_desc, &material) != GRANIT_SUCCESS)
    return 1;

  granit::command_recorder recorder;
  granit::timestamp_query_pool timestamps;
  if (recorder.initialize(native) != granit::result::success ||
      timestamps.initialize(native, 2) != granit::result::success) {
    return 1;
  }
  const granit::material::pbr_frame_constants frame{.view_projection = identity(),
                                                    .camera_position = {},
                                                    .direction_to_light = {},
                                                    .light_radiance = {}};
  const granit::material::pbr_object_constants object{
      .model = identity(), .normal_matrix = identity(), .object_id = {}};

  std::cout << "# revision=" << GRANIT_BENCHMARK_REVISION
            << ",compiler=" << GRANIT_BENCHMARK_COMPILER << ",system=" << GRANIT_BENCHMARK_SYSTEM
            << ",link=" << GRANIT_BENCHMARK_LINK_MODE << '\n'
            << "schema,name,items,batches,coverage_layers,samples,mean_ns,p50_ns,p95_ns,p99_ns\n";
  bool succeeded = true;
  struct benchmark_case {
    std::string_view name;
    std::uint32_t items;
    bool alternating;
  };
  constexpr std::array cases{benchmark_case{"transparent_overlap_clear", 0, false},
                             benchmark_case{"transparent_overlap_compatible", 2, false},
                             benchmark_case{"transparent_overlap_alternating", 2, true},
                             benchmark_case{"transparent_overlap_compatible", 8, false},
                             benchmark_case{"transparent_overlap_alternating", 8, true},
                             benchmark_case{"transparent_overlap_compatible", 32, false},
                             benchmark_case{"transparent_overlap_alternating", 32, true},
                             benchmark_case{"canvas_compatible", 100, false},
                             benchmark_case{"canvas_alternating", 100, true},
                             benchmark_case{"canvas_compatible", 1'000, false},
                             benchmark_case{"canvas_alternating", 1'000, true},
                             benchmark_case{"canvas_compatible", 10'000, false},
                             benchmark_case{"canvas_alternating", 10'000, true}};
  for (const auto& benchmark_case : cases) {
    auto list =
        make_list(benchmark_case.items, first_view.native_handle(), second_view.native_handle(),
                  sampler.native_handle(), benchmark_case.alternating);
    const auto batches = list.batches();
    canvas_geometry_upload geometry;
    if (!list.items().empty() && geometry.upload(native, list) != GRANIT_SUCCESS)
      return 1;
    const auto iterations = benchmark_case.items == 10'000 ? 2U : 10U;
    constexpr std::uint32_t warmup = 3;
    constexpr std::uint32_t sample_count = 15;
    std::vector<double> samples;
    std::vector<double> record_samples;
    std::vector<double> submit_samples;
    std::vector<double> reset_samples;
    for (std::uint32_t sample = 0; sample < warmup + sample_count; ++sample) {
      double total = 0;
      double record_total = 0;
      double submit_total = 0;
      double reset_total = 0;
      for (std::uint32_t iteration = 0; iteration < iterations; ++iteration) {
        auto result = recorder.begin();
        if (granit::succeeded(result))
          result = recorder.reset_timestamp_queries(timestamps.native_handle(), 0, 2);
        if (granit::succeeded(result)) {
          result =
              recorder.write_timestamp(timestamps.native_handle(), GRANIT_TIMESTAMP_STAGE_TOP, 0);
        }
        if (granit::succeeded(result)) {
          const auto record_begin = std::chrono::steady_clock::now();
          result = list.items().empty()
                       ? record_clear(native, recorder.native_handle(), output_view.native_handle(),
                                      size)
                       : granit::from_native(granit::pipeline::detail::record_canvas_pass(
                             native, recorder.native_handle(),
                             {.color = output_view.native_handle(),
                              .color_format = GRANIT_TEXTURE_FORMAT_RGBA8_UNORM,
                              .width = size,
                              .height = size,
                              .material = material,
                              .frame = frame,
                              .object = object,
                              .load_operation = GRANIT_ATTACHMENT_LOAD_OPERATION_CLEAR},
                             list, geometry));
          record_total += std::chrono::duration<double, std::nano>(
                              std::chrono::steady_clock::now() - record_begin)
                              .count();
        }
        if (granit::succeeded(result)) {
          result = recorder.write_timestamp(timestamps.native_handle(),
                                            GRANIT_TIMESTAMP_STAGE_BOTTOM, 1);
        }
        if (granit::succeeded(result))
          result = recorder.end();
        if (granit::succeeded(result)) {
          const auto submit_begin = std::chrono::steady_clock::now();
          result = recorder.submit();
          submit_total += std::chrono::duration<double, std::nano>(
                              std::chrono::steady_clock::now() - submit_begin)
                              .count();
        }
        if (granit::succeeded(result)) {
          const auto reset_begin = std::chrono::steady_clock::now();
          result = recorder.reset();
          reset_total += std::chrono::duration<double, std::nano>(std::chrono::steady_clock::now() -
                                                                  reset_begin)
                             .count();
        }
        std::array<std::uint64_t, 2> values{};
        if (granit::succeeded(result))
          result = timestamps.get_results(0, values);
        if (granit::failed(result) || values[1] < values[0]) {
          succeeded = false;
          break;
        }
        total += static_cast<double>(values[1] - values[0]);
      }
      if (!succeeded)
        break;
      if (sample >= warmup) {
        samples.push_back(total / iterations);
        record_samples.push_back(record_total / iterations);
        submit_samples.push_back(submit_total / iterations);
        reset_samples.push_back(reset_total / iterations);
      }
    }
    if (!succeeded)
      break;
    print_result(benchmark_case.name, benchmark_case.items,
                 static_cast<std::uint32_t>(batches.size()), benchmark_case.items, samples);
    print_cpu_result("cpu_record", benchmark_case.name, benchmark_case.items,
                     static_cast<std::uint32_t>(batches.size()), record_samples);
    print_cpu_result("cpu_submit", benchmark_case.name, benchmark_case.items,
                     static_cast<std::uint32_t>(batches.size()), submit_samples);
    print_cpu_result("cpu_reset_wait", benchmark_case.name, benchmark_case.items,
                     static_cast<std::uint32_t>(batches.size()), reset_samples);
  }
  static_cast<void>(granit_material_destroy(native, material));
  return succeeded ? 0 : 1;
}
