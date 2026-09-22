// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/renderer/pipeline.hpp>

#include <type_traits>

static_assert(!std::is_copy_constructible_v<granit::pipeline_layout>);
static_assert(std::is_move_constructible_v<granit::pipeline_layout>);
static_assert(!std::is_copy_constructible_v<granit::bind_group_layout>);
static_assert(std::is_move_constructible_v<granit::bind_group_layout>);
static_assert(!std::is_copy_constructible_v<granit::bind_group>);
static_assert(std::is_move_constructible_v<granit::bind_group>);
static_assert(!std::is_copy_constructible_v<granit::graphics_pipeline>);
static_assert(std::is_move_constructible_v<granit::graphics_pipeline>);
static_assert(!std::is_copy_constructible_v<granit::compute_pipeline>);
static_assert(std::is_move_constructible_v<granit::compute_pipeline>);
static_assert(std::is_trivially_copyable_v<granit::pipeline_layout_ref>);
static_assert(std::is_trivially_copyable_v<granit::binding_resource_ref>);
static_assert(std::is_constructible_v<granit::binding_resource_ref, granit::buffer_ref>);
static_assert(std::is_constructible_v<granit::binding_resource_ref, granit::texture_view_ref>);
static_assert(std::is_constructible_v<granit::binding_resource_ref, granit::sampler_ref>);
static_assert(!std::is_constructible_v<granit::binding_resource_ref, granit::shader_ref>);
static_assert(std::is_trivially_copyable_v<granit::bind_group_layout_ref>);
static_assert(std::is_trivially_copyable_v<granit::bind_group_ref>);
static_assert(!std::is_convertible_v<granit::shader_ref, granit::pipeline_layout_ref>);
static_assert(granit::pipeline_layout_ref::from_native(UINT64_C(3)).native_handle() == UINT64_C(3));
