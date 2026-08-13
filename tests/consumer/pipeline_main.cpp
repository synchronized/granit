// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/pipeline/material.hpp>
#include <granit/pipeline/render_pipeline.hpp>
#include <granit/pipeline/scene.hpp>

int main() {
  granit::render_pipeline pipeline;
  granit::scene_snapshot scene;
  granit::material_instance material;
  return !pipeline.valid() && !scene.valid() && !material.valid() &&
                 granit::material_parameter_id("base_color") != 0
             ? 0
             : 1;
}
