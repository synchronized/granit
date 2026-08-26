// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_BACKEND_PLUGIN_API_H_
#define GRANIT_BACKEND_PLUGIN_API_H_

#include <stdint.h>

#define GRANIT_BACKEND_PLUGIN_ABI_VERSION UINT32_C(1)
#define GRANIT_BACKEND_PLUGIN_KIND_WEBGPU UINT32_C(1)
#define GRANIT_BACKEND_PLUGIN_QUERY_SYMBOL "granit_backend_plugin_query"

typedef uint32_t granit_backend_plugin_kind;

/** 后端插件入口返回的只读描述；字符串在插件卸载前有效。 */
typedef struct granit_backend_plugin_api {
  uint32_t struct_size;
  uint32_t abi_version;
  granit_backend_plugin_kind kind;
  uint32_t reserved;
  const char* name;
  uint32_t name_length;
} granit_backend_plugin_api;

typedef const granit_backend_plugin_api* (*granit_backend_plugin_query_fn)(uint32_t requested_abi);

#endif
