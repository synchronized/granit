// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/pipeline/render_pipeline.hpp>

#include <type_traits>

static_assert(!std::is_copy_constructible_v<granit::render_pipeline>);
static_assert(std::is_move_constructible_v<granit::render_pipeline>);
static_assert(requires(granit::render_pipeline& pipeline, granit::renderer& renderer,
                       const granit::render_pipeline_desc& desc,
                       const granit::render_pipeline_render_desc& render,
                       granit::render_pipeline_metrics& metrics) {
  pipeline.initialize(renderer, desc);
  pipeline.render(render);
  pipeline.get_metrics(metrics);
  pipeline.ref();
});
