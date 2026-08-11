// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_MATERIAL_MATERIAL_GPU_INSTANCE_H
#define GRANIT_MATERIAL_MATERIAL_GPU_INSTANCE_H

#include <granit/core/result.h>
#include <granit/renderer/buffer.h>
#include <granit/renderer/pipeline.h>

#include <memory>
#include <vector>

#include "material/material_metadata.h"
#include "material/material_migration.h"

namespace granit::material {

class material_gpu_instance {
public:
  material_gpu_instance() = default;
  ~material_gpu_instance();
  material_gpu_instance(const material_gpu_instance&) = delete;
  material_gpu_instance& operator=(const material_gpu_instance&) = delete;

  [[nodiscard]] granit_result initialize(granit_renderer renderer, granit_bind_group_layout layout,
                                         const material_metadata& metadata);
  [[nodiscard]] granit_result reset() noexcept;
  [[nodiscard]] metadata_error set(parameter_id id, parameter_type type,
                                   std::span<const std::byte> value);
  [[nodiscard]] metadata_error set_resource(parameter_id id, parameter_type type,
                                            granit_handle resource);
  [[nodiscard]] granit_result flush();
  [[nodiscard]] granit_result prepare_migration(granit_bind_group_layout target_layout,
                                                const material_metadata& target_metadata,
                                                material_gpu_instance& target,
                                                migration_report& report) const;
  void swap(material_gpu_instance& other) noexcept;

  [[nodiscard]] granit_bind_group bind_group() const noexcept { return bind_group_; }
  [[nodiscard]] granit_buffer uniform_buffer() const noexcept { return uniform_buffer_; }
  [[nodiscard]] const material_instance_data* data() const noexcept { return data_.get(); }
  [[nodiscard]] bool initialized() const noexcept { return renderer_ != GRANIT_NULL_HANDLE; }

private:
  struct resource_binding {
    parameter_id id = 0;
    parameter_type type = parameter_type::texture_view;
    std::uint32_t binding = 0;
    granit_handle resource = GRANIT_NULL_HANDLE;
  };

  granit_renderer renderer_ = GRANIT_NULL_HANDLE;
  granit_bind_group_layout layout_ = GRANIT_NULL_HANDLE;
  granit_buffer uniform_buffer_ = GRANIT_NULL_HANDLE;
  granit_bind_group bind_group_ = GRANIT_NULL_HANDLE;
  std::unique_ptr<material_instance_data> data_;
  std::vector<resource_binding> resources_;
  bool resources_dirty_ = true;
};

} // namespace granit::material

#endif
