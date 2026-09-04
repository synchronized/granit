// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_PIPELINE_RENDER_PIPELINE_STATE_H_
#define GRANIT_PIPELINE_RENDER_PIPELINE_STATE_H_

#include "lighting/ibl_resources.h"
#include "lighting/shadow_ibl_resources.h"
#include "lighting/tone_mapping_resources.h"
#include "pipeline/default_ibl_resources.h"
#include "pipeline/dynamic_uniform_arena.h"
#include "pipeline/pbr_draw_bindings.h"

#include <granit/pipeline/render_pipeline.h>
#include <granit/renderer/shader.hpp>
#include <granit/renderer/texture.hpp>
#include <granit/renderer/timestamp_query.h>

#include <array>
#include <memory>
#include <mutex>
#include <vector>

namespace granit::pipeline::detail {

/** Render Pipeline 各录制组件共享的私有状态；不跨公共 ABI。 */
struct render_pipeline_state {
  struct metrics_slot {
    granit_timestamp_query_pool pool = GRANIT_NULL_HANDLE;
    bool pending = false;
  };
  struct shadow_pipeline_entry {
    granit_pipeline_layout layout = GRANIT_NULL_HANDLE;
    granit_mesh mesh = GRANIT_NULL_HANDLE;
    granit_graphics_pipeline pipeline = GRANIT_NULL_HANDLE;
  };
  struct draw_binding_entry {
    granit_material material = GRANIT_NULL_HANDLE;
    lighting::ibl_texture_views ibl_views{};
    pbr_draw_bindings bindings;
    lighting::shadow_ibl_resources lighting;
  };

  std::mutex mutex;
  granit_renderer renderer = GRANIT_NULL_HANDLE;
  granit_render_pipeline_record_callback record = nullptr;
  void* user_data = nullptr;
  std::array<lighting::tone_mapping_pipeline_resources, 4> tone_mapping_pipelines;
  default_ibl_resources default_ibl;
  granit::texture shadow_texture;
  granit::texture_view shadow_view;
  granit::shader shadow_vertex_shader;
  granit::shader shadow_fragment_shader;
  granit::texture shadow_placeholder_texture;
  granit::texture_view shadow_placeholder_view;
  std::vector<shadow_pipeline_entry> shadow_pipelines;
  std::vector<draw_binding_entry> opaque_draw_bindings;
  std::vector<draw_binding_entry> shadow_draw_bindings;
  dynamic_uniform_arena uniform_arena;
  std::vector<metrics_slot> metrics_slots;
  granit_render_pipeline_metrics metrics = GRANIT_RENDER_PIPELINE_METRICS_INIT;
  bool metrics_enabled = false;
  bool metrics_available = false;
  float shadow_half_extent = 20.0F;
  granit_sample_count sample_count = GRANIT_SAMPLE_COUNT_1;
  bool enable_fxaa = true;
  bool enable_specular_aa = true;
  bool alive = true;
};

} // namespace granit::pipeline::detail

#endif
