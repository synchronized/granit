// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "assets/asset_group.h"

#include <mutex>
#include <new>
#include <utility>
#include <vector>

namespace granit::example::assets {

struct asset_group::implementation {
  struct observation {
    std::shared_ptr<detail::asset_state> state;
    std::shared_ptr<detail::asset_observer> observer;
  };

  mutable std::mutex mutex;
  std::vector<observation> observations;
};

asset_group::asset_group(asset_manager& manager) noexcept : manager_(&manager) {
  try {
    implementation_ = std::make_shared<implementation>();
  } catch (...) {
    manager_ = nullptr;
  }
}

void asset_group::observe(const std::shared_ptr<detail::asset_state>& state,
                          const std::shared_ptr<detail::asset_observer>& observer) noexcept {
  if (!implementation_ || !state || !observer)
    return;
  try {
    std::scoped_lock lock{implementation_->mutex};
    implementation_->observations.push_back({state, observer});
  } catch (...) {
    observer->cancelled.store(true, std::memory_order_release);
  }
}

asset_group_progress asset_group::progress() const noexcept {
  asset_group_progress output;
  if (!implementation_)
    return output;

  std::scoped_lock group_lock{implementation_->mutex};
  output.total_assets = static_cast<std::uint32_t>(implementation_->observations.size());
  bool has_unknown_bytes{};
  bool has_unknown_dependencies{};
  bool has_unknown_fraction{};
  float fraction_sum{};
  for (const auto& observation : implementation_->observations) {
    if (observation.observer->cancelled.load(std::memory_order_acquire)) {
      ++output.cancelled_assets;
      ++output.completed_assets;
      has_unknown_fraction = true;
      continue;
    }
    const auto status = observation.state->status.load(std::memory_order_acquire);
    if (status == asset_status::ready || status == asset_status::failed ||
        status == asset_status::cancelled)
      ++output.completed_assets;
    if (status == asset_status::failed)
      ++output.failed_assets;
    if (status == asset_status::cancelled)
      ++output.cancelled_assets;

    std::scoped_lock state_lock{observation.state->mutex};
    const auto& progress = observation.state->progress;
    output.completed_bytes += progress.completed_bytes;
    if (progress.total_bytes)
      output.total_bytes = output.total_bytes.value_or(0) + *progress.total_bytes;
    else
      has_unknown_bytes = true;
    output.completed_dependencies += progress.completed_dependencies;
    if (progress.total_dependencies) {
      output.total_dependencies =
          output.total_dependencies.value_or(0) + *progress.total_dependencies;
    } else {
      has_unknown_dependencies = true;
    }
    if (progress.fraction)
      fraction_sum += *progress.fraction;
    else
      has_unknown_fraction = true;
  }
  if (has_unknown_bytes)
    output.total_bytes.reset();
  if (has_unknown_dependencies)
    output.total_dependencies.reset();
  if (!has_unknown_fraction && output.total_assets != 0)
    output.fraction = fraction_sum / static_cast<float>(output.total_assets);
  return output;
}

bool asset_group::complete() const noexcept {
  const auto snapshot = progress();
  return snapshot.total_assets != 0 && snapshot.completed_assets == snapshot.total_assets;
}

void asset_group::cancel() noexcept {
  if (!implementation_)
    return;
  std::scoped_lock lock{implementation_->mutex};
  for (const auto& observation : implementation_->observations)
    observation.observer->cancelled.store(true, std::memory_order_release);
}

} // namespace granit::example::assets
