// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "render_graph/graph_compiler.h"
#include "render_graph/serial_graph.h"

#include <granit/renderer/renderer.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <numeric>
#include <string_view>
#include <vector>

namespace {

using clock_type = std::chrono::steady_clock;

struct options {
  std::string_view case_name{"all"};
  std::uint32_t pass_count{10};
  std::uint32_t iterations{100};
  std::uint32_t samples{20};
  std::uint32_t warmup{5};
};

bool parse_u32(const char* text, std::uint32_t& value) {
  char* end = nullptr;
  const auto parsed = std::strtoull(text, &end, 10);
  if (end == text || *end != '\0' || parsed == 0 || parsed > UINT32_MAX) {
    return false;
  }
  value = static_cast<std::uint32_t>(parsed);
  return true;
}

bool parse_options(int argc, char** argv, options& config) {
  for (int index = 1; index < argc; ++index) {
    const std::string_view argument{argv[index]};
    if (argument == "--help") {
      std::cout << "用法：granit_render_graph_benchmarks [--case 名称] [--passes N] "
                   "[--iterations N] [--samples N] [--warmup N]\n";
      return false;
    }
    if (index + 1 >= argc) {
      return false;
    }
    const char* value = argv[++index];
    if (argument == "--case") {
      config.case_name = value;
    } else if (argument == "--passes") {
      if (!parse_u32(value, config.pass_count))
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
  return true;
}

double percentile(std::vector<double> values, double fraction) {
  std::ranges::sort(values);
  const auto index = static_cast<std::size_t>(fraction * static_cast<double>(values.size() - 1));
  return values[index];
}

granit::render_graph::graph_compiler make_compiler(std::uint32_t pass_count) {
  granit::render_graph::graph_compiler graph;
  auto previous = graph.add_resource({.imported = true});
  for (std::uint32_t index = 0; index < pass_count; ++index) {
    const auto next = graph.add_resource({.exported = index + 1 == pass_count});
    static_cast<void>(
        graph.add_pass({.accesses = {{previous, granit::render_graph::access_type::read},
                                     {next, granit::render_graph::access_type::write}}}));
    previous = next;
  }
  return graph;
}

granit::render_graph::serial_graph make_imported_graph(std::uint32_t pass_count) {
  granit::render_graph::serial_graph graph;
  auto previous = graph.import_buffer(1);
  for (std::uint32_t index = 0; index < pass_count; ++index) {
    const auto next =
        graph.import_buffer(static_cast<granit_buffer>(index + 2), index + 1 == pass_count);
    static_cast<void>(
        graph.add_pass({.accesses = {{previous, granit::render_graph::access_type::read},
                                     {next, granit::render_graph::access_type::write}}},
                       [](granit::render_graph::pass_context&) { return GRANIT_SUCCESS; }));
    previous = next;
  }
  return graph;
}

granit::render_graph::serial_graph make_transient_graph(std::uint32_t pass_count) {
  granit::render_graph::serial_graph graph;
  granit_buffer_desc desc = GRANIT_BUFFER_DESC_INIT;
  desc.usage = GRANIT_BUFFER_USAGE_TRANSFER_DESTINATION_BIT;
  desc.size = 256;
  for (std::uint32_t index = 0; index < pass_count; ++index) {
    const auto buffer = graph.create_transient_buffer(desc);
    static_cast<void>(graph.add_pass(
        {.side_effect = true, .accesses = {{buffer, granit::render_graph::access_type::write}}},
        [buffer](granit::render_graph::pass_context& context) {
          return granit_command_recorder_fill_buffer(context.renderer(), context.recorder(),
                                                     context.buffer(buffer), 0, 256, 0);
        }));
  }
  return graph;
}

bool direct_execute(granit_renderer renderer) {
  granit_command_recorder recorder = GRANIT_NULL_HANDLE;
  const granit_command_recorder_desc desc = GRANIT_COMMAND_RECORDER_DESC_INIT;
  if (granit_command_recorder_create(renderer, &desc, &recorder) != GRANIT_SUCCESS ||
      granit_command_recorder_begin(renderer, recorder) != GRANIT_SUCCESS ||
      granit_command_recorder_end(renderer, recorder) != GRANIT_SUCCESS ||
      granit_command_recorder_submit(renderer, recorder) != GRANIT_SUCCESS) {
    return false;
  }
  return granit_command_recorder_destroy(renderer, recorder) == GRANIT_SUCCESS;
}

template <typename Operation>
bool run_case(std::string_view name, const options& config, Operation&& operation) {
  for (std::uint32_t index = 0; index < config.warmup; ++index) {
    if (!operation())
      return false;
  }
  std::vector<double> samples;
  samples.reserve(config.samples);
  for (std::uint32_t sample = 0; sample < config.samples; ++sample) {
    const auto begin = clock_type::now();
    for (std::uint32_t iteration = 0; iteration < config.iterations; ++iteration) {
      if (!operation())
        return false;
    }
    const auto elapsed =
        std::chrono::duration<double, std::nano>(clock_type::now() - begin).count();
    samples.push_back(elapsed / static_cast<double>(config.iterations));
  }
  const auto total = std::accumulate(samples.begin(), samples.end(), 0.0);
  const auto average = total / static_cast<double>(samples.size());
  std::cout << "1," << name << ',' << config.pass_count << ',' << config.iterations << ','
            << config.samples << ',' << average << ',' << percentile(samples, 0.50) << ','
            << percentile(samples, 0.95) << ',' << percentile(samples, 0.99) << '\n';
  return true;
}

bool selected(std::string_view requested, std::string_view name) {
  return requested == "all" || requested == name;
}

} // namespace

int main(int argc, char** argv) {
  options config;
  if (!parse_options(argc, argv, config)) {
    return 2;
  }
  std::cout << "# revision=" << GRANIT_BENCHMARK_REVISION
            << ",compiler=" << GRANIT_BENCHMARK_COMPILER << ",system=" << GRANIT_BENCHMARK_SYSTEM
            << ",link=" << GRANIT_BENCHMARK_LINK_MODE << '\n';
  std::cout << "schema,name,passes,iterations,samples,mean_ns,p50_ns,p95_ns,p99_ns\n";

  bool succeeded = true;
  if (selected(config.case_name, "graph_compile")) {
    auto graph = make_compiler(config.pass_count);
    succeeded &= run_case("graph_compile", config, [&] { return graph.compile().succeeded(); });
  }
  if (config.case_name == "graph_compile") {
    return succeeded ? 0 : 1;
  }

  granit_renderer_desc renderer_desc = GRANIT_RENDERER_DESC_INIT;
  granit_renderer renderer = GRANIT_NULL_HANDLE;
  if (granit_renderer_create(&renderer_desc, &renderer) != GRANIT_SUCCESS) {
    std::cerr << "无法创建 Vulkan Renderer\n";
    return 1;
  }
  if (selected(config.case_name, "direct_execute")) {
    succeeded &= run_case("direct_execute", config, [&] { return direct_execute(renderer); });
  }
  if (selected(config.case_name, "graph_execute")) {
    auto graph = make_imported_graph(config.pass_count);
    succeeded &= run_case("graph_execute", config, [&] {
      const auto result = graph.execute(renderer);
      return result.succeeded() &&
             granit_command_recorder_destroy(renderer, result.recorder) == GRANIT_SUCCESS;
    });
  }
  if (selected(config.case_name, "transient_execute")) {
    auto graph = make_transient_graph(config.pass_count);
    succeeded &= run_case("transient_execute", config, [&] {
      const auto result = graph.execute(renderer);
      return result.succeeded() &&
             granit_command_recorder_destroy(renderer, result.recorder) == GRANIT_SUCCESS;
    });
  }
  static_cast<void>(granit_renderer_destroy(renderer));
  return succeeded ? 0 : 1;
}
