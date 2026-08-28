// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_RENDERER_RENDERER_FACTORY_H_
#define GRANIT_RENDERER_RENDERER_FACTORY_H_

#include <granit/renderer/renderer.h>

namespace granit::detail {

[[nodiscard]] granit_result create_default_renderer(const granit_renderer_desc& desc,
                                                    granit_renderer& renderer);

} // namespace granit::detail

#endif
