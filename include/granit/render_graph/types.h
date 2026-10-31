// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_RENDER_GRAPH_TYPES_H_
#define GRANIT_RENDER_GRAPH_TYPES_H_

#include <stddef.h>
#include <stdint.h>

#include <granit/core/result.h>
#include <granit/core/types.h>
#include <granit/renderer/buffer.h>
#include <granit/renderer/command_recorder.h>
#include <granit/renderer/texture.h>

typedef granit_handle granit_render_graph;
typedef uint32_t granit_render_graph_resource;
typedef uint32_t granit_render_graph_pass;

#define GRANIT_RENDER_GRAPH_INVALID_RESOURCE UINT32_MAX
#define GRANIT_RENDER_GRAPH_INVALID_PASS UINT32_MAX

typedef uint32_t granit_render_graph_access;
#define GRANIT_RENDER_GRAPH_ACCESS_READ_BIT (UINT32_C(1) << 0)
#define GRANIT_RENDER_GRAPH_ACCESS_WRITE_BIT (UINT32_C(1) << 1)

typedef uint32_t granit_render_graph_usage;
#define GRANIT_RENDER_GRAPH_USAGE_SAMPLED_BIT (UINT32_C(1) << 0)
#define GRANIT_RENDER_GRAPH_USAGE_STORAGE_BIT (UINT32_C(1) << 1)
#define GRANIT_RENDER_GRAPH_USAGE_COLOR_ATTACHMENT_BIT (UINT32_C(1) << 2)
#define GRANIT_RENDER_GRAPH_USAGE_DEPTH_ATTACHMENT_BIT (UINT32_C(1) << 3)
#define GRANIT_RENDER_GRAPH_USAGE_TRANSFER_SOURCE_BIT (UINT32_C(1) << 4)
#define GRANIT_RENDER_GRAPH_USAGE_TRANSFER_DESTINATION_BIT (UINT32_C(1) << 5)

typedef struct granit_render_graph_access_desc {
  uint32_t struct_size;
  granit_render_graph_resource resource;
  granit_render_graph_access access;
  granit_render_graph_usage usage;
  uint32_t reserved;
} granit_render_graph_access_desc;

#define GRANIT_RENDER_GRAPH_ACCESS_DESC_VERSION_1_SIZE UINT32_C(20)
#define GRANIT_RENDER_GRAPH_ACCESS_DESC_INIT                                                       \
  {GRANIT_RENDER_GRAPH_ACCESS_DESC_VERSION_1_SIZE, GRANIT_RENDER_GRAPH_INVALID_RESOURCE,           \
   UINT32_C(0), UINT32_C(0), UINT32_C(0)}

typedef struct granit_render_graph_desc {
  uint32_t struct_size;
  uint32_t flags;
  uint64_t reserved;
} granit_render_graph_desc;

#define GRANIT_RENDER_GRAPH_DESC_VERSION_1_SIZE UINT32_C(16)
#define GRANIT_RENDER_GRAPH_DESC_INIT {GRANIT_RENDER_GRAPH_DESC_VERSION_1_SIZE, UINT32_C(0), 0}

typedef struct granit_render_graph_pass_desc {
  uint32_t struct_size;
  uint32_t flags;
  const char* name;
  uint32_t name_length;
  const granit_render_graph_access_desc* accesses;
  uint32_t access_count;
  uint64_t reserved;
} granit_render_graph_pass_desc;

#define GRANIT_RENDER_GRAPH_PASS_SIDE_EFFECT_BIT (UINT32_C(1) << 0)
#define GRANIT_RENDER_GRAPH_PASS_DESC_VERSION_1_SIZE                                               \
  ((uint32_t)(offsetof(granit_render_graph_pass_desc, reserved) + sizeof(uint64_t)))
#define GRANIT_RENDER_GRAPH_PASS_DESC_INIT                                                         \
  {GRANIT_RENDER_GRAPH_PASS_DESC_VERSION_1_SIZE,                                                   \
   UINT32_C(0),                                                                                    \
   NULL,                                                                                           \
   UINT32_C(0),                                                                                    \
   NULL,                                                                                           \
   UINT32_C(0),                                                                                    \
   UINT64_C(0)}

typedef struct granit_render_graph_transient_texture_update {
  uint32_t struct_size;
  granit_render_graph_resource resource;
  granit_texture_desc desc;
  uint32_t reserved;
} granit_render_graph_transient_texture_update;

#define GRANIT_RENDER_GRAPH_TRANSIENT_TEXTURE_UPDATE_VERSION_1_SIZE                                \
  ((uint32_t)(offsetof(granit_render_graph_transient_texture_update, reserved) + sizeof(uint32_t)))
#define GRANIT_RENDER_GRAPH_TRANSIENT_TEXTURE_UPDATE_INIT                                          \
  {GRANIT_RENDER_GRAPH_TRANSIENT_TEXTURE_UPDATE_VERSION_1_SIZE,                                    \
   GRANIT_RENDER_GRAPH_INVALID_RESOURCE, GRANIT_TEXTURE_DESC_INIT, UINT32_C(0)}

typedef struct granit_render_graph_pass_context granit_render_graph_pass_context;
typedef granit_result (*granit_render_graph_record_callback)(
    const granit_render_graph_pass_context* context, void* user_data);

typedef struct granit_render_graph_execution_result {
  uint32_t struct_size;
  granit_result result;
  uint32_t phase;
  uint32_t graph_error;
  granit_render_graph_pass error_pass;
  granit_render_graph_resource error_resource;
  const char* error_pass_name;
  uint32_t error_pass_name_length;
  const char* error_resource_name;
  uint32_t error_resource_name_length;
  granit_command_recorder recorder;
  uint32_t reserved[2];
} granit_render_graph_execution_result;

#define GRANIT_RENDER_GRAPH_EXECUTION_PHASE_NONE UINT32_C(0)
#define GRANIT_RENDER_GRAPH_EXECUTION_PHASE_COMPILE UINT32_C(1)
#define GRANIT_RENDER_GRAPH_EXECUTION_PHASE_CREATE_RECORDER UINT32_C(2)
#define GRANIT_RENDER_GRAPH_EXECUTION_PHASE_BEGIN_RECORDER UINT32_C(3)
#define GRANIT_RENDER_GRAPH_EXECUTION_PHASE_RECORD_PASS UINT32_C(4)
#define GRANIT_RENDER_GRAPH_EXECUTION_PHASE_END_RECORDER UINT32_C(5)
#define GRANIT_RENDER_GRAPH_EXECUTION_PHASE_SUBMIT UINT32_C(6)
#define GRANIT_RENDER_GRAPH_EXECUTION_PHASE_CREATE_RESOURCES UINT32_C(7)
#define GRANIT_RENDER_GRAPH_EXECUTION_PHASE_DESTROY_RESOURCES UINT32_C(8)

#define GRANIT_RENDER_GRAPH_COMPILE_ERROR_NONE UINT32_C(0)
#define GRANIT_RENDER_GRAPH_COMPILE_ERROR_INVALID_RESOURCE UINT32_C(1)
#define GRANIT_RENDER_GRAPH_COMPILE_ERROR_INVALID_PASS UINT32_C(2)
#define GRANIT_RENDER_GRAPH_COMPILE_ERROR_DUPLICATE_ACCESS UINT32_C(3)
#define GRANIT_RENDER_GRAPH_COMPILE_ERROR_UNINITIALIZED_READ UINT32_C(4)
#define GRANIT_RENDER_GRAPH_COMPILE_ERROR_CYCLE UINT32_C(5)

#define GRANIT_RENDER_GRAPH_EXECUTION_RESULT_VERSION_1_SIZE UINT32_C(64)
#define GRANIT_RENDER_GRAPH_EXECUTION_RESULT_INIT                                                  \
  {                                                                                                \
    GRANIT_RENDER_GRAPH_EXECUTION_RESULT_VERSION_1_SIZE, GRANIT_SUCCESS, UINT32_C(0), UINT32_C(0), \
        GRANIT_RENDER_GRAPH_INVALID_PASS, GRANIT_RENDER_GRAPH_INVALID_RESOURCE, NULL, UINT32_C(0), \
        NULL, UINT32_C(0), GRANIT_NULL_HANDLE, {                                                   \
      UINT32_C(0), UINT32_C(0)                                                                     \
    }                                                                                              \
  }

#endif
