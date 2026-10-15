// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_TUTORIAL_05_MODEL_DATA_HPP_
#define GRANIT_TUTORIAL_05_MODEL_DATA_HPP_

#include "geometry/primitives.hpp"

namespace tutorial_model {

using vertex = granit::example::geometry::textured_vertex;
inline constexpr auto& vertices = granit::example::geometry::textured_cube_vertices;
inline constexpr auto& indices = granit::example::geometry::textured_cube_indices;

} // namespace tutorial_model

#endif
