// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_BACKEND_RESOURCES_H_
#define GRANIT_BACKEND_RESOURCES_H_

namespace granit::detail {

/** 仅表达后端资源拥有关系，原生句柄与销毁方式由具体后端保存。 */
class backend_resource {
public:
  backend_resource() = default;
  virtual ~backend_resource() = default;
  backend_resource(const backend_resource&) = delete;
  backend_resource& operator=(const backend_resource&) = delete;
  backend_resource(backend_resource&&) = delete;
  backend_resource& operator=(backend_resource&&) = delete;
};

class backend_buffer_resource : public backend_resource {};
class backend_texture_resource : public backend_resource {};
class backend_texture_view_resource : public backend_resource {};
class backend_sampler_resource : public backend_resource {};
class backend_shader_resource : public backend_resource {};
class backend_bind_group_layout_resource : public backend_resource {};
class backend_bind_group_resource : public backend_resource {};
class backend_pipeline_layout_resource : public backend_resource {};
class backend_graphics_pipeline_resource : public backend_resource {};
class backend_compute_pipeline_resource : public backend_resource {};
class backend_command_recorder_resource : public backend_resource {};
class backend_surface_resource : public backend_resource {};
class backend_swapchain_resource : public backend_resource {};
class backend_timestamp_query_pool_resource : public backend_resource {};

} // namespace granit::detail

#endif
