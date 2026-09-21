// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "support/shader_asset_file.h"

#include <granit/granit.hpp>

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <span>
#include <string>
#include <vector>

int main() {
  constexpr std::uint32_t value_count = 16;
  constexpr std::uint64_t buffer_size = value_count * sizeof(std::uint32_t);
  granit::renderer renderer;
  auto result =
      renderer.initialize({.application_name = "Granit Compute", .enable_validation = true});

  granit::buffer storage;
  granit::buffer readback;
  if (result.ok()) {
    result = storage.initialize(
        renderer.native_handle(),
        {.size = buffer_size,
         .usage = granit::buffer_usage::storage | granit::buffer_usage::transfer_source,
         .location = granit::memory_location::device});
  }
  if (result.ok()) {
    result = readback.initialize(renderer.native_handle(),
                                 {.size = buffer_size,
                                  .usage = granit::buffer_usage::transfer_destination,
                                  .location = granit::memory_location::readback});
  }

  const granit::bind_group_layout_entry declaration{.binding = 0,
                                                    .type = granit::binding_type::storage_buffer,
                                                    .visibility =
                                                        granit::shader_stage_flags::compute};
  granit::bind_group_layout group_layout;
  if (result.ok())
    result = group_layout.initialize(renderer.native_handle(), std::span{&declaration, 1});
  const auto group_layout_handle = group_layout.native_handle();
  granit::pipeline_layout pipeline_layout;
  if (result.ok())
    result =
        pipeline_layout.initialize(renderer.native_handle(), std::span{&group_layout_handle, 1});
  const granit::bind_group_entry entry{
      .binding = 0, .resource = storage.native_handle(), .size = buffer_size};
  granit::bind_group group;
  if (result.ok()) {
    result = group.initialize(renderer.native_handle(), group_layout.native_handle(),
                              std::span{&entry, 1});
  }

  granit::shader shader;
  if (result.ok())
    result = granit::tests::load_shader_asset(
        renderer.native_handle(), std::string{GRANIT_SMOKE_ASSET_DIR} + "/compute.comp.grshaderobj",
        shader);
  granit::compute_pipeline pipeline;
  if (result.ok()) {
    result = pipeline.initialize(renderer,
                                 {.layout = pipeline_layout.ref(), .compute_shader = shader.ref()});
  }

  granit::command_recorder recorder;
  if (result.ok())
    result = recorder.initialize(renderer.native_handle());
  if (result.ok())
    result = recorder.begin();
  if (result.ok())
    result = recorder.bind_compute_pipeline(pipeline);
  const auto group_handle = group.native_handle();
  if (result.ok()) {
    result = recorder.bind_compute_groups(pipeline_layout.native_handle(), 0,
                                          std::span{&group_handle, 1});
  }
  if (result.ok())
    result = recorder.dispatch(value_count);
  const granit::buffer_copy_region copy{
      .source_offset = 0, .destination_offset = 0, .size = buffer_size};
  if (result.ok()) {
    result = recorder.copy_buffer(storage.native_handle(), readback.native_handle(),
                                  std::span{&copy, 1});
  }
  if (result.ok())
    result = recorder.end();
  if (result.ok())
    result = recorder.submit();
  if (result.ok())
    result = recorder.reset();

  void* mapped = nullptr;
  if (result.ok())
    result = readback.map(0, buffer_size, &mapped);
  bool values_match = true;
  if (result.ok()) {
    const auto* values = static_cast<const std::uint32_t*>(mapped);
    for (std::uint32_t index = 0; index < value_count; ++index) {
      const auto expected = index * 3U + 7U;
      if (values[index] != expected) {
        std::cerr << "Compute 回读值不一致：索引 " << index << "，实际 " << values[index]
                  << "，预期 " << expected << '\n';
        values_match = false;
      }
    }
    result = readback.unmap();
  }
  if (result.failed() || !values_match) {
    if (result.failed())
      std::cerr << "Compute GPU 测试失败：" << granit::result_message(result) << '\n';
    return 1;
  }
  return 0;
}
