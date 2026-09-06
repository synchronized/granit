// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/pipeline/pbr_material.hpp>

static_assert((granit::pbr_texture::base_color | granit::pbr_texture::normal) !=
              granit::pbr_texture::none);
