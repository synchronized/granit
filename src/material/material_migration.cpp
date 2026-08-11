// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "material/material_migration.h"

#include <algorithm>
#include <new>
#include <span>

namespace granit::material {
namespace {

std::uint64_t storage_size(const parameter_desc& parameter) noexcept {
  const auto element_size = parameter_element_size(parameter.type);
  if (element_size == 0 || parameter.array_count == 0) {
    return 0;
  }
  return parameter.array_count == 1
             ? element_size
             : static_cast<std::uint64_t>(parameter.array_stride) * (parameter.array_count - 1U) +
                   element_size;
}

} // namespace

migration_error migrate_material_instance_data(const material_metadata& source_metadata,
                                               const material_instance_data& source,
                                               const material_metadata& target_metadata,
                                               std::unique_ptr<material_instance_data>& target,
                                               migration_report& report) noexcept {
  if (&source.metadata() != &source_metadata ||
      source.bytes().size() != source_metadata.constant_buffer_size()) {
    return migration_error::invalid_source;
  }
  try {
    auto migrated = std::make_unique<material_instance_data>(target_metadata);
    migration_report migrated_report;
    for (const auto& target_parameter : target_metadata.parameters()) {
      if (is_resource_type(target_parameter.type)) {
        ++migrated_report.pending_resource_parameters;
        continue;
      }
      const auto* source_parameter = source_metadata.find(target_parameter.id);
      if (source_parameter == nullptr) {
        ++migrated_report.defaulted_constant_parameters;
        migrated_report.issues.push_back(
            {target_parameter.id, migration_issue_reason::missing_source_parameter});
        continue;
      }
      if (source_parameter->type != target_parameter.type ||
          is_resource_type(source_parameter->type)) {
        ++migrated_report.defaulted_constant_parameters;
        migrated_report.issues.push_back(
            {target_parameter.id, migration_issue_reason::type_mismatch});
        continue;
      }
      if (source_parameter->array_count != target_parameter.array_count) {
        ++migrated_report.defaulted_constant_parameters;
        migrated_report.issues.push_back(
            {target_parameter.id, migration_issue_reason::array_mismatch});
        continue;
      }

      const auto target_size = static_cast<std::size_t>(storage_size(target_parameter));
      std::vector<std::byte> value(target_size);
      const auto existing = migrated->bytes().subspan(target_parameter.offset, target_size);
      std::ranges::copy(existing, value.begin());
      const auto element_size = parameter_element_size(target_parameter.type);
      const auto source_stride =
          source_parameter->array_count == 1 ? element_size : source_parameter->array_stride;
      const auto target_stride =
          target_parameter.array_count == 1 ? element_size : target_parameter.array_stride;
      for (std::uint32_t index = 0; index < target_parameter.array_count; ++index) {
        const auto source_offset = static_cast<std::size_t>(source_parameter->offset) +
                                   static_cast<std::size_t>(index) * source_stride;
        const auto target_offset = static_cast<std::size_t>(index) * target_stride;
        std::ranges::copy(source.bytes().subspan(source_offset, element_size),
                          value.begin() + static_cast<std::ptrdiff_t>(target_offset));
      }
      if (migrated->set(target_parameter.id, target_parameter.type, value) !=
          metadata_error::none) {
        return migration_error::internal_error;
      }
      ++migrated_report.copied_constant_parameters;
    }
    target = std::move(migrated);
    report = std::move(migrated_report);
    return migration_error::none;
  } catch (const std::bad_alloc&) {
    return migration_error::out_of_memory;
  } catch (...) {
    return migration_error::internal_error;
  }
}

} // namespace granit::material
