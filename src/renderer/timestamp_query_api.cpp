// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/renderer/timestamp_query.h>

#include <span>

#include "renderer/renderer_registry.h"

extern "C" granit_result
granit_timestamp_query_pool_create(granit_renderer renderer,
                                   const granit_timestamp_query_pool_desc* desc,
                                   granit_timestamp_query_pool* pool) {
  if (renderer == GRANIT_NULL_HANDLE || desc == nullptr || pool == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  *pool = GRANIT_NULL_HANDLE;
  if (desc->struct_size < GRANIT_TIMESTAMP_QUERY_POOL_DESC_VERSION_1_SIZE ||
      desc->query_count < 2 || desc->reserved != 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  return granit::detail::renderer_registry::instance().create_timestamp_query_pool(
      renderer, desc->query_count, *pool);
}

extern "C" granit_result granit_timestamp_query_pool_get_results(granit_renderer renderer,
                                                                 granit_timestamp_query_pool pool,
                                                                 uint32_t first_query,
                                                                 uint32_t query_count,
                                                                 uint64_t* nanoseconds) {
  if (renderer == GRANIT_NULL_HANDLE || pool == GRANIT_NULL_HANDLE || query_count == 0 ||
      nanoseconds == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  return granit::detail::renderer_registry::instance().get_timestamp_query_results(
      renderer, pool, first_query, std::span{nanoseconds, query_count});
}

extern "C" granit_result granit_timestamp_query_pool_destroy(granit_renderer renderer,
                                                             granit_timestamp_query_pool pool) {
  if (renderer == GRANIT_NULL_HANDLE || pool == GRANIT_NULL_HANDLE)
    return GRANIT_ERROR_INVALID_HANDLE;
  return granit::detail::renderer_registry::instance().destroy_timestamp_query_pool(renderer, pool);
}

extern "C" granit_result granit_command_recorder_reset_timestamp_queries(
    granit_renderer renderer, granit_command_recorder recorder, granit_timestamp_query_pool pool,
    uint32_t first_query, uint32_t query_count) {
  if (renderer == GRANIT_NULL_HANDLE || recorder == GRANIT_NULL_HANDLE ||
      pool == GRANIT_NULL_HANDLE || query_count == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  return granit::detail::renderer_registry::instance().reset_timestamp_queries(
      renderer, recorder, pool, first_query, query_count);
}

extern "C" granit_result granit_command_recorder_write_timestamp(granit_renderer renderer,
                                                                 granit_command_recorder recorder,
                                                                 granit_timestamp_query_pool pool,
                                                                 granit_timestamp_stage stage,
                                                                 uint32_t query_index) {
  if (renderer == GRANIT_NULL_HANDLE || recorder == GRANIT_NULL_HANDLE ||
      pool == GRANIT_NULL_HANDLE)
    return GRANIT_ERROR_INVALID_HANDLE;
  return granit::detail::renderer_registry::instance().write_timestamp(renderer, recorder, pool,
                                                                       stage, query_index);
}
