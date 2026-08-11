// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "material/pbr_render_graph_adapter.h"

#include <new>
#include <utility>

namespace granit::material {

render_graph::pass_id add_pbr_graph_pass(render_graph::serial_graph& graph,
                                         pbr_graph_pass_desc desc,
                                         pbr_graph_record_callback callback, std::string name) {
  if (desc.color == render_graph::invalid_resource_id || desc.objects.empty() || !callback)
    return render_graph::invalid_pass_id;

  pbr_frame_constants frame{};
  std::vector<pbr_object_constants> objects;
  try {
    objects.reserve(desc.objects.size());
    for (const auto& object : desc.objects) {
      pbr_object_constants packed{};
      if (pack_pbr_draw_inputs(desc.view, object, desc.light, frame, packed) !=
          pbr_draw_input_error::none) {
        return render_graph::invalid_pass_id;
      }
      objects.push_back(packed);
    }
  } catch (const std::bad_alloc&) {
    return render_graph::invalid_pass_id;
  }

  render_graph::pass_desc pass{.side_effect = true,
                               .accesses = {{desc.color, render_graph::access_type::write}}};
  if (desc.depth != render_graph::invalid_resource_id)
    pass.accesses.push_back({desc.depth, render_graph::access_type::write});
  return graph.add_pass(
      std::move(pass),
      [frame, objects = std::move(objects), callback = std::move(callback)](
          render_graph::pass_context& context) { return callback(context, frame, objects); },
      std::move(name));
}

} // namespace granit::material
