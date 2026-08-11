// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_MATERIAL_MATERIAL_MIGRATION_H
#define GRANIT_MATERIAL_MATERIAL_MIGRATION_H

#include "material/material_metadata.h"

#include <cstdint>
#include <memory>
#include <vector>

namespace granit::material {

enum class migration_issue_reason : std::uint8_t {
  missing_source_parameter,
  type_mismatch,
  array_mismatch,
  source_resource_unset,
};

struct migration_issue {
  parameter_id id = 0;
  migration_issue_reason reason = migration_issue_reason::missing_source_parameter;
};

struct migration_report {
  std::uint32_t copied_constant_parameters = 0;
  std::uint32_t defaulted_constant_parameters = 0;
  std::uint32_t pending_resource_parameters = 0;
  std::uint32_t copied_resource_parameters = 0;
  std::uint32_t defaulted_resource_parameters = 0;
  std::vector<migration_issue> issues;
};

enum class migration_error : std::uint8_t {
  none,
  invalid_source,
  out_of_memory,
  internal_error,
};

[[nodiscard]] migration_error migrate_material_instance_data(
    const material_metadata& source_metadata, const material_instance_data& source,
    const material_metadata& target_metadata, std::unique_ptr<material_instance_data>& target,
    migration_report& report) noexcept;

} // namespace granit::material

#endif
