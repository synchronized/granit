// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_PIPELINE_MESH_ACCESS_H_
#define GRANIT_PIPELINE_MESH_ACCESS_H_

#include <granit/pipeline/mesh.h>

namespace granit::pipeline::detail {

[[nodiscard]] granit_result validate_mesh_handle(granit_renderer renderer,
                                                 granit_mesh mesh) noexcept;

} // namespace granit::pipeline::detail

#endif
