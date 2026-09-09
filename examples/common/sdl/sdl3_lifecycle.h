// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_EXAMPLES_COMMON_SDL_SDL3_LIFECYCLE_H_
#define GRANIT_EXAMPLES_COMMON_SDL_SDL3_LIFECYCLE_H_

#include <SDL3/SDL.h>

#include <backends/imgui_impl_sdl3.h>
#include <imgui.h>

namespace granit::example::sdl {

/** RAII 负责在作用域结束时调用 SDL_Quit。 */
struct sdl_quit {
  ~sdl_quit() { SDL_Quit(); }
};

/** 配合 std::unique_ptr 在窗口销毁时释放 SDL_Window。 */
struct window_deleter {
  void operator()(SDL_Window* window) const noexcept { SDL_DestroyWindow(window); }
};

/** RAII 负责销毁 ImGui 上下文与 SDL3 后端。 */
struct imgui_quit {
  ~imgui_quit() {
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
  }
};

} // namespace granit::example::sdl

#endif
