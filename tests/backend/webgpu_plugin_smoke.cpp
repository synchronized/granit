// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <cstddef>
#include <cstdio>
#include <new>

#include "backend/plugin_loader.h"

namespace {

void* allocate(uint64_t size, uint64_t alignment, void*) {
  return ::operator new(static_cast<std::size_t>(size),
                        std::align_val_t{static_cast<std::size_t>(alignment)}, std::nothrow);
}

void deallocate(void* memory, uint64_t, uint64_t alignment, void*) {
  ::operator delete(memory, std::align_val_t{static_cast<std::size_t>(alignment)});
}

void diagnose(granit_diagnostic_severity severity, granit_diagnostic_category category,
              const char* message, uint32_t message_length, void*) {
  std::fprintf(stderr, "WebGPU 诊断 severity=%u category=%u: %.*s\n",
               static_cast<unsigned>(severity), static_cast<unsigned>(category),
               static_cast<int>(message_length), message == nullptr ? "" : message);
}

} // namespace

int main() {
  granit::detail::backend_plugin_loader loader;
  const auto open_result =
      loader.open(GRANIT_WEBGPU_PLUGIN_PATH, GRANIT_BACKEND_PLUGIN_KIND_WEBGPU);
  if (open_result != GRANIT_SUCCESS) {
    std::fprintf(stderr, "加载 WebGPU 插件失败：%d\n", static_cast<int>(open_result));
    return 1;
  }

  granit_backend_plugin_host_api host{
      sizeof(granit_backend_plugin_host_api), 0, diagnose, nullptr, allocate, deallocate, nullptr};
  granit_backend_plugin_instance instance{};
  const auto create_result = loader.create_instance(&host, &instance);
  if (create_result != GRANIT_SUCCESS || instance == 0) {
    std::fprintf(stderr, "创建 WebGPU 插件实例失败：%d，instance=%llu\n",
                 static_cast<int>(create_result), static_cast<unsigned long long>(instance));
    return 2;
  }
  const auto destroy_result = loader.destroy_instance(instance);
  if (destroy_result != GRANIT_SUCCESS) {
    std::fprintf(stderr, "销毁 WebGPU 插件实例失败：%d\n", static_cast<int>(destroy_result));
    return 3;
  }
  return 0;
}
