// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_INTEGRATIONS_SDL3_SURFACE_HPP_
#define GRANIT_INTEGRATIONS_SDL3_SURFACE_HPP_

#include <SDL3/SDL_video.h>

#include <granit/integrations/sdl3/export.hpp>
#include <granit/renderer/renderer.hpp>
#include <granit/renderer/surface.hpp>

namespace granit::integration::sdl3 {

/** 借用 SDL Window 的原生值创建 Granit Surface；调用方继续拥有 SDL Window。 */
[[nodiscard]] GRANIT_INTEGRATION_SDL3_API result create_surface(renderer& owner,
                                                                SDL_Window* window,
                                                                surface& output) noexcept;

} // namespace granit::integration::sdl3

#endif
