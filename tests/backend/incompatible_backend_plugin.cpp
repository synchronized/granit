// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "backend/plugin_api.h"

#if defined(_WIN32)
#define GRANIT_TEST_PLUGIN_EXPORT __declspec(dllexport)
#else
#define GRANIT_TEST_PLUGIN_EXPORT __attribute__((visibility("default")))
#endif

namespace {

constexpr char plugin_name[] = "不兼容插件";
constexpr granit_backend_plugin_api plugin_api{sizeof(granit_backend_plugin_api),
                                               GRANIT_BACKEND_PLUGIN_ABI_VERSION + 1,
                                               GRANIT_BACKEND_PLUGIN_KIND_WEBGPU,
                                               0,
                                               plugin_name,
                                               sizeof(plugin_name) - 1,
                                               nullptr,
                                               nullptr,
                                               nullptr};

} // namespace

extern "C" GRANIT_TEST_PLUGIN_EXPORT const granit_backend_plugin_api*
granit_backend_plugin_query(uint32_t) {
  return &plugin_api;
}
