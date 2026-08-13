// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_PIPELINE_MATERIAL_ACCESS_H_
#define GRANIT_PIPELINE_MATERIAL_ACCESS_H_

#include <granit/pipeline/material.h>

namespace granit::pipeline::detail {

[[nodiscard]] granit_result validate_material_handle(granit_renderer renderer,
                                                     granit_material material) noexcept;

} // namespace granit::pipeline::detail

#endif
