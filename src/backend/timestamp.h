// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_BACKEND_TIMESTAMP_H_
#define GRANIT_BACKEND_TIMESTAMP_H_

#include <cstdint>
#include <memory>
#include <span>
#include <string_view>

#include <granit/core/result.h>
#include <granit/renderer/timestamp_query.h>

#include "backend/resources.h"

namespace granit::detail {

/** 统一时间戳查询池及命令记录能力。 */
class backend_timestamp_renderer {
public:
  backend_timestamp_renderer() = default;
  virtual ~backend_timestamp_renderer() = default;
  backend_timestamp_renderer(const backend_timestamp_renderer&) = delete;
  backend_timestamp_renderer& operator=(const backend_timestamp_renderer&) = delete;

  [[nodiscard]] virtual granit_result create_timestamp_query_pool(
      std::uint32_t query_count,
      std::unique_ptr<backend_timestamp_query_pool_resource>& pool) noexcept = 0;
  [[nodiscard]] virtual granit_result
  read_timestamp_query_results(backend_timestamp_query_pool_resource& pool, std::uint32_t first,
                               std::span<std::uint64_t> values) noexcept = 0;
  [[nodiscard]] virtual granit_result
  reset_timestamp_queries(backend_command_recorder_resource& recorder,
                          backend_timestamp_query_pool_resource& pool, std::uint32_t first,
                          std::uint32_t count) noexcept = 0;
  [[nodiscard]] virtual granit_result write_timestamp(backend_command_recorder_resource& recorder,
                                                      backend_timestamp_query_pool_resource& pool,
                                                      granit_timestamp_stage stage,
                                                      std::uint32_t index) noexcept = 0;
  [[nodiscard]] virtual granit_result
  set_timestamp_query_pool_name(backend_timestamp_query_pool_resource& pool,
                                std::string_view name) noexcept = 0;
};

} // namespace granit::detail

#endif
