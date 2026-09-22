// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "imgui/imgui_texture_registry.h"

#include <catch2/catch_all.hpp>

TEST_CASE("ImGui Texture Registry 映射存活资源", "[example][model-viewer][imgui][texture]") {
  granit::example::imgui::texture_registry registry;
  ImTextureID texture = ImTextureID_Invalid;
  REQUIRE(registry.register_texture(granit::texture_view_ref::from_native(11),
                                    granit::sampler_ref::from_native(22), texture) ==
          granit::result::success);
  REQUIRE(texture != ImTextureID_Invalid);
  granit_canvas_draw_state state{};
  REQUIRE(registry.resolve(texture, state) == granit::result::success);
  CHECK(state.texture == 11);
  CHECK(state.sampler == 22);
  CHECK(granit::example::imgui::texture_registry::resolver(texture, state, &registry) ==
        granit::result::success);
}

TEST_CASE("ImGui Texture Registry 拒绝未知与陈旧 ID", "[example][model-viewer][imgui][texture]") {
  granit::example::imgui::texture_registry registry;
  granit_canvas_draw_state state{};
  CHECK(registry.resolve(ImTextureID_Invalid, state) == granit::result::invalid_handle);
  CHECK(granit::example::imgui::texture_registry::resolver(7, state, nullptr) ==
        granit::result::invalid_argument);

  ImTextureID first = ImTextureID_Invalid;
  REQUIRE(registry.register_texture(granit::texture_view_ref::from_native(1),
                                    granit::sampler_ref::from_native(2), first) ==
          granit::result::success);
  REQUIRE(registry.unregister_texture(first) == granit::result::success);
  CHECK(registry.resolve(first, state) == granit::result::invalid_handle);

  ImTextureID replacement = ImTextureID_Invalid;
  REQUIRE(registry.register_texture(granit::texture_view_ref::from_native(3),
                                    granit::sampler_ref::from_native(4), replacement) ==
          granit::result::success);
  CHECK(replacement != first);
  CHECK(registry.resolve(first, state) == granit::result::invalid_handle);
  REQUIRE(registry.resolve(replacement, state) == granit::result::success);
  registry.clear();
  CHECK(registry.resolve(replacement, state) == granit::result::invalid_handle);
}
