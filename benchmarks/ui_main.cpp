// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "pipeline/canvas_draw_list.h"
#include "pipeline/ui_geometry_upload.h"

#include <granit/renderer/renderer.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <numeric>
#include <string_view>
#include <vector>

namespace {

using clock_type = std::chrono::steady_clock;
using granit::pipeline::detail::canvas_draw_list;
using granit::pipeline::detail::canvas_draw_state;
using granit::pipeline::detail::canvas_vertex;
using granit::pipeline::detail::ui_geometry_upload;

constexpr std::array quad_vertices{
    canvas_vertex{0, 0, 0, 0, UINT32_MAX}, canvas_vertex{1, 0, 1, 0, UINT32_MAX},
    canvas_vertex{1, 1, 1, 1, UINT32_MAX}, canvas_vertex{0, 1, 0, 1, UINT32_MAX}};
constexpr std::array<std::uint32_t, 6> quad_indices{0, 1, 2, 0, 2, 3};

double percentile(std::vector<double> values, double fraction) {
  std::ranges::sort(values);
  return values[static_cast<std::size_t>(fraction * static_cast<double>(values.size() - 1))];
}

canvas_draw_list make_list(std::uint32_t rectangle_count, bool alternating) {
  canvas_draw_list list;
  for (std::uint32_t index = 0; index < rectangle_count; ++index) {
    const auto state = canvas_draw_state{.texture = alternating && index % 2 != 0 ? 2U : 1U,
                                         .sampler = 3,
                                         .scissor = alternating && index % 2 != 0
                                                        ? granit_scissor{1, 0, 1023, 1024}
                                                        : granit_scissor{0, 0, 1024, 1024}};
    if (list.append(quad_vertices, quad_indices, state) != GRANIT_SUCCESS)
      return {};
  }
  return list;
}

template <typename Operation>
bool run_case(std::string_view name, std::uint32_t rectangles, std::uint32_t iterations,
              Operation&& operation) {
  constexpr std::uint32_t warmup = 3;
  constexpr std::uint32_t sample_count = 20;
  std::uint64_t checksum = 0;
  for (std::uint32_t sample = 0; sample < warmup; ++sample) {
    for (std::uint32_t iteration = 0; iteration < iterations; ++iteration) {
      if (!operation(checksum))
        return false;
    }
  }
  std::vector<double> samples;
  samples.reserve(sample_count);
  for (std::uint32_t sample = 0; sample < sample_count; ++sample) {
    const auto begin = clock_type::now();
    for (std::uint32_t iteration = 0; iteration < iterations; ++iteration) {
      if (!operation(checksum))
        return false;
    }
    const auto elapsed =
        std::chrono::duration<double, std::nano>(clock_type::now() - begin).count();
    samples.push_back(elapsed / iterations);
  }
  const auto mean = std::accumulate(samples.begin(), samples.end(), 0.0) / samples.size();
  std::cout << "1," << name << ',' << rectangles << ',' << iterations << ',' << sample_count << ','
            << mean << ',' << percentile(samples, 0.50) << ',' << percentile(samples, 0.95) << ','
            << percentile(samples, 0.99) << ',' << checksum << '\n';
  return true;
}

} // namespace

int main() {
  std::cout << "# revision=" << GRANIT_BENCHMARK_REVISION
            << ",compiler=" << GRANIT_BENCHMARK_COMPILER << ",system=" << GRANIT_BENCHMARK_SYSTEM
            << ",link=" << GRANIT_BENCHMARK_LINK_MODE << '\n';
  std::cout << "schema,name,rectangles,iterations,samples,mean_ns,p50_ns,p95_ns,p99_ns,checksum\n";

  granit::renderer renderer;
  if (renderer.initialize({.application_name = "granit-ui-benchmark"}) != granit::result::success) {
    std::cerr << "无法创建 Vulkan Renderer\n";
    return 1;
  }
  bool succeeded = true;
  for (const auto rectangles : {100U, 1'000U, 10'000U}) {
    const auto iterations = std::max(1U, 100'000U / rectangles);
    succeeded &= run_case("draw_list_build", rectangles, iterations, [&](std::uint64_t& checksum) {
      auto list = make_list(rectangles, false);
      checksum += list.items().size();
      return list.items().size() == rectangles;
    });
    auto compatible = make_list(rectangles, false);
    succeeded &= run_case("batch_compatible", rectangles, iterations, [&](std::uint64_t& checksum) {
      const auto batches = compatible.batches();
      checksum += batches.size();
      return batches.size() == 1;
    });
    auto alternating = make_list(rectangles, true);
    succeeded &=
        run_case("batch_alternating", rectangles, iterations, [&](std::uint64_t& checksum) {
          const auto batches = alternating.batches();
          checksum += batches.size();
          return batches.size() == rectangles;
        });
    ui_geometry_upload upload;
    succeeded &= run_case("geometry_upload", rectangles, iterations, [&](std::uint64_t& checksum) {
      const auto result = upload.upload(renderer.native_handle(), compatible);
      checksum += upload.index_count();
      return result == GRANIT_SUCCESS;
    });
    std::cout << "# counts rectangles=" << rectangles
              << ",compatible_items=" << compatible.items().size()
              << ",compatible_batches=" << compatible.batches().size()
              << ",compatible_draws=" << compatible.batches().size()
              << ",alternating_batches=" << alternating.batches().size()
              << ",alternating_draws=" << alternating.batches().size() << '\n';
  }
  return succeeded ? 0 : 1;
}
