// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "scene/multi_view_submission.h"

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
constexpr granit::scene::matrix4 identity{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};

struct options {
  std::uint32_t objects{1000};
  std::uint32_t iterations{1000};
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
      std::cout << "用法：granit_scene_benchmarks [--objects N] [--iterations N] [--samples N] "
                   "[--warmup N]\n";
      return false;
    }
    if (index + 1 >= argc)
      return false;
    const std::string_view value{argv[++index]};
    auto* target = argument == "--objects"      ? &config.objects
                   : argument == "--iterations" ? &config.iterations
                   : argument == "--samples"    ? &config.samples
                   : argument == "--warmup"     ? &config.warmup
                                                : nullptr;
    if (target == nullptr || !parse_u32(value, *target))
      return false;
  }
  return config.objects <= 1'000'000;
}

double percentile(std::vector<double> values, double fraction) {
  std::ranges::sort(values);
  return values[static_cast<std::size_t>(fraction * static_cast<double>(values.size() - 1))];
}

struct fixture {
  std::vector<granit::scene::renderable_input> renderables;
  std::array<granit::scene::view_input, 2> views;
  std::array<granit::scene::directional_light_input, 1> lights;
};

fixture make_fixture(std::uint32_t object_count) {
  fixture value;
  value.views = {granit::scene::view_input{identity, identity, identity, {}, {0, 0, 1280, 720}, 1},
                 granit::scene::view_input{identity, identity, identity, {}, {0, 0, 640, 480}, 2}};
  value.lights = {granit::scene::directional_light_input{{0, 0, 1}, {1, 1, 1}, 3}};
  value.renderables.reserve(object_count);
  for (std::uint32_t index = 0; index < object_count; ++index) {
    const bool outside = index % 4 == 0;
    value.renderables.push_back({.model = identity,
                                 .normal_matrix = identity,
                                 .bounds = {{outside ? 2.0F : 0.0F, 0, 0.5F}, 0.1F},
                                 .layer_mask = index % 2 == 0 ? 1U : 2U,
                                 .sort_key = object_count - index,
                                 .payload = index,
                                 .object_id = index});
  }
  return value;
}

bool run_once(const fixture& data, std::uint64_t& checksum) {
  granit::scene::multi_view_snapshot snapshot;
  const auto result = granit::scene::build_multi_view_snapshot({.views = data.views,
                                                                .renderables = data.renderables,
                                                                .directional_lights = data.lights,
                                                                .point_lights = {},
                                                                .spot_lights = {}},
                                                               snapshot);
  if (result != granit::scene::multi_view_error::none)
    return false;
  for (const auto& view : snapshot.views())
    checksum += view.renderables.indices().size() + view.directional_lights.size();
  return true;
}

} // namespace

int main(int argc, char** argv) {
  options config;
  if (!parse_options(argc, argv, config))
    return argc > 1 && std::string_view{argv[1]} == "--help" ? 0 : 2;
  const auto data = make_fixture(config.objects);
  std::uint64_t checksum = 0;
  for (std::uint32_t warmup = 0; warmup < config.warmup; ++warmup) {
    for (std::uint32_t iteration = 0; iteration < config.iterations; ++iteration) {
      if (!run_once(data, checksum))
        return 1;
    }
  }
  std::vector<double> samples;
  samples.reserve(config.samples);
  for (std::uint32_t sample = 0; sample < config.samples; ++sample) {
    const auto begin = clock_type::now();
    for (std::uint32_t iteration = 0; iteration < config.iterations; ++iteration) {
      if (!run_once(data, checksum))
        return 1;
    }
    const auto elapsed =
        std::chrono::duration<double, std::nano>(clock_type::now() - begin).count();
    samples.push_back(elapsed / config.iterations);
  }
  const auto mean =
      std::accumulate(samples.begin(), samples.end(), 0.0) / static_cast<double>(samples.size());
  std::cout << std::setprecision(10) << "# revision=" << GRANIT_BENCHMARK_REVISION
            << ",compiler=" << GRANIT_BENCHMARK_COMPILER << ",system=" << GRANIT_BENCHMARK_SYSTEM
            << ",link=" << GRANIT_BENCHMARK_LINK_MODE << '\n'
            << "schema,name,objects,views,iterations,samples,mean_ns,p50_ns,p95_ns,p99_ns\n"
            << "1,multi_view_snapshot," << config.objects << ",2," << config.iterations << ','
            << config.samples << ',' << mean << ',' << percentile(samples, 0.50) << ','
            << percentile(samples, 0.95) << ',' << percentile(samples, 0.99) << '\n';
  std::cerr << "multi_view_snapshot checksum=" << checksum << '\n';
  return 0;
}
