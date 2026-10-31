// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_RENDER_GRAPH_GRAPH_H_
#define GRANIT_RENDER_GRAPH_GRAPH_H_

#include <granit/core/export.h>
#include <granit/render_graph/types.h>
#include <granit/renderer/frame.h>
#include <granit/renderer/renderer.h>

#ifdef __cplusplus
extern "C" {
#endif

GRANIT_API granit_result granit_render_graph_create(const granit_render_graph_desc* desc,
                                                    granit_render_graph* graph);
GRANIT_API granit_result granit_render_graph_destroy(granit_render_graph graph);

GRANIT_API granit_result granit_render_graph_import_buffer(granit_render_graph graph,
                                                           granit_buffer buffer, uint32_t exported,
                                                           const char* name, uint32_t name_length,
                                                           granit_render_graph_resource* resource);
GRANIT_API granit_result granit_render_graph_import_texture_view(
    granit_render_graph graph, granit_texture_view view, uint32_t exported, const char* name,
    uint32_t name_length, granit_render_graph_resource* resource);
GRANIT_API granit_result granit_render_graph_create_transient_buffer(
    granit_render_graph graph, const granit_buffer_desc* desc, const char* name,
    uint32_t name_length, granit_render_graph_resource* resource);
GRANIT_API granit_result granit_render_graph_create_transient_texture(
    granit_render_graph graph, const granit_texture_desc* desc, const char* name,
    uint32_t name_length, granit_render_graph_resource* resource);
GRANIT_API granit_result granit_render_graph_update_transient_texture(
    granit_render_graph graph, const granit_render_graph_transient_texture_update* update);

GRANIT_API granit_result granit_render_graph_add_pass(granit_render_graph graph,
                                                      const granit_render_graph_pass_desc* desc,
                                                      granit_render_graph_record_callback callback,
                                                      void* user_data,
                                                      granit_render_graph_pass* pass);
GRANIT_API granit_result granit_render_graph_add_dependency(granit_render_graph graph,
                                                            granit_render_graph_pass before,
                                                            granit_render_graph_pass after);
GRANIT_API granit_result granit_render_graph_compile(granit_render_graph graph);

/**
 * 执行 Graph。frame 为零时离屏提交；传入有效 Frame 时提交到该 Frame 对应的 Recorder。
 * 返回的 Recorder 由调用方在确认提交完成后销毁；临时资源由 Graph 在提交后释放。
 */
GRANIT_API granit_result granit_render_graph_execute(granit_render_graph graph,
                                                     granit_renderer renderer, granit_frame frame,
                                                     uint32_t frame_slot,
                                                     granit_render_graph_execution_result* result);

GRANIT_API granit_renderer
granit_render_graph_context_renderer(const granit_render_graph_pass_context* context);
GRANIT_API granit_command_recorder
granit_render_graph_context_recorder(const granit_render_graph_pass_context* context);
GRANIT_API granit_buffer granit_render_graph_context_buffer(
    const granit_render_graph_pass_context* context, granit_render_graph_resource resource);
GRANIT_API granit_texture_view granit_render_graph_context_texture_view(
    const granit_render_graph_pass_context* context, granit_render_graph_resource resource);

#ifdef __cplusplus
}
#endif

#endif
