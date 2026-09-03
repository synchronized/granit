// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "material/material_migration.h"
#include "material/material_package.h"
#include "material/material_template_gpu.h"

#include <granit/renderer/renderer.h>

#include <algorithm>
#include <array>
#include <bit>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <memory>
#include <numeric>
#include <string>
#include <string_view>
#include <vector>

namespace {

using clock_type = std::chrono::steady_clock;

struct options {
  std::string_view case_name{"all"};
  std::uint32_t iterations{100'000};
  std::uint32_t samples{20};
  std::uint32_t warmup{5};
  std::uint32_t variants{64};
};

bool parse_u32(std::string_view text, std::uint32_t& value) {
  std::uint32_t parsed = 0;
  const auto result = std::from_chars(text.data(), text.data() + text.size(), parsed);
  if (result.ec != std::errc{} || result.ptr != text.data() + text.size() || parsed == 0) {
    return false;
  }
  value = parsed;
  return true;
}

bool parse_options(int argc, char** argv, options& config) {
  for (int index = 1; index < argc; ++index) {
    const std::string_view argument{argv[index]};
    if (argument == "--help") {
      std::cout << "用法：granit_material_benchmarks [--case all|parameter_set|variant_lookup|"
                   "instance_migration|pipeline_cache_hit] [--iterations N] [--samples N] "
                   "[--warmup N] [--variants N]\n";
      return false;
    }
    if (index + 1 >= argc) {
      return false;
    }
    const std::string_view value{argv[++index]};
    if (argument == "--case") {
      config.case_name = value;
    } else if (argument == "--iterations") {
      if (!parse_u32(value, config.iterations))
        return false;
    } else if (argument == "--samples") {
      if (!parse_u32(value, config.samples))
        return false;
    } else if (argument == "--warmup") {
      if (!parse_u32(value, config.warmup))
        return false;
    } else if (argument == "--variants") {
      if (!parse_u32(value, config.variants))
        return false;
    } else {
      return false;
    }
  }
  return config.variants <= 4096;
}

double percentile(std::vector<double> values, double fraction) {
  std::ranges::sort(values);
  const auto index = static_cast<std::size_t>(fraction * static_cast<double>(values.size() - 1));
  return values[index];
}

template <typename Operation>
bool run_case(std::string_view name, const options& config, Operation&& operation) {
  std::uint64_t checksum = 0;
  for (std::uint32_t index = 0; index < config.warmup; ++index) {
    for (std::uint32_t iteration = 0; iteration < config.iterations; ++iteration) {
      if (!operation(iteration, checksum))
        return false;
    }
  }
  std::vector<double> samples;
  samples.reserve(config.samples);
  for (std::uint32_t sample = 0; sample < config.samples; ++sample) {
    const auto begin = clock_type::now();
    for (std::uint32_t iteration = 0; iteration < config.iterations; ++iteration) {
      if (!operation(iteration, checksum))
        return false;
    }
    const auto elapsed =
        std::chrono::duration<double, std::nano>(clock_type::now() - begin).count();
    samples.push_back(elapsed / static_cast<double>(config.iterations));
  }
  const auto mean =
      std::accumulate(samples.begin(), samples.end(), 0.0) / static_cast<double>(samples.size());
  std::cout << "1," << name << ',' << config.variants << ',' << config.iterations << ','
            << config.samples << ',' << mean << ',' << percentile(samples, 0.50) << ','
            << percentile(samples, 0.95) << ',' << percentile(samples, 0.99) << '\n';
  std::cerr << name << " checksum=" << checksum << '\n';
  return true;
}

bool selected(std::string_view requested, std::string_view name) {
  return requested == "all" || requested == name;
}

std::vector<std::uint32_t> load_shader(std::string_view name) {
  const auto path = std::string{GRANIT_BENCHMARK_ASSET_DIR} + "/" + std::string{name};
  std::ifstream stream{path, std::ios::binary};
  const std::vector<char> bytes{std::istreambuf_iterator<char>{stream}, {}};
  if (bytes.empty() || bytes.size() % sizeof(std::uint32_t) != 0)
    return {};
  std::vector<std::uint32_t> words(bytes.size() / sizeof(std::uint32_t));
  std::memcpy(words.data(), bytes.data(), bytes.size());
  return words;
}

std::string load_shader_text(std::string_view name) {
  const auto path = std::string{GRANIT_BENCHMARK_ASSET_DIR} + "/" + std::string{name};
  std::ifstream stream{path, std::ios::binary};
  return {std::istreambuf_iterator<char>{stream}, {}};
}

bool make_metadata(granit::material::material_metadata& metadata) {
  granit::material::metadata_desc desc;
  desc.constant_buffer_size = 16;
  desc.parameters = {{.name = "color",
                      .type = granit::material::parameter_type::float4,
                      .offset = 0,
                      .default_value = {}}};
  return granit::material::material_metadata::build(std::move(desc), metadata) ==
         granit::material::metadata_error::none;
}

bool make_package(std::uint32_t variant_count, granit::material::material_package& package) {
  using namespace granit::material;
  const auto vertex = load_shader("triangle.vert.spv");
  const auto fragment = load_shader("triangle.frag.spv");
  const auto vertex_wgsl = load_shader_text("triangle.vert.wgsl");
  const auto fragment_wgsl = load_shader_text("triangle.frag.wgsl");
  if (vertex.empty() || fragment.empty() || vertex_wgsl.empty() || fragment_wgsl.empty())
    return false;
  material_package_desc desc;
  desc.variants.reserve(variant_count);
  for (std::uint32_t value = 0; value < variant_count; ++value) {
    desc.variants.push_back(
        {.pass = make_feature_id("opaque"),
         .features = {{make_feature_id("mode"), value}},
         .shaders =
             {{.stage = package_shader_stage::vertex,
               .entry_point = "main",
               .spirv = vertex,
               .wgsl = vertex_wgsl},
              {.stage = package_shader_stage::fragment,
               .entry_point = "main",
               .spirv = fragment,
               .wgsl = fragment_wgsl}},
         .pipeline = {}});
  }
  return material_package::build(std::move(desc), package) == package_error::none;
}

} // namespace

int main(int argc, char** argv) {
  options config;
  if (!parse_options(argc, argv, config)) {
    return argc > 1 && std::string_view{argv[1]} == "--help" ? 0 : 2;
  }
  constexpr std::array cases{"parameter_set", "variant_lookup", "instance_migration",
                             "pipeline_cache_hit"};
  if (config.case_name != "all" && std::ranges::find(cases, config.case_name) == cases.end()) {
    std::cerr << "未知 benchmark 用例\n";
    return 2;
  }
  std::cout << std::setprecision(10) << "# revision=" << GRANIT_BENCHMARK_REVISION
            << ",compiler=" << GRANIT_BENCHMARK_COMPILER << ",system=" << GRANIT_BENCHMARK_SYSTEM
            << ",link=" << GRANIT_BENCHMARK_LINK_MODE << '\n'
            << "schema,name,variants,iterations,samples,mean_ns,p50_ns,p95_ns,p99_ns\n";

  granit::material::material_metadata metadata;
  granit::material::material_package package;
  if (!make_metadata(metadata) || !make_package(config.variants, package))
    return 1;
  granit::material::material_instance_data instance{metadata};
  bool succeeded = true;
  if (selected(config.case_name, "parameter_set")) {
    succeeded &=
        run_case("parameter_set", config, [&](std::uint32_t iteration, std::uint64_t& checksum) {
          const auto value = std::bit_cast<std::array<std::byte, 16>>(
              std::array<std::uint32_t, 4>{iteration, iteration + 1, iteration + 2, iteration + 3});
          const auto result = instance.set(granit::material::make_parameter_id("color"),
                                           granit::material::parameter_type::float4, value);
          checksum += std::to_integer<std::uint8_t>(instance.bytes().front());
          return result == granit::material::metadata_error::none;
        });
  }
  if (selected(config.case_name, "variant_lookup")) {
    succeeded &=
        run_case("variant_lookup", config, [&](std::uint32_t iteration, std::uint64_t& checksum) {
          const std::array feature_values{granit::material::material_feature_value{
              granit::material::make_feature_id("mode"), iteration % config.variants}};
          const auto* found = package.find(granit::material::make_feature_id("opaque"),
                                           granit::material::make_variant_key(feature_values));
          if (found != nullptr)
            checksum ^= found->key;
          return found != nullptr;
        });
  }
  if (selected(config.case_name, "instance_migration")) {
    succeeded &=
        run_case("instance_migration", config, [&](std::uint32_t, std::uint64_t& checksum) {
          std::unique_ptr<granit::material::material_instance_data> migrated;
          granit::material::migration_report report;
          const auto result = granit::material::migrate_material_instance_data(
              metadata, instance, metadata, migrated, report);
          checksum += report.copied_constant_parameters;
          return result == granit::material::migration_error::none;
        });
  }
  if (selected(config.case_name, "pipeline_cache_hit")) {
    granit_renderer renderer = GRANIT_NULL_HANDLE;
    const granit_renderer_desc renderer_desc = GRANIT_RENDERER_DESC_INIT;
    if (granit_renderer_create(&renderer_desc, &renderer) != GRANIT_SUCCESS)
      return 3;
    granit::material::material_template_gpu gpu_template;
    if (gpu_template.initialize(renderer, package) != GRANIT_SUCCESS) {
      static_cast<void>(granit_renderer_destroy(renderer));
      return 4;
    }
    const std::array feature_values{
        granit::material::material_feature_value{granit::material::make_feature_id("mode"), 0}};
    const granit::material::material_pipeline_request request{
        .pass = granit::material::make_feature_id("opaque"),
        .variant = granit::material::make_variant_key(feature_values),
        .color_format = GRANIT_TEXTURE_FORMAT_RGBA8_UNORM};
    granit_graphics_pipeline pipeline = GRANIT_NULL_HANDLE;
    if (gpu_template.acquire_pipeline(request, pipeline) != GRANIT_SUCCESS) {
      static_cast<void>(gpu_template.reset());
      static_cast<void>(granit_renderer_destroy(renderer));
      return 5;
    }
    succeeded &=
        run_case("pipeline_cache_hit", config, [&](std::uint32_t, std::uint64_t& checksum) {
          granit_graphics_pipeline cached = GRANIT_NULL_HANDLE;
          const auto result = gpu_template.acquire_pipeline(request, cached);
          checksum ^= cached;
          return result == GRANIT_SUCCESS && cached == pipeline;
        });
    static_cast<void>(gpu_template.reset());
    static_cast<void>(granit_renderer_destroy(renderer));
  }
  return succeeded ? 0 : 1;
}
