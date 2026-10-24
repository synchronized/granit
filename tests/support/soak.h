// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_TESTS_SUPPORT_SOAK_H_
#define GRANIT_TESTS_SUPPORT_SOAK_H_

#include <granit/granit.hpp>

#include <atomic>
#include <charconv>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string_view>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
// Windows SDK 的 psapi.h 依赖 windows.h 中的基础类型。
#include <psapi.h>
#else
#include <unistd.h>
#endif

namespace granit::tests {

inline volatile std::sig_atomic_t soak_interrupted = 0;
inline void interrupt_soak(int) { soak_interrupted = 1; }

inline std::uint32_t soak_number(const char* name, std::uint32_t fallback) {
#if defined(_WIN32)
  char storage[32]{};
  const auto length = GetEnvironmentVariableA(name, storage, sizeof(storage));
  if (length == 0)
    return fallback;
  if (length >= sizeof(storage))
    throw std::invalid_argument{name};
  const char* text = storage;
#else
  const char* text = std::getenv(name);
  if (text == nullptr)
    return fallback;
#endif
  const std::string_view value{text};
  std::uint32_t number{};
  const auto parsed = std::from_chars(value.data(), value.data() + value.size(), number);
  if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size())
    throw std::invalid_argument{name};
  return number;
}

inline std::uint64_t resident_bytes() {
#if defined(_WIN32)
  PROCESS_MEMORY_COUNTERS info{};
  if (GetProcessMemoryInfo(GetCurrentProcess(), &info, sizeof(info)))
    return info.WorkingSetSize;
#else
  std::ifstream stat{"/proc/self/statm"};
  std::uint64_t total{}, resident{};
  const auto page_size = sysconf(_SC_PAGESIZE);
  if (stat >> total >> resident && page_size > 0)
    return resident * static_cast<std::uint64_t>(page_size);
#endif
  return 0;
}

class soak_run {
public:
  soak_run()
      : seconds_{soak_number("GRANIT_STRESS_SECONDS", 3600)},
        seed_{soak_number("GRANIT_STRESS_SEED", 20261024)}, random_{seed_} {
    if (seconds_ == 0 || seconds_ > 86400)
      throw std::invalid_argument{"GRANIT_STRESS_SECONDS 必须在 1..86400"};
    std::signal(SIGINT, interrupt_soak);
    std::signal(SIGTERM, interrupt_soak);
    std::cout << "soak seed=" << seed_ << " seconds=" << seconds_ << std::endl;
  }

  [[nodiscard]] bool running() const {
    return soak_interrupted == 0 &&
           std::chrono::steady_clock::now() - started_ < std::chrono::seconds{seconds_};
  }

  void iteration() {
    ++iterations_;
    const auto now = std::chrono::steady_clock::now();
    if (now - last_sample_ >= std::chrono::seconds{10}) {
      sample();
      last_sample_ = now;
    }
  }

  ~soak_run() { sample(); }

  void sample() const {
    const auto elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - started_);
    std::cout << "soak elapsed=" << elapsed.count() << " iterations=" << iterations_
              << " seed=" << seed_ << " rss_bytes=" << resident_bytes()
              << " validation_errors=" << validation_errors.load()
              << " expected_diagnostics=" << expected_diagnostics.load()
              << " interrupted=" << soak_interrupted << std::endl;
  }

  void device(renderer& owner) {
    renderer_info info;
    if (owner.get_info(info).failed())
      throw std::runtime_error{"无法记录 GPU 身份"};
    std::cout << "soak adapter=" << info.adapter_name << " vendor=" << info.vendor_id
              << " device=" << info.device_id << " backend=" << static_cast<int>(info.backend)
              << " validation=requested;availability=see-diagnostics" << std::endl;
  }

  static void diagnostic(granit_diagnostic_severity severity, granit_diagnostic_category category,
                         const char* message, std::uint32_t length, void* data) noexcept {
    auto& run = *static_cast<soak_run*>(data);
    if (run.expect_invalid_buffer.load() &&
        std::string_view{message, length}.starts_with("granit_buffer_destroy:")) {
      ++run.expected_diagnostics;
      return;
    }
    if ((severity == GRANIT_DIAGNOSTIC_SEVERITY_ERROR &&
         category == GRANIT_DIAGNOSTIC_CATEGORY_VALIDATION) ||
        category == GRANIT_DIAGNOSTIC_CATEGORY_LIFECYCLE) {
      ++run.validation_errors;
      // 只保留首批错误，防止数小时运行耗尽磁盘。
      if (run.validation_errors.load() <= 16)
        std::cerr << "validation: " << std::string_view{message, length} << std::endl;
    }
  }

  [[nodiscard]] std::uint32_t random() { return static_cast<std::uint32_t>(random_()); }
  [[nodiscard]] std::uint64_t iterations() const { return iterations_; }
  std::atomic<std::uint32_t> validation_errors{};
  std::atomic<bool> expect_invalid_buffer{};
  std::atomic<std::uint64_t> expected_diagnostics{};

private:
  std::uint32_t seconds_;
  std::uint32_t seed_;
  std::mt19937 random_;
  std::uint64_t iterations_{};
  std::chrono::steady_clock::time_point started_{std::chrono::steady_clock::now()};
  std::chrono::steady_clock::time_point last_sample_{started_};
};

} // namespace granit::tests

#endif
