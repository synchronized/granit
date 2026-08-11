// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_MATERIAL_MATERIAL_DEBUG_JSON_H
#define GRANIT_MATERIAL_MATERIAL_DEBUG_JSON_H

#include "material/material_archive.h"

#include <span>
#include <string>

namespace granit::material {

[[nodiscard]] archive_error export_material_archive_debug_json(std::span<const std::byte> bytes,
                                                               std::string& json) noexcept;

} // namespace granit::material

#endif
