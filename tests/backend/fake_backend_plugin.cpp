// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "backend/plugin_api.h"

#if defined(_WIN32)
#define GRANIT_TEST_PLUGIN_EXPORT __declspec(dllexport)
#else
#define GRANIT_TEST_PLUGIN_EXPORT __attribute__((visibility("default")))
#endif

namespace {

constexpr char plugin_name[] = "测试 WebGPU 插件";

struct plugin_state {
  granit_backend_plugin_host_api host{};
  void* allocation{};
  granit_backend_plugin_instance handle{};
};

plugin_state state;

granit_result create_plugin(const granit_backend_plugin_host_api* host,
                            granit_backend_plugin_instance* out_instance) {
  if (host == nullptr || out_instance == nullptr || state.handle != 0) {
    return GRANIT_ERROR_INITIALIZATION_FAILED;
  }
  state.allocation = host->allocate(32, 16, host->allocator_user_data);
  if (state.allocation == nullptr) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  }
  state.host = *host;
  state.handle = 1;
  *out_instance = state.handle;
  if (host->diagnostic_callback != nullptr) {
    constexpr char message[] = "测试插件实例已创建";
    host->diagnostic_callback(GRANIT_DIAGNOSTIC_SEVERITY_INFO, GRANIT_DIAGNOSTIC_CATEGORY_GENERAL,
                              message, sizeof(message) - 1, host->diagnostic_user_data);
  }
  return GRANIT_SUCCESS;
}

void destroy_plugin(granit_backend_plugin_instance instance) {
  if (instance != state.handle || instance == 0) {
    return;
  }
  const auto host = state.host;
  void* allocation = state.allocation;
  state = {};
  host.deallocate(allocation, 32, 16, host.allocator_user_data);
  if (host.diagnostic_callback != nullptr) {
    constexpr char message[] = "测试插件实例已销毁";
    host.diagnostic_callback(GRANIT_DIAGNOSTIC_SEVERITY_INFO, GRANIT_DIAGNOSTIC_CATEGORY_GENERAL,
                             message, sizeof(message) - 1, host.diagnostic_user_data);
  }
}

constexpr granit_backend_plugin_api plugin_api{sizeof(granit_backend_plugin_api),
                                               GRANIT_BACKEND_PLUGIN_ABI_VERSION,
                                               GRANIT_BACKEND_PLUGIN_KIND_WEBGPU,
                                               0,
                                               plugin_name,
                                               sizeof(plugin_name) - 1,
                                               create_plugin,
                                               destroy_plugin};

} // namespace

extern "C" GRANIT_TEST_PLUGIN_EXPORT const granit_backend_plugin_api*
granit_backend_plugin_query(uint32_t requested_abi) {
  return requested_abi == GRANIT_BACKEND_PLUGIN_ABI_VERSION ? &plugin_api : nullptr;
}
