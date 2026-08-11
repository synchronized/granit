// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "material/material_hot_reload.h"

#include <new>
#include <utility>

namespace granit::material {

granit_result
material_runtime_template::create(granit_renderer renderer, material_package package,
                                  std::shared_ptr<material_runtime_template>& runtime_template) {
  if (renderer == GRANIT_NULL_HANDLE) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  try {
    auto candidate = std::shared_ptr<material_runtime_template>(
        new material_runtime_template(std::move(package)));
    const auto result = candidate->gpu_.initialize(renderer, candidate->package_);
    if (result != GRANIT_SUCCESS) {
      return result;
    }
    runtime_template = std::move(candidate);
    return GRANIT_SUCCESS;
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

material_hot_reload_slot::material_hot_reload_slot(
    std::shared_ptr<material_runtime_template> fallback)
    : fallback_(std::move(fallback)) {}

material_reload_result material_hot_reload_slot::reload(granit_renderer renderer,
                                                        material_package package) {
  std::shared_ptr<material_runtime_template> candidate;
  const auto result = material_runtime_template::create(renderer, std::move(package), candidate);
  std::lock_guard lock{mutex_};
  if (result != GRANIT_SUCCESS) {
    return {.result = result,
            .outcome = active_ == nullptr ? material_reload_outcome::using_fallback
                                          : material_reload_outcome::retained_previous,
            .generation = generation_};
  }
  active_ = std::move(candidate);
  ++generation_;
  return {.result = GRANIT_SUCCESS,
          .outcome = material_reload_outcome::replaced,
          .generation = generation_};
}

std::shared_ptr<material_runtime_template> material_hot_reload_slot::snapshot() const {
  std::lock_guard lock{mutex_};
  return active_ == nullptr ? fallback_ : active_;
}

material_pipeline_resolution
material_hot_reload_slot::resolve_pipeline(const material_pipeline_request& request) const {
  std::shared_ptr<material_runtime_template> active;
  std::shared_ptr<material_runtime_template> fallback;
  std::uint64_t current_generation = 0;
  {
    std::lock_guard lock{mutex_};
    active = active_;
    fallback = fallback_;
    current_generation = generation_;
  }

  material_pipeline_resolution resolution;
  resolution.generation = current_generation;
  if (active != nullptr) {
    resolution.primary_result = active->gpu().acquire_pipeline(request, resolution.pipeline);
    if (resolution.primary_result == GRANIT_SUCCESS) {
      resolution.result = GRANIT_SUCCESS;
      resolution.keepalive = std::move(active);
      return resolution;
    }
  }
  if (fallback == nullptr || fallback == active) {
    resolution.result = resolution.primary_result;
    return resolution;
  }
  resolution.pipeline = GRANIT_NULL_HANDLE;
  resolution.result = fallback->gpu().acquire_pipeline(request, resolution.pipeline);
  if (resolution.result == GRANIT_SUCCESS) {
    resolution.used_fallback = true;
    resolution.keepalive = std::move(fallback);
  }
  return resolution;
}

std::uint64_t material_hot_reload_slot::generation() const {
  std::lock_guard lock{mutex_};
  return generation_;
}

} // namespace granit::material
