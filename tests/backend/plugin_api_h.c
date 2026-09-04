// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "backend/plugin/plugin_api.h"

static granit_backend_plugin_api granit_test_plugin_api = {sizeof(granit_backend_plugin_api),
                                                           GRANIT_BACKEND_PLUGIN_ABI_VERSION,
                                                           GRANIT_BACKEND_PLUGIN_KIND_WEBGPU,
                                                           0,
                                                           "test",
                                                           UINT32_C(4),
                                                           0,
                                                           0,
                                                           0};

static granit_backend_plugin_swapchain_desc granit_test_swapchain_desc = {
    sizeof(granit_backend_plugin_swapchain_desc), UINT32_C(640), UINT32_C(480), UINT32_C(2),
    GRANIT_BACKEND_PLUGIN_PRESENT_MODE_FIFO};

static granit_backend_plugin_acquired_frame granit_test_acquired_frame = {
    sizeof(granit_backend_plugin_acquired_frame), UINT32_C(0), UINT32_C(0), UINT32_C(0), 0, 0};

const granit_backend_plugin_api* granit_test_backend_plugin_query(uint32_t requested_abi) {
  (void)granit_test_swapchain_desc;
  (void)granit_test_acquired_frame;
  return requested_abi == GRANIT_BACKEND_PLUGIN_ABI_VERSION ? &granit_test_plugin_api : 0;
}
