// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "backend/plugin_api.h"

static granit_backend_plugin_api granit_test_plugin_api = {sizeof(granit_backend_plugin_api),
                                                           GRANIT_BACKEND_PLUGIN_ABI_VERSION,
                                                           GRANIT_BACKEND_PLUGIN_KIND_WEBGPU,
                                                           0,
                                                           "test",
                                                           UINT32_C(4),
                                                           0,
                                                           0};

const granit_backend_plugin_api* granit_test_backend_plugin_query(uint32_t requested_abi) {
  return requested_abi == GRANIT_BACKEND_PLUGIN_ABI_VERSION ? &granit_test_plugin_api : 0;
}
