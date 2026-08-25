// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/pipeline/material.hpp>
#include <granit/pipeline/mesh.hpp>
#include <granit/pipeline/render_pipeline.hpp>
#include <granit/pipeline/scene.hpp>
#include <granit/renderer/renderer.hpp>

#include "linkage_check.h"

#include <utility>

int main() {
  granit::render_pipeline pipeline;
  granit::scene_snapshot scene;
  granit::material_instance material;
  granit::mesh mesh;
  if (pipeline.valid() || scene.valid() || material.valid() || mesh.valid() ||
      granit::material_parameter_id("base_color") == 0)
    return 1;
  const granit_render_pipeline_desc desc = GRANIT_RENDER_PIPELINE_DESC_INIT;
  if (pipeline.initialize(GRANIT_NULL_HANDLE, desc) != granit::result::invalid_handle ||
      pipeline.valid())
    return 2;

  granit::renderer renderer;
  const auto renderer_result = renderer.initialize();
  if (renderer_result == granit::result::backend_unavailable ||
      renderer_result == granit::result::incompatible_driver ||
      renderer_result == granit::result::no_suitable_device)
    return renderer.valid() ? 3 : 0;
  if (granit::failed(renderer_result))
    return 4;
  if (granit::failed(pipeline.initialize(renderer.native_handle(), desc)))
    return 5;
  granit::render_pipeline moved = std::move(pipeline);
  if (pipeline.valid() || !moved.valid())
    return 6;
  if (granit::failed(moved.reset()) || granit::failed(moved.reset()))
    return 7;
  return granit::failed(renderer.reset()) ? 8 : 0;
}
