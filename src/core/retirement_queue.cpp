// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "core/retirement_queue.h"

#include <limits>
#include <utility>

namespace granit::detail {

submission_serial submission_serials::next() const noexcept {
  if (last_submitted_ == std::numeric_limits<submission_serial>::max()) {
    return 0;
  }
  return last_submitted_ + 1;
}

bool submission_serials::commit(submission_serial serial) noexcept {
  if (serial == 0 || serial != next()) {
    return false;
  }
  last_submitted_ = serial;
  return true;
}

void submission_serials::mark_completed(submission_serial serial) noexcept {
  if (serial > completed_) {
    completed_ = serial < last_submitted_ ? serial : last_submitted_;
  }
}

void retirement_queue::retire(submission_serial retire_after, retirement_order order,
                              std::shared_ptr<void> resource) {
  if (!resource) {
    return;
  }
  const auto [position, inserted] = batches_.try_emplace(retire_after);
  try {
    if (order == retirement_order::dependent) {
      position->second.dependents.push_back(std::move(resource));
    } else {
      position->second.resources.push_back(std::move(resource));
    }
  } catch (...) {
    if (inserted) {
      batches_.erase(position);
    }
    throw;
  }
  ++size_;
}

std::size_t retirement_queue::collect(submission_serial completed_serial) noexcept {
  std::size_t collected{};
  auto batch = batches_.begin();
  while (batch != batches_.end() && batch->first <= completed_serial) {
    collected += batch->second.size();
    batch->second.dependents.clear();
    batch->second.resources.clear();
    batch = batches_.erase(batch);
  }
  size_ -= collected;
  return collected;
}

std::size_t retirement_queue::drain() noexcept {
  const auto count = size_;
  for (auto& entry : batches_) {
    entry.second.dependents.clear();
    entry.second.resources.clear();
  }
  batches_.clear();
  size_ = 0;
  return count;
}

submission_serial retirement_queue::earliest_serial() const noexcept {
  return batches_.empty() ? 0 : batches_.begin()->first;
}

} // namespace granit::detail
