// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/renderer/render_target.hpp>

static_assert(sizeof(granit_rendering_desc) == GRANIT_RENDERING_DESC_VERSION_1_SIZE);

constexpr granit::color_attachment_desc color{
    .view = UINT64_C(7),
    .clear_value = {.red = 0.25F, .green = 0.5F, .blue = 0.75F, .alpha = 1.0F},
};
constexpr auto native_color = color.native();
static_assert(native_color.view == UINT64_C(7));
static_assert(native_color.load_operation == GRANIT_ATTACHMENT_LOAD_OPERATION_CLEAR);

constexpr granit::depth_stencil_attachment_desc depth{.view = UINT64_C(9)};
constexpr auto native_depth = depth.native();
static_assert(native_depth.clear_value.depth == 1.0F);
static_assert(native_depth.stencil_load_operation == GRANIT_ATTACHMENT_LOAD_OPERATION_DISCARD);
