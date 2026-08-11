// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_MATERIAL_MATERIAL_SOURCE_JSON_H
#define GRANIT_MATERIAL_MATERIAL_SOURCE_JSON_H

#include "material/material_package.h"

#include <filesystem>
#include <string_view>

namespace granit::material {

enum class source_json_error : std::uint8_t {
  none,
  invalid_json,
  invalid_schema,
  unsupported_value,
  referenced_file_error,
  invalid_spirv,
  invalid_package,
};

[[nodiscard]] source_json_error
parse_material_source_json(std::string_view json, const std::filesystem::path& source_directory,
                           material_package& package) noexcept;

} // namespace granit::material

#endif
