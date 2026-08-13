// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "lighting/light_data.h"

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
  std::uint32_t lights{64};
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
      std::cout << "用法：granit_lighting_benchmarks [--lights N] [--iterations N] "
                   "[--samples N] [--warmup N]\n";
      return false;
    }
    if (index + 1 >= argc)
      return false;
    const std::string_view value{argv[++index]};
    auto* target = argument == "--lights"       ? &config.lights
                   : argument == "--iterations" ? &config.iterations
                   : argument == "--samples"    ? &config.samples
                   : argument == "--warmup"     ? &config.warmup
                                                : nullptr;
    if (target == nullptr || !parse_u32(value, *target))
      return false;
  }
  return config.lights <= granit::lighting::maximum_point_lights;
}

double percentile(std::vector<double> values, double fraction) {
  std::ranges::sort(values);
  return values[static_cast<std::size_t>(fraction * static_cast<double>(values.size() - 1))];
}

bool make_snapshot(std::uint32_t light_count, granit::scene::multi_view_snapshot& snapshot) {
  const granit::scene::view_input view{identity, identity, identity, {}, {0, 0, 1280, 720}, 1};
  std::vector<granit::scene::point_light_input> lights;
  lights.reserve(light_count);
  for (std::uint32_t index = 0; index < light_count; ++index) {
    const auto x = static_cast<float>(index % 16U) * 0.05F - 0.375F;
    const auto y = static_cast<float>(index / 16U) * 0.05F - 0.375F;
    lights.push_back({.position = {x, y, 0.5F},
                      .intensity = {1.0F, 0.75F, 0.5F},
                      .radius = 2.0F,
                      .layer_mask = 1});
  }
  return granit::scene::build_multi_view_snapshot(
             {.views = std::span{&view, 1}, .point_lights = lights}, snapshot) ==
         granit::scene::multi_view_error::none;
}

bool pack_once(const granit::scene::multi_view_snapshot& snapshot, std::uint32_t light_count,
               std::uint64_t& checksum) {
  granit::lighting::packed_view_lights packed;
  granit::lighting::light_requirements requirements;
  if (granit::lighting::pack_view_lights(
          snapshot, 0, {.directional = 0, .point = light_count, .spot = 0}, packed, requirements) !=
      granit::lighting::light_pack_error::none)
    return false;
  checksum += packed.point.size() + requirements.point;
  return true;
}

} // namespace

int main(int argc, char** argv) {
  options config;
  if (!parse_options(argc, argv, config))
    return argc > 1 && std::string_view{argv[1]} == "--help" ? 0 : 2;

  granit::scene::multi_view_snapshot snapshot;
  if (!make_snapshot(config.lights, snapshot))
    return 1;

  std::uint64_t checksum = 0;
  for (std::uint32_t sample = 0; sample < config.warmup; ++sample) {
    for (std::uint32_t iteration = 0; iteration < config.iterations; ++iteration) {
      if (!pack_once(snapshot, config.lights, checksum))
        return 1;
    }
  }

  std::vector<double> samples;
  samples.reserve(config.samples);
  for (std::uint32_t sample = 0; sample < config.samples; ++sample) {
    const auto begin = clock_type::now();
    for (std::uint32_t iteration = 0; iteration < config.iterations; ++iteration) {
      if (!pack_once(snapshot, config.lights, checksum))
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
            << "schema,name,lights,iterations,samples,mean_ns,p50_ns,p95_ns,p99_ns\n"
            << "1,pack_view_point_lights," << config.lights << ',' << config.iterations << ','
            << config.samples << ',' << mean << ',' << percentile(samples, 0.50) << ','
            << percentile(samples, 0.95) << ',' << percentile(samples, 0.99) << '\n';
  std::cerr << "pack_view_point_lights checksum=" << checksum << '\n';
  return 0;
}
