// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/integrations/imgui/renderer.hpp>
#include <granit/renderer/renderer.hpp>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <numeric>
#include <string_view>
#include <vector>

namespace {

using clock_type = std::chrono::steady_clock;

double percentile(std::vector<double> values, double fraction) {
  std::ranges::sort(values);
  return values[static_cast<std::size_t>(fraction * static_cast<double>(values.size() - 1))];
}

granit::result resolve_texture(ImTextureID texture, granit_canvas_draw_state& state,
                               void*) noexcept {
  state.texture = static_cast<granit_texture_view>(texture + 1);
  state.sampler = 1;
  return granit::result::success;
}

struct draw_fixture {
  ImDrawList list{nullptr};
  ImDrawData data;

  explicit draw_fixture(std::uint32_t command_count) {
    for (std::uint32_t command_index = 0; command_index < command_count; ++command_index) {
      const auto x = static_cast<float>(command_index % 100U) * 8.0F;
      const auto y = static_cast<float>(command_index / 100U) * 8.0F;
      list.VtxBuffer.push_back({{x, y}, {0.0F, 0.0F}, IM_COL32_WHITE});
      list.VtxBuffer.push_back({{x + 6.0F, y}, {1.0F, 0.0F}, IM_COL32_WHITE});
      list.VtxBuffer.push_back({{x + 6.0F, y + 6.0F}, {1.0F, 1.0F}, IM_COL32_WHITE});
      list.VtxBuffer.push_back({{x, y + 6.0F}, {0.0F, 1.0F}, IM_COL32_WHITE});
      for (const auto index :
           {ImDrawIdx{0}, ImDrawIdx{1}, ImDrawIdx{2}, ImDrawIdx{0}, ImDrawIdx{2}, ImDrawIdx{3}})
        list.IdxBuffer.push_back(index);

      ImDrawCmd command;
      command.ClipRect = {0.0F, 0.0F, 800.0F, 800.0F};
      command.TexRef = ImTextureRef{1};
      command.VtxOffset = command_index * 4U;
      command.IdxOffset = command_index * 6U;
      command.ElemCount = 6;
      list.CmdBuffer.push_back(command);
    }

    data.Valid = true;
    data.CmdLists.push_back(&list);
    data.CmdListsCount = 1;
    data.DisplaySize = {800.0F, 800.0F};
    data.FramebufferScale = {1.0F, 1.0F};
  }
};

bool run_case(std::string_view name, std::uint32_t command_count, std::uint32_t iterations,
              granit::canvas_draw_list& canvas, const ImDrawData& data) {
  constexpr std::uint32_t warmup = 3;
  constexpr std::uint32_t sample_count = 20;
  std::uint64_t checksum = 0;
  const auto operation = [&] {
    if (canvas.clear() != granit::result::success ||
        granit::integration::imgui::append_draw_data(&data, canvas, resolve_texture) !=
            granit::result::success)
      return false;
    granit_canvas_draw_list_stats stats = GRANIT_CANVAS_DRAW_LIST_STATS_INIT;
    if (canvas.get_stats(stats) != granit::result::success)
      return false;
    checksum += stats.index_count + stats.batch_count;
    return true;
  };
  for (std::uint32_t sample = 0; sample < warmup; ++sample) {
    for (std::uint32_t iteration = 0; iteration < iterations; ++iteration) {
      if (!operation())
        return false;
    }
  }
  std::vector<double> samples;
  samples.reserve(sample_count);
  for (std::uint32_t sample = 0; sample < sample_count; ++sample) {
    const auto begin = clock_type::now();
    for (std::uint32_t iteration = 0; iteration < iterations; ++iteration) {
      if (!operation())
        return false;
    }
    const auto elapsed =
        std::chrono::duration<double, std::nano>(clock_type::now() - begin).count();
    samples.push_back(elapsed / iterations);
  }
  const auto mean = std::accumulate(samples.begin(), samples.end(), 0.0) / samples.size();
  std::cout << "1," << name << ',' << command_count << ',' << iterations << ',' << sample_count
            << ',' << mean << ',' << percentile(samples, 0.50) << ',' << percentile(samples, 0.95)
            << ',' << percentile(samples, 0.99) << ',' << checksum << '\n';
  return true;
}

} // namespace

int main() {
  std::cout << "# revision=" << GRANIT_BENCHMARK_REVISION
            << ",compiler=" << GRANIT_BENCHMARK_COMPILER << ",system=" << GRANIT_BENCHMARK_SYSTEM
            << ",link=" << GRANIT_BENCHMARK_LINK_MODE << '\n';
  std::cout << "schema,name,commands,iterations,samples,mean_ns,p50_ns,p95_ns,p99_ns,checksum\n";

  granit::renderer renderer;
  if (renderer.initialize({.application_name = "granit-imgui-benchmark"}) !=
      granit::result::success) {
    std::cerr << "无法创建 Vulkan Renderer\n";
    return 1;
  }
  granit::canvas_draw_list canvas;
  granit_canvas_draw_list_desc desc = GRANIT_CANVAS_DRAW_LIST_DESC_INIT;
  if (canvas.initialize(renderer.native_handle(), desc) != granit::result::success) {
    std::cerr << "无法创建 Canvas Draw List\n";
    return 1;
  }

  bool succeeded = true;
  for (const auto command_count : {10U, 100U, 1'000U}) {
    draw_fixture fixture(command_count);
    const auto iterations = std::max(1U, 10'000U / command_count);
    succeeded &= run_case("append_draw_data", command_count, iterations, canvas, fixture.data);
  }
  return succeeded ? 0 : 1;
}
