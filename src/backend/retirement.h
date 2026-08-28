// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_BACKEND_RETIREMENT_H_
#define GRANIT_BACKEND_RETIREMENT_H_

#include <cstddef>
#include <memory>

#include "backend/queue.h"
#include "core/retirement_queue.h"

namespace granit::detail {

/** 提供提交序列驱动的后端资源延迟销毁能力。 */
class backend_retirement_renderer {
public:
  backend_retirement_renderer() = default;
  virtual ~backend_retirement_renderer() = default;
  backend_retirement_renderer(const backend_retirement_renderer&) = delete;
  backend_retirement_renderer& operator=(const backend_retirement_renderer&) = delete;

  virtual void retire_resource(submission_serial retire_after, retirement_order order,
                               std::shared_ptr<void> resource) = 0;
  [[nodiscard]] virtual std::size_t collect_retired() noexcept = 0;
};

} // namespace granit::detail

#endif
