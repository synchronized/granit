// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_BACKEND_QUEUE_H_
#define GRANIT_BACKEND_QUEUE_H_

#include <span>

#include <granit/core/result.h>

#include "backend/resources.h"
#include "core/retirement_queue.h"

namespace granit::detail {

/** 批量提交后端命令，并以统一序列表达完成与延迟销毁边界。 */
class backend_queue {
public:
  backend_queue() = default;
  virtual ~backend_queue() = default;
  backend_queue(const backend_queue&) = delete;
  backend_queue& operator=(const backend_queue&) = delete;
  backend_queue(backend_queue&&) = delete;
  backend_queue& operator=(backend_queue&&) = delete;

  [[nodiscard]] virtual granit_result
  submit_command_recorder(backend_command_recorder_resource& recorder,
                          submission_serial& submitted_serial) = 0;
  [[nodiscard]] virtual granit_result
  submit_command_recorders(std::span<backend_command_recorder_resource* const> recorders,
                           submission_serial& submitted_serial) = 0;
  [[nodiscard]] virtual granit_result
  wait_command_recorder(backend_command_recorder_resource& recorder) noexcept = 0;
  [[nodiscard]] virtual granit_result wait_for_all_submissions() noexcept = 0;
};

} // namespace granit::detail

#endif
