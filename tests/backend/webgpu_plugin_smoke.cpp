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
  granit_backend_plugin_capabilities capabilities{};
  capabilities.struct_size = sizeof(capabilities);
  const auto capabilities_result = loader.get_capabilities(instance, &capabilities);
  if (capabilities_result != GRANIT_SUCCESS || capabilities.max_buffer_size == 0 ||
      capabilities.max_texture_dimension_2d == 0 || capabilities.max_bind_groups == 0 ||
      capabilities.max_color_attachments == 0) {
    std::fprintf(stderr, "查询 WebGPU 能力失败：%d\n", static_cast<int>(capabilities_result));
    return 3;
  }

  granit_backend_plugin_buffer_desc buffer_desc{};
  buffer_desc.struct_size = sizeof(buffer_desc);
  buffer_desc.size = 16;
  buffer_desc.usage = GRANIT_BACKEND_PLUGIN_BUFFER_USAGE_MAP_READ_BIT |
                      GRANIT_BACKEND_PLUGIN_BUFFER_USAGE_COPY_DST_BIT;
  granit_backend_plugin_buffer buffer{};
  const std::uint32_t source[]{1, 2, 3, 4};
  std::uint32_t destination[4]{};
  if (loader.create_buffer(instance, &buffer_desc, &buffer) != GRANIT_SUCCESS || buffer == 0 ||
      loader.write_buffer(instance, buffer, 0, source, sizeof(source)) != GRANIT_SUCCESS ||
      loader.read_buffer(instance, buffer, 0, destination, sizeof(destination)) != GRANIT_SUCCESS ||
      destination[0] != source[0] || destination[1] != source[1] || destination[2] != source[2] ||
      destination[3] != source[3] || loader.destroy_buffer(instance, buffer) != GRANIT_SUCCESS) {
    std::fprintf(stderr, "WebGPU Buffer 写入或回读失败\n");
    return 4;
  }
  const auto destroy_result = loader.destroy_instance(instance);
  if (destroy_result != GRANIT_SUCCESS) {
    std::fprintf(stderr, "销毁 WebGPU 插件实例失败：%d\n", static_cast<int>(destroy_result));
    return 5;
  }
  return 0;
}
