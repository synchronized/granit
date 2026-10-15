// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_EXAMPLES_TUTORIALS_10_TRANSPARENCY_MODEL_DATA_HPP_
#define GRANIT_EXAMPLES_TUTORIALS_10_TRANSPARENCY_MODEL_DATA_HPP_

#include "geometry/primitives.hpp"

namespace tutorial_transparency {
using vertex = granit::example::geometry::normal_vertex;
inline constexpr auto& vertices = granit::example::geometry::cube_vertices;
inline constexpr auto& indices = granit::example::geometry::cube_indices;
} // namespace tutorial_transparency

#endif
