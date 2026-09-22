// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/renderer/buffer.hpp>

#include <type_traits>

template <typename Object>
concept granit_nameable_object = requires(granit::renderer& renderer, const Object& object) {
  renderer.set_object_name(object, "object");
};

static_assert(!std::is_copy_constructible_v<granit::buffer>);
static_assert(std::is_move_constructible_v<granit::buffer>);
static_assert(std::is_trivially_copyable_v<granit::buffer_ref>);
static_assert(requires(granit::buffer& buffer) { buffer.flush(0, 1); });
static_assert(granit_nameable_object<granit::buffer>);
static_assert(granit_nameable_object<granit::buffer_ref>);
static_assert(!granit_nameable_object<granit_buffer>);
