// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "material/pbr_render_graph_adapter.h"

#include <algorithm>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <string_view>
#include <vector>

namespace {

using clock_type = std::chrono::steady_clock;

constexpr granit::material::pbr_matrix4 identity{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};

struct options {
  std::uint32_t objects{100};
  std::uint32_t iterations{10'000};
  std::uint32_t samples{20};
  std::uint32_t warmup{5};
};

bool parse_u32(std::string_view text, std::uint32_t& value) {
  std::uint32_t parsed = 0;
  const auto result = std::from_chars(text.data(), text.data() + text.size(), parsed);
  if (result.ec != std::errc{} || result.ptr != text.data() + text.size() || parsed == 0)
    return false;
  value = parsed;
  return true;
}

bool parse_options(int argc, char** argv, options& config) {
  for (int index = 1; index < argc; ++index) {
    const std::string_view argument{argv[index]};
    if (argument == "--help") {
      std::cout << "用法：granit_pbr_benchmarks [--objects N] [--iterations N] [--samples N] "
                   "[--warmup N]\n";
      return false;
    }
    if (index + 1 >= argc)
      return false;
    const std::string_view value{argv[++index]};
    if (argument == "--objects") {
      if (!parse_u32(value, config.objects))
        return false;
    } else if (argument == "--iterations") {
      if (!parse_u32(value, config.iterations))
        return false;
    } else if (argument == "--samples") {
      if (!parse_u32(value, config.samples))
        return false;
    } else if (argument == "--warmup") {
      if (!parse_u32(value, config.warmup))
        return false;
    } else {
      return false;
    }
  }
  return config.objects <= 1'000'000;
}

double percentile(std::vector<double> values, double fraction) {
  std::ranges::sort(values);
  const auto index = static_cast<std::size_t>(fraction * static_cast<double>(values.size() - 1));
  return values[index];
}

bool build_graph(const options& config, std::uint64_t& checksum) {
  granit::render_graph::serial_graph graph;
  const auto color = graph.import_texture_view(1, true, "PBR Color");
  granit::material::pbr_graph_pass_desc desc{
      .color = color, .view = {.view_projection = identity}, .light = {}, .objects = {}};
  desc.objects.resize(config.objects, {.model = identity, .normal_matrix = identity});
  for (std::uint32_t index = 0; index < config.objects; ++index)
    desc.objects[index].object_id = index;
  const auto pass = granit::material::add_pbr_graph_pass(
      graph, std::move(desc),
      [](granit::render_graph::pass_context&, const granit::material::pbr_frame_constants&,
         std::span<const granit::material::pbr_object_constants>) { return GRANIT_SUCCESS; });
  if (pass == granit::render_graph::invalid_pass_id)
    return false;
  const auto diagnostics = graph.diagnostics();
  checksum += diagnostics.compilation.execution_order.size() + config.objects;
  return diagnostics.compilation.succeeded();
}

} // namespace

int main(int argc, char** argv) {
  options config;
  if (!parse_options(argc, argv, config))
    return argc > 1 && std::string_view{argv[1]} == "--help" ? 0 : 2;

  std::uint64_t checksum = 0;
  for (std::uint32_t sample = 0; sample < config.warmup; ++sample) {
    for (std::uint32_t iteration = 0; iteration < config.iterations; ++iteration) {
      if (!build_graph(config, checksum))
        return 1;
    }
  }
  std::vector<double> samples;
  samples.reserve(config.samples);
  for (std::uint32_t sample = 0; sample < config.samples; ++sample) {
    const auto begin = clock_type::now();
    for (std::uint32_t iteration = 0; iteration < config.iterations; ++iteration) {
      if (!build_graph(config, checksum))
        return 1;
    }
    const auto elapsed =
        std::chrono::duration<double, std::nano>(clock_type::now() - begin).count();
    samples.push_back(elapsed / static_cast<double>(config.iterations));
  }
  const auto mean = std::accumulate(samples.begin(), samples.end(), 0.0) / samples.size();
  std::cout << std::setprecision(10) << "# revision=" << GRANIT_BENCHMARK_REVISION
            << ",compiler=" << GRANIT_BENCHMARK_COMPILER << ",system=" << GRANIT_BENCHMARK_SYSTEM
            << ",link=" << GRANIT_BENCHMARK_LINK_MODE << '\n'
            << "schema,name,objects,iterations,samples,mean_ns,p50_ns,p95_ns,p99_ns\n"
            << "1,pbr_graph_build," << config.objects << ',' << config.iterations << ','
            << config.samples << ',' << mean << ',' << percentile(samples, 0.50) << ','
            << percentile(samples, 0.95) << ',' << percentile(samples, 0.99) << '\n';
  std::cerr << "pbr_graph_build checksum=" << checksum << '\n';
  return 0;
}
