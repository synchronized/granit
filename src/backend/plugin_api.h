// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_BACKEND_PLUGIN_API_H_
#define GRANIT_BACKEND_PLUGIN_API_H_

#include <stdint.h>

#include <granit/core/diagnostic.h>
#include <granit/core/result.h>

#define GRANIT_BACKEND_PLUGIN_ABI_VERSION UINT32_C(1)
#define GRANIT_BACKEND_PLUGIN_KIND_WEBGPU UINT32_C(1)
#define GRANIT_BACKEND_PLUGIN_QUERY_SYMBOL "granit_backend_plugin_query"

typedef uint32_t granit_backend_plugin_kind;
typedef uint64_t granit_backend_plugin_instance;

typedef void* (*granit_backend_plugin_allocate_fn)(uint64_t size, uint64_t alignment,
                                                   void* user_data);
typedef void (*granit_backend_plugin_deallocate_fn)(void* memory, uint64_t size, uint64_t alignment,
                                                    void* user_data);

/**
 * Host 服务及其 user_data 在所有插件实例销毁前保持有效。
 *
 * 回调可能由插件工作线程并发调用，不得向 ABI 外抛出异常。allocate 和 deallocate 必须成对提供；
 * 插件只能用 deallocate 释放同一 Host allocate 返回的内存。
 */
typedef struct granit_backend_plugin_host_api {
  uint32_t struct_size;
  uint32_t reserved;
  granit_diagnostic_callback diagnostic_callback;
  void* diagnostic_user_data;
  granit_backend_plugin_allocate_fn allocate;
  granit_backend_plugin_deallocate_fn deallocate;
  void* allocator_user_data;
} granit_backend_plugin_host_api;

typedef granit_result (*granit_backend_plugin_create_fn)(
    const granit_backend_plugin_host_api* host, granit_backend_plugin_instance* out_instance);
/** instance 非零且只允许销毁一次；销毁返回前插件不得继续使用 Host 服务。 */
typedef void (*granit_backend_plugin_destroy_fn)(granit_backend_plugin_instance instance);

/** 后端插件入口返回的只读描述；字符串在插件卸载前有效。 */
typedef struct granit_backend_plugin_api {
  uint32_t struct_size;
  uint32_t abi_version;
  granit_backend_plugin_kind kind;
  uint32_t reserved;
  const char* name;
  uint32_t name_length;
  granit_backend_plugin_create_fn create;
  granit_backend_plugin_destroy_fn destroy;
} granit_backend_plugin_api;

typedef const granit_backend_plugin_api* (*granit_backend_plugin_query_fn)(uint32_t requested_abi);

#endif
