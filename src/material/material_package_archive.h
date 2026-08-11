// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_MATERIAL_MATERIAL_PACKAGE_ARCHIVE_H
#define GRANIT_MATERIAL_MATERIAL_PACKAGE_ARCHIVE_H

#include "material/material_archive.h"
#include "material/material_package.h"

#include <vector>

namespace granit::material {

[[nodiscard]] archive_error encode_material_package_archive(const material_package& package,
                                                            std::vector<std::byte>& bytes) noexcept;

} // namespace granit::material

#endif
