// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "core/handle_table.h"

#include <algorithm>
#include <atomic>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace {

using clock_type = std::chrono::steady_clock;
using granit::detail::handle_table;
using granit::detail::resource_type;

struct options {
  std::string_view case_name{"all"};
  std::uint32_t threads{1};
  std::uint64_t iterations{1'000'000};
  std::uint32_t samples{30};
  std::uint32_t warmup{5};
  std::uint32_t table_size{10'000};
};

struct context {
  handle_table table;
  std::vector<std::uint64_t> resources;
  std::vector<granit_handle> handles;
  granit_handle stale_handle{};
};

enum class benchmark_case {
  find_hit,
  find_wrong_type,
  find_wrong_domain,
  find_stale,
  insert_erase
};

constexpr std::uint32_t domain = 7;
std::atomic_uint64_t checksum{};

void print_help() {
  std::cout << "用法：granit_benchmarks [选项]\n"
               "  --case <all|find_hit|find_wrong_type|find_wrong_domain|find_stale|insert_erase>\n"
               "  --threads <数量>       独立句柄表工作线程数\n"
               "  --iterations <数量>    每个线程、每个样本的操作数\n"
               "  --samples <数量>       正式样本数\n"
               "  --warmup <数量>        预热样本数\n"
               "  --table-size <数量>    查询场景的句柄表大小\n";
}

template <typename Value> bool parse_integer(std::string_view text, Value& value) {
  Value parsed{};
  const auto result = std::from_chars(text.data(), text.data() + text.size(), parsed);
  if (result.ec != std::errc{} || result.ptr != text.data() + text.size() || parsed == 0)
    return false;
  value = parsed;
  return true;
}

bool parse_options(int argc, char** argv, options& result) {
  for (int index = 1; index < argc; ++index) {
    const std::string_view argument{argv[index]};
    if (argument == "--help") {
      print_help();
      return false;
    }
    if (index + 1 >= argc) {
      std::cerr << "缺少选项值：" << argument << '\n';
      return false;
    }
    const std::string_view value{argv[++index]};
    if (argument == "--case")
      result.case_name = value;
    else if (argument == "--threads") {
      if (!parse_integer(value, result.threads))
        return false;
    } else if (argument == "--iterations") {
      if (!parse_integer(value, result.iterations))
        return false;
    } else if (argument == "--samples") {
      if (!parse_integer(value, result.samples))
        return false;
    } else if (argument == "--warmup") {
      if (!parse_integer(value, result.warmup))
        return false;
    } else if (argument == "--table-size") {
      if (!parse_integer(value, result.table_size))
        return false;
    } else {
      std::cerr << "未知选项：" << argument << '\n';
      return false;
    }
  }
  if (result.threads > std::max(1U, std::thread::hardware_concurrency())) {
    std::cerr << "线程数超过当前机器逻辑处理器数量\n";
    return false;
  }
  return true;
}

std::unique_ptr<context> make_context(std::uint32_t table_size, benchmark_case selected) {
  auto result = std::make_unique<context>();
  result->resources.resize(table_size);
  result->handles.reserve(table_size);
  for (std::uint32_t index = 0; index < table_size; ++index) {
    result->resources[index] = index + 1;
    result->handles.push_back(
        result->table.insert(&result->resources[index], resource_type::buffer, domain));
  }
  if (selected == benchmark_case::find_stale) {
    result->stale_handle = result->handles.back();
    static_cast<void>(result->table.erase(result->stale_handle, resource_type::buffer, domain));
    result->handles.pop_back();
  }
  return result;
}

std::uint64_t run_operations(context& state, benchmark_case selected, std::uint64_t iterations) {
  std::uint64_t local_checksum{};
  if (selected == benchmark_case::insert_erase) {
    std::uint64_t resource{1};
    for (std::uint64_t index = 0; index < iterations; ++index) {
      const auto handle = state.table.insert(&resource, resource_type::buffer, domain);
      local_checksum ^= handle;
      if (state.table.erase(handle, resource_type::buffer, domain) != GRANIT_SUCCESS)
        return 0;
    }
    return local_checksum;
  }

  const auto count = state.handles.size();
  for (std::uint64_t index = 0; index < iterations; ++index) {
    const auto handle = selected == benchmark_case::find_stale
                            ? state.stale_handle
                            : state.handles[static_cast<std::size_t>(index % count)];
    void* found{};
    switch (selected) {
    case benchmark_case::find_hit:
      found = state.table.find(handle, resource_type::buffer, domain);
      break;
    case benchmark_case::find_wrong_type:
      found = state.table.find(handle, resource_type::texture, domain);
      break;
    case benchmark_case::find_wrong_domain:
      found = state.table.find(handle, resource_type::buffer, domain + 1);
      break;
    case benchmark_case::find_stale:
      found = state.table.find(handle, resource_type::buffer, domain);
      break;
    case benchmark_case::insert_erase:
      break;
    }
    local_checksum ^= static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(found));
  }
  return local_checksum;
}

double run_sample(benchmark_case selected, const options& config) {
  std::vector<std::unique_ptr<context>> contexts;
  contexts.reserve(config.threads);
  for (std::uint32_t index = 0; index < config.threads; ++index)
    contexts.push_back(make_context(config.table_size, selected));

  std::atomic_uint32_t ready{};
  std::atomic_bool start{};
  std::vector<std::thread> workers;
  workers.reserve(config.threads);
  for (std::uint32_t index = 0; index < config.threads; ++index) {
    workers.emplace_back([&, index] {
      ready.fetch_add(1, std::memory_order_release);
      while (!start.load(std::memory_order_acquire))
        std::this_thread::yield();
      const auto value = run_operations(*contexts[index], selected, config.iterations);
      checksum.fetch_xor(value, std::memory_order_relaxed);
    });
  }
  while (ready.load(std::memory_order_acquire) != config.threads)
    std::this_thread::yield();
  const auto begin = clock_type::now();
  start.store(true, std::memory_order_release);
  for (auto& worker : workers)
    worker.join();
  const auto end = clock_type::now();
  return std::chrono::duration<double, std::nano>{end - begin}.count();
}

double percentile(const std::vector<double>& sorted, double fraction) {
  const auto rank = std::ceil(fraction * static_cast<double>(sorted.size()));
  const auto index = static_cast<std::size_t>(std::max(1.0, rank)) - 1;
  return sorted[index];
}

void run_case(std::string_view name, benchmark_case selected, const options& config) {
  for (std::uint32_t index = 0; index < config.warmup; ++index)
    static_cast<void>(run_sample(selected, config));
  std::vector<double> samples;
  samples.reserve(config.samples);
  double total_ns{};
  const auto operations =
      static_cast<double>(config.threads) * static_cast<double>(config.iterations);
  for (std::uint32_t index = 0; index < config.samples; ++index) {
    const auto elapsed = run_sample(selected, config);
    total_ns += elapsed;
    samples.push_back(elapsed / operations);
  }
  std::sort(samples.begin(), samples.end());
  const auto total_operations = operations * static_cast<double>(config.samples);
  const auto ns_per_operation = total_ns / total_operations;
  const auto operations_per_second = total_operations * 1'000'000'000.0 / total_ns;
  std::cout << "1," << name << ',' << config.threads << ',' << config.iterations << ','
            << config.samples << ',' << static_cast<std::uint64_t>(total_ns) << ','
            << ns_per_operation << ',' << percentile(samples, 0.50) << ','
            << percentile(samples, 0.95) << ',' << percentile(samples, 0.99) << ','
            << operations_per_second << '\n';
}

bool selected(std::string_view requested, std::string_view name) {
  return requested == "all" || requested == name;
}

std::string cpu_name() {
#ifdef _WIN32
  char* value{};
  std::size_t size{};
  if (_dupenv_s(&value, &size, "PROCESSOR_IDENTIFIER") != 0 || value == nullptr)
    return "unknown";
  std::string result{value};
  std::free(value);
  return result;
#else
  const auto* value = std::getenv("PROCESSOR_IDENTIFIER");
  return value == nullptr ? "unknown" : value;
#endif
}

} // namespace

int main(int argc, char** argv) {
  options config;
  if (!parse_options(argc, argv, config)) {
    for (int index = 1; index < argc; ++index) {
      if (std::string_view{argv[index]} == "--help")
        return 0;
    }
    return 2;
  }
  constexpr std::string_view cases[]{"find_hit", "find_wrong_type", "find_wrong_domain",
                                     "find_stale", "insert_erase"};
  if (config.case_name != "all" &&
      std::find(std::begin(cases), std::end(cases), config.case_name) == std::end(cases)) {
    std::cerr << "未知 benchmark 用例：" << config.case_name << '\n';
    return 2;
  }

  const auto cpu = cpu_name();
  std::cout << std::setprecision(10)
            << "# clock=steady_clock\n"
               "# build_type="
#ifdef NDEBUG
            << "Release\n"
#else
            << "Debug\n"
#endif
               "# hardware_concurrency="
            << std::thread::hardware_concurrency() << '\n'
            << "# compiler=" << GRANIT_BENCHMARK_COMPILER << '\n'
            << "# revision=" << GRANIT_BENCHMARK_REVISION << '\n'
            << "# link_mode=" << GRANIT_BENCHMARK_LINK_MODE << '\n'
            << "# system=" << GRANIT_BENCHMARK_SYSTEM << '\n'
            << "# cpu=" << cpu << '\n'
            << "schema,name,threads,iterations,samples,total_ns,ns_per_op,p50_ns,p95_ns,p99_ns,"
               "ops_per_second\n";
  if (selected(config.case_name, "find_hit"))
    run_case("find_hit", benchmark_case::find_hit, config);
  if (selected(config.case_name, "find_wrong_type"))
    run_case("find_wrong_type", benchmark_case::find_wrong_type, config);
  if (selected(config.case_name, "find_wrong_domain"))
    run_case("find_wrong_domain", benchmark_case::find_wrong_domain, config);
  if (selected(config.case_name, "find_stale"))
    run_case("find_stale", benchmark_case::find_stale, config);
  if (selected(config.case_name, "insert_erase"))
    run_case("insert_erase", benchmark_case::insert_erase, config);
  std::cerr << "checksum=" << checksum.load(std::memory_order_relaxed) << '\n';
  return 0;
}
