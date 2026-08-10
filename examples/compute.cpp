// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/granit.hpp>

#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <iterator>
#include <span>
#include <string>
#include <vector>

namespace {

std::vector<std::byte> load_shader() {
  std::ifstream stream{std::string{GRANIT_EXAMPLE_ASSET_DIR} + "/compute.comp.spv",
                       std::ios::binary};
  const std::vector<char> bytes{std::istreambuf_iterator<char>{stream}, {}};
  std::vector<std::byte> result(bytes.size());
  for (std::size_t index = 0; index < bytes.size(); ++index)
    result[index] = static_cast<std::byte>(bytes[index]);
  return result;
}

} // namespace

int main() {
  constexpr std::uint32_t value_count = 16;
  constexpr std::uint64_t buffer_size = value_count * sizeof(std::uint32_t);
  granit::renderer renderer;
  auto result =
      renderer.initialize({.application_name = "Granit Compute", .enable_validation = true});

  granit::buffer storage;
  granit::buffer readback;
  if (granit::succeeded(result)) {
    result = storage.initialize(
        renderer.native_handle(),
        {.size = buffer_size,
         .usage = granit::buffer_usage::storage | granit::buffer_usage::transfer_source,
         .location = granit::memory_location::device});
  }
  if (granit::succeeded(result)) {
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
  if (granit::succeeded(result))
    result = group_layout.initialize(renderer.native_handle(), std::span{&declaration, 1});
  const auto group_layout_handle = group_layout.native_handle();
  granit::pipeline_layout pipeline_layout;
  if (granit::succeeded(result))
    result =
        pipeline_layout.initialize(renderer.native_handle(), std::span{&group_layout_handle, 1});
  const granit::bind_group_entry entry{
      .binding = 0, .resource = storage.native_handle(), .size = buffer_size};
  granit::bind_group group;
  if (granit::succeeded(result)) {
    result = group.initialize(renderer.native_handle(), group_layout.native_handle(),
                              std::span{&entry, 1});
  }

  const auto code = load_shader();
  granit::shader shader;
  if (granit::succeeded(result) && code.empty())
    result = granit::result::invalid_argument;
  if (granit::succeeded(result)) {
    result = shader.initialize(renderer.native_handle(),
                               {.stage = granit::shader_stage::compute, .code = code});
  }
  granit::compute_pipeline pipeline;
  if (granit::succeeded(result)) {
    result =
        pipeline.initialize(renderer.native_handle(), {.layout = pipeline_layout.native_handle(),
                                                       .compute_shader = shader.native_handle()});
  }

  granit::command_recorder recorder;
  if (granit::succeeded(result))
    result = recorder.initialize(renderer.native_handle());
  if (granit::succeeded(result))
    result = recorder.begin();
  if (granit::succeeded(result))
    result = recorder.bind_compute_pipeline(pipeline.native_handle());
  const auto group_handle = group.native_handle();
  if (granit::succeeded(result)) {
    result = recorder.bind_compute_groups(pipeline_layout.native_handle(), 0,
                                          std::span{&group_handle, 1});
  }
  if (granit::succeeded(result))
    result = recorder.dispatch(value_count);
  const granit::buffer_copy_region copy{
      .source_offset = 0, .destination_offset = 0, .size = buffer_size};
  if (granit::succeeded(result)) {
    result = recorder.copy_buffer(storage.native_handle(), readback.native_handle(),
                                  std::span{&copy, 1});
  }
  if (granit::succeeded(result))
    result = recorder.end();
  if (granit::succeeded(result))
    result = recorder.submit();
  if (granit::succeeded(result))
    result = recorder.reset();

  void* mapped = nullptr;
  if (granit::succeeded(result))
    result = readback.map(0, buffer_size, &mapped);
  if (granit::succeeded(result)) {
    const auto* values = static_cast<const std::uint32_t*>(mapped);
    for (std::uint32_t index = 0; index < value_count; ++index)
      std::cout << values[index] << (index + 1 == value_count ? '\n' : ' ');
    result = readback.unmap();
  }
  if (granit::failed(result)) {
    std::cerr << "Compute 示例失败：" << granit::result_message(result) << '\n';
    return 1;
  }
  return 0;
}
