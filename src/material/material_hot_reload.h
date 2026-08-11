// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_MATERIAL_MATERIAL_HOT_RELOAD_H
#define GRANIT_MATERIAL_MATERIAL_HOT_RELOAD_H

#include "material/material_package.h"
#include "material/material_template_gpu.h"

#include <cstdint>
#include <memory>
#include <mutex>

namespace granit::material {

class material_runtime_template {
public:
  [[nodiscard]] static granit_result
  create(granit_renderer renderer, material_package package,
         std::shared_ptr<material_runtime_template>& runtime_template);

  [[nodiscard]] const material_package& package() const noexcept { return package_; }
  [[nodiscard]] material_template_gpu& gpu() noexcept { return gpu_; }

private:
  explicit material_runtime_template(material_package package) : package_(std::move(package)) {}

  // GPU 模板借用 package，声明顺序保证 GPU 对象先销毁。
  material_package package_;
  material_template_gpu gpu_;
};

enum class material_reload_outcome : std::uint8_t {
  replaced,
  retained_previous,
  using_fallback,
};

struct material_reload_result {
  granit_result result = GRANIT_SUCCESS;
  material_reload_outcome outcome = material_reload_outcome::replaced;
  std::uint64_t generation = 0;
};

struct material_pipeline_resolution {
  granit_result result = GRANIT_ERROR_NOT_READY;
  granit_result primary_result = GRANIT_ERROR_NOT_READY;
  bool used_fallback = false;
  std::uint64_t generation = 0;
  granit_graphics_pipeline pipeline = GRANIT_NULL_HANDLE;
  std::shared_ptr<material_runtime_template> keepalive;
};

class material_hot_reload_slot {
public:
  explicit material_hot_reload_slot(std::shared_ptr<material_runtime_template> fallback = {});

  [[nodiscard]] material_reload_result reload(granit_renderer renderer, material_package package);
  [[nodiscard]] std::shared_ptr<material_runtime_template> snapshot() const;
  [[nodiscard]] material_pipeline_resolution
  resolve_pipeline(const material_pipeline_request& request) const;
  [[nodiscard]] std::uint64_t generation() const;

private:
  mutable std::mutex mutex_;
  std::shared_ptr<material_runtime_template> active_;
  std::shared_ptr<material_runtime_template> fallback_;
  std::uint64_t generation_ = 0;
};

} // namespace granit::material

#endif
