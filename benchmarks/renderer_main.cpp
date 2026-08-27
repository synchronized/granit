// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/renderer/buffer.h>
#include <granit/renderer/command_recorder.h>
#include <granit/renderer/pipeline.h>
#include <granit/renderer/renderer.h>
#include <granit/renderer/shader.h>
#include <granit/renderer/texture.h>
#include <granit/renderer/upload_batch.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

namespace {

using clock_type = std::chrono::steady_clock;

struct options {
  std::string_view case_name{"all"};
  std::uint32_t threads{1};
  std::uint64_t iterations{10'000};
  std::uint32_t samples{10};
  std::uint32_t warmup{2};
  std::uint64_t buffer_size{4'096};
  std::uint32_t commands{10};
  std::uint32_t submissions{100};
  std::uint32_t frames_in_flight{2};
  std::uint32_t uploads{10};
};

enum class benchmark_case {
  invalid_lookup,
  create_destroy,
  independent_write,
  recorder_create_destroy,
  empty_record,
  buffer_record,
  mixed_pipeline_record,
  queue_submit,
  queue_submit_batch,
  staging_buffer_upload,
  staging_texture_upload,
  batch_buffer_upload,
  batch_texture_upload
};

struct thread_context {
  granit_buffer buffer{GRANIT_NULL_HANDLE};
  granit_command_recorder recorder{GRANIT_NULL_HANDLE};
  granit_texture texture{GRANIT_NULL_HANDLE};
  granit_texture_view view{GRANIT_NULL_HANDLE};
  granit_upload_batch upload_batch{GRANIT_NULL_HANDLE};
  granit_bind_group compute_group{GRANIT_NULL_HANDLE};
  std::uint32_t texture_width{};
  std::uint32_t texture_height{};
  std::vector<granit_command_recorder> submit_recorders;
  std::vector<double> submit_latencies;
  std::vector<std::byte> data;
};

struct pipeline_fixture {
  granit_shader vertex_shader{GRANIT_NULL_HANDLE};
  granit_shader fragment_shader{GRANIT_NULL_HANDLE};
  granit_shader compute_shader{GRANIT_NULL_HANDLE};
  granit_bind_group_layout compute_group_layout{GRANIT_NULL_HANDLE};
  granit_pipeline_layout graphics_layout{GRANIT_NULL_HANDLE};
  granit_pipeline_layout compute_layout{GRANIT_NULL_HANDLE};
  granit_graphics_pipeline graphics_pipeline{GRANIT_NULL_HANDLE};
  granit_compute_pipeline compute_pipeline{GRANIT_NULL_HANDLE};
};

std::atomic_uint64_t checksum{};

void print_help() {
  std::cout << "用法：granit_renderer_benchmarks [选项]\n"
               "  --case <all|invalid_lookup|create_destroy|independent_write|\n"
               "          recorder_create_destroy|empty_record|buffer_record|\n"
               "          mixed_pipeline_record|queue_submit|queue_submit_batch|\n"
               "          staging_buffer_upload|\n"
               "          staging_texture_upload|batch_buffer_upload|\n"
               "          batch_texture_upload>\n"
               "  --threads <数量>       工作线程数\n"
               "  --iterations <数量>    每个线程、每个样本的操作数\n"
               "  --samples <数量>       正式样本数\n"
               "  --warmup <数量>        预热样本数\n"
               "  --buffer-size <字节>   Buffer 大小\n"
               "  --commands <数量>      每次录制的命令数或混合工作负载重复数\n"
               "  --submissions <数量>   每线程、每样本预录制并提交的 Recorder 数\n"
               "  --frames-in-flight <数量> Renderer 帧槽数量（1～4）\n"
               "  --uploads <数量>       每线程、每样本的同步 staging 上传数\n";
}

std::vector<std::byte> load_shader(std::string_view name) {
  std::ifstream stream{std::string{GRANIT_BENCHMARK_ASSET_DIR} + "/" + std::string{name},
                       std::ios::binary};
  const std::vector<char> bytes{std::istreambuf_iterator<char>{stream}, {}};
  std::vector<std::byte> result(bytes.size());
  std::transform(bytes.begin(), bytes.end(), result.begin(),
                 [](char value) { return static_cast<std::byte>(value); });
  return result;
}

void destroy_pipeline_fixture(granit_renderer renderer, pipeline_fixture& fixture) {
  if (fixture.graphics_pipeline != GRANIT_NULL_HANDLE)
    static_cast<void>(granit_graphics_pipeline_destroy(renderer, fixture.graphics_pipeline));
  if (fixture.compute_pipeline != GRANIT_NULL_HANDLE)
    static_cast<void>(granit_compute_pipeline_destroy(renderer, fixture.compute_pipeline));
  if (fixture.graphics_layout != GRANIT_NULL_HANDLE)
    static_cast<void>(granit_pipeline_layout_destroy(renderer, fixture.graphics_layout));
  if (fixture.compute_layout != GRANIT_NULL_HANDLE)
    static_cast<void>(granit_pipeline_layout_destroy(renderer, fixture.compute_layout));
  if (fixture.compute_group_layout != GRANIT_NULL_HANDLE)
    static_cast<void>(granit_bind_group_layout_destroy(renderer, fixture.compute_group_layout));
  if (fixture.vertex_shader != GRANIT_NULL_HANDLE)
    static_cast<void>(granit_shader_destroy(renderer, fixture.vertex_shader));
  if (fixture.fragment_shader != GRANIT_NULL_HANDLE)
    static_cast<void>(granit_shader_destroy(renderer, fixture.fragment_shader));
  if (fixture.compute_shader != GRANIT_NULL_HANDLE)
    static_cast<void>(granit_shader_destroy(renderer, fixture.compute_shader));
}

granit_result create_shader(granit_renderer renderer, granit_shader_stage stage,
                            std::string_view name, granit_shader& shader) {
  const auto code = load_shader(name);
  if (code.empty())
    return GRANIT_ERROR_INVALID_ARGUMENT;
  granit_shader_desc desc = GRANIT_SHADER_DESC_INIT;
  desc.stage = stage;
  desc.code = code.data();
  desc.code_size = code.size();
  return granit_shader_create(renderer, &desc, &shader);
}

granit_result create_pipeline_fixture(granit_renderer renderer, pipeline_fixture& fixture) {
  auto result = create_shader(renderer, GRANIT_SHADER_STAGE_VERTEX, "triangle.vert.spv",
                              fixture.vertex_shader);
  if (result == GRANIT_SUCCESS)
    result = create_shader(renderer, GRANIT_SHADER_STAGE_FRAGMENT, "triangle.frag.spv",
                           fixture.fragment_shader);
  if (result == GRANIT_SUCCESS)
    result = create_shader(renderer, GRANIT_SHADER_STAGE_COMPUTE, "compute.comp.spv",
                           fixture.compute_shader);
  granit_pipeline_layout_desc graphics_layout_desc = GRANIT_PIPELINE_LAYOUT_DESC_INIT;
  if (result == GRANIT_SUCCESS)
    result =
        granit_pipeline_layout_create(renderer, &graphics_layout_desc, &fixture.graphics_layout);
  const granit_bind_group_layout_entry declaration{0, GRANIT_BINDING_TYPE_STORAGE_BUFFER, 1,
                                                   GRANIT_SHADER_STAGE_COMPUTE_BIT};
  granit_bind_group_layout_desc group_layout_desc = GRANIT_BIND_GROUP_LAYOUT_DESC_INIT;
  group_layout_desc.entry_count = 1;
  group_layout_desc.entries = &declaration;
  if (result == GRANIT_SUCCESS) {
    result = granit_bind_group_layout_create(renderer, &group_layout_desc,
                                             &fixture.compute_group_layout);
  }
  granit_pipeline_layout_desc compute_layout_desc = GRANIT_PIPELINE_LAYOUT_DESC_INIT;
  compute_layout_desc.bind_group_layout_count = 1;
  compute_layout_desc.bind_group_layouts = &fixture.compute_group_layout;
  if (result == GRANIT_SUCCESS)
    result = granit_pipeline_layout_create(renderer, &compute_layout_desc, &fixture.compute_layout);
  const granit_texture_format color_format = GRANIT_TEXTURE_FORMAT_RGBA8_UNORM;
  granit_graphics_pipeline_desc graphics_desc = GRANIT_GRAPHICS_PIPELINE_DESC_INIT;
  graphics_desc.layout = fixture.graphics_layout;
  graphics_desc.vertex_shader = fixture.vertex_shader;
  graphics_desc.fragment_shader = fixture.fragment_shader;
  graphics_desc.color_format_count = 1;
  graphics_desc.color_formats = &color_format;
  if (result == GRANIT_SUCCESS) {
    result = granit_graphics_pipeline_create(renderer, &graphics_desc, &fixture.graphics_pipeline);
  }
  granit_compute_pipeline_desc compute_desc = GRANIT_COMPUTE_PIPELINE_DESC_INIT;
  compute_desc.layout = fixture.compute_layout;
  compute_desc.compute_shader = fixture.compute_shader;
  if (result == GRANIT_SUCCESS)
    result = granit_compute_pipeline_create(renderer, &compute_desc, &fixture.compute_pipeline);
  if (result != GRANIT_SUCCESS)
    destroy_pipeline_fixture(renderer, fixture);
  return result;
}

template <typename Value> bool parse_integer(std::string_view text, Value& value) {
  Value parsed{};
  const auto result = std::from_chars(text.data(), text.data() + text.size(), parsed);
  if (result.ec != std::errc{} || result.ptr != text.data() + text.size() || parsed == 0)
    return false;
  value = parsed;
  return true;
}

bool parse_options(int argc, char** argv, options& result) {
  for (int index = 1; index < argc; ++index) {
    const std::string_view argument{argv[index]};
    if (argument == "--help") {
      print_help();
      return false;
    }
    if (index + 1 >= argc) {
      std::cerr << "缺少选项值：" << argument << '\n';
      return false;
    }
    const std::string_view value{argv[++index]};
    if (argument == "--case")
      result.case_name = value;
    else if (argument == "--threads") {
      if (!parse_integer(value, result.threads))
        return false;
    } else if (argument == "--iterations") {
      if (!parse_integer(value, result.iterations))
        return false;
    } else if (argument == "--samples") {
      if (!parse_integer(value, result.samples))
        return false;
    } else if (argument == "--warmup") {
      if (!parse_integer(value, result.warmup))
        return false;
    } else if (argument == "--buffer-size") {
      if (!parse_integer(value, result.buffer_size))
        return false;
    } else if (argument == "--commands") {
      if (!parse_integer(value, result.commands))
        return false;
    } else if (argument == "--submissions") {
      if (!parse_integer(value, result.submissions))
        return false;
    } else if (argument == "--frames-in-flight") {
      if (!parse_integer(value, result.frames_in_flight))
        return false;
    } else if (argument == "--uploads") {
      if (!parse_integer(value, result.uploads))
        return false;
    } else {
      std::cerr << "未知选项：" << argument << '\n';
      return false;
    }
  }
  if (result.threads > std::max(1U, std::thread::hardware_concurrency())) {
    std::cerr << "线程数超过当前机器逻辑处理器数量\n";
    return false;
  }
  if (result.buffer_size % 4 != 0) {
    std::cerr << "Buffer 大小必须是 4 的倍数\n";
    return false;
  }
  if (result.frames_in_flight > GRANIT_MAX_FRAMES_IN_FLIGHT) {
    std::cerr << "frames-in-flight 必须在 1～4 之间\n";
    return false;
  }
  return true;
}

granit_result create_upload_buffer(granit_renderer renderer, std::uint64_t size,
                                   granit_buffer& buffer) {
  granit_buffer_desc desc = GRANIT_BUFFER_DESC_INIT;
  desc.usage = GRANIT_BUFFER_USAGE_TRANSFER_SOURCE_BIT;
  desc.memory_location = GRANIT_MEMORY_LOCATION_UPLOAD;
  desc.size = size;
  return granit_buffer_create(renderer, &desc, &buffer);
}

std::pair<std::uint32_t, std::uint32_t> texture_extent(std::uint64_t size) {
  const auto pixels = size / 4;
  auto width = static_cast<std::uint32_t>(std::min<std::uint64_t>(pixels, 1'024));
  while (pixels % width != 0)
    --width;
  return {width, static_cast<std::uint32_t>(pixels / width)};
}

void destroy_contexts(granit_renderer renderer, std::vector<thread_context>& contexts);

granit_result create_recorder(granit_renderer renderer, granit_command_recorder& recorder) {
  const granit_command_recorder_desc desc = GRANIT_COMMAND_RECORDER_DESC_INIT;
  return granit_command_recorder_create(renderer, &desc, &recorder);
}

std::vector<thread_context> make_contexts(granit_renderer renderer, benchmark_case selected,
                                          const options& config,
                                          const pipeline_fixture* pipelines) {
  std::vector<thread_context> contexts(config.threads);
  for (auto& context : contexts) {
    if ((selected == benchmark_case::empty_record || selected == benchmark_case::buffer_record ||
         selected == benchmark_case::mixed_pipeline_record) &&
        create_recorder(renderer, context.recorder) != GRANIT_SUCCESS) {
      destroy_contexts(renderer, contexts);
      return {};
    }
    if (selected == benchmark_case::independent_write ||
        selected == benchmark_case::staging_buffer_upload ||
        selected == benchmark_case::batch_buffer_upload) {
      context.data.resize(static_cast<std::size_t>(config.buffer_size), std::byte{0x5a});
      granit_buffer_desc desc = GRANIT_BUFFER_DESC_INIT;
      desc.usage = selected == benchmark_case::independent_write
                       ? GRANIT_BUFFER_USAGE_TRANSFER_SOURCE_BIT
                       : GRANIT_BUFFER_USAGE_TRANSFER_DESTINATION_BIT;
      desc.memory_location = selected == benchmark_case::independent_write
                                 ? GRANIT_MEMORY_LOCATION_UPLOAD
                                 : GRANIT_MEMORY_LOCATION_DEVICE;
      desc.size = config.buffer_size;
      if (granit_buffer_create(renderer, &desc, &context.buffer) != GRANIT_SUCCESS) {
        destroy_contexts(renderer, contexts);
        return {};
      }
    } else if (selected == benchmark_case::buffer_record) {
      granit_buffer_desc desc = GRANIT_BUFFER_DESC_INIT;
      desc.usage = GRANIT_BUFFER_USAGE_TRANSFER_DESTINATION_BIT;
      desc.memory_location = GRANIT_MEMORY_LOCATION_DEVICE;
      desc.size = config.buffer_size;
      if (granit_buffer_create(renderer, &desc, &context.buffer) != GRANIT_SUCCESS) {
        destroy_contexts(renderer, contexts);
        return {};
      }
    } else if (selected == benchmark_case::mixed_pipeline_record) {
      granit_texture_desc texture_desc = GRANIT_TEXTURE_DESC_INIT;
      texture_desc.format = GRANIT_TEXTURE_FORMAT_RGBA8_UNORM;
      texture_desc.usage = GRANIT_TEXTURE_USAGE_COLOR_ATTACHMENT_BIT;
      texture_desc.width = 32;
      texture_desc.height = 32;
      if (granit_texture_create_with_default_view(renderer, &texture_desc, &context.texture,
                                                  &context.view) != GRANIT_SUCCESS) {
        destroy_contexts(renderer, contexts);
        return {};
      }
      granit_buffer_desc buffer_desc = GRANIT_BUFFER_DESC_INIT;
      buffer_desc.usage = GRANIT_BUFFER_USAGE_STORAGE_BIT;
      buffer_desc.memory_location = GRANIT_MEMORY_LOCATION_DEVICE;
      buffer_desc.size = config.buffer_size;
      if (granit_buffer_create(renderer, &buffer_desc, &context.buffer) != GRANIT_SUCCESS) {
        destroy_contexts(renderer, contexts);
        return {};
      }
      const granit_bind_group_entry entry{0, 0, context.buffer, 0, config.buffer_size};
      granit_bind_group_desc group_desc = GRANIT_BIND_GROUP_DESC_INIT;
      group_desc.layout = pipelines->compute_group_layout;
      group_desc.entry_count = 1;
      group_desc.entries = &entry;
      if (granit_bind_group_create(renderer, &group_desc, &context.compute_group) !=
          GRANIT_SUCCESS) {
        destroy_contexts(renderer, contexts);
        return {};
      }
    } else if (selected == benchmark_case::queue_submit ||
               selected == benchmark_case::queue_submit_batch) {
      context.submit_recorders.resize(static_cast<std::size_t>(config.iterations),
                                      GRANIT_NULL_HANDLE);
      context.submit_latencies.reserve(static_cast<std::size_t>(config.iterations));
      for (auto& recorder : context.submit_recorders) {
        if (create_recorder(renderer, recorder) != GRANIT_SUCCESS ||
            granit_command_recorder_begin(renderer, recorder) != GRANIT_SUCCESS ||
            granit_command_recorder_end(renderer, recorder) != GRANIT_SUCCESS) {
          destroy_contexts(renderer, contexts);
          return {};
        }
      }
    } else if (selected == benchmark_case::staging_texture_upload ||
               selected == benchmark_case::batch_texture_upload) {
      context.data.resize(static_cast<std::size_t>(config.buffer_size), std::byte{0x5a});
      const auto [width, height] = texture_extent(config.buffer_size);
      context.texture_width = width;
      context.texture_height = height;
      granit_texture_desc desc = GRANIT_TEXTURE_DESC_INIT;
      desc.format = GRANIT_TEXTURE_FORMAT_RGBA8_UNORM;
      desc.usage = GRANIT_TEXTURE_USAGE_TRANSFER_DESTINATION_BIT;
      desc.memory_location = GRANIT_MEMORY_LOCATION_DEVICE;
      desc.width = width;
      desc.height = height;
      if (granit_texture_create(renderer, &desc, &context.texture) != GRANIT_SUCCESS) {
        destroy_contexts(renderer, contexts);
        return {};
      }
    }
    if (selected == benchmark_case::batch_buffer_upload ||
        selected == benchmark_case::batch_texture_upload) {
      const granit_upload_batch_desc desc = GRANIT_UPLOAD_BATCH_DESC_INIT;
      if (granit_upload_batch_create(renderer, &desc, &context.upload_batch) != GRANIT_SUCCESS) {
        destroy_contexts(renderer, contexts);
        return {};
      }
    }
  }
  return contexts;
}

void destroy_contexts(granit_renderer renderer, std::vector<thread_context>& contexts) {
  for (auto& context : contexts) {
    if (context.upload_batch != GRANIT_NULL_HANDLE)
      static_cast<void>(granit_upload_batch_destroy(renderer, context.upload_batch));
    for (const auto recorder : context.submit_recorders) {
      if (recorder != GRANIT_NULL_HANDLE)
        static_cast<void>(granit_command_recorder_destroy(renderer, recorder));
    }
    if (context.recorder != GRANIT_NULL_HANDLE)
      static_cast<void>(granit_command_recorder_destroy(renderer, context.recorder));
    if (context.compute_group != GRANIT_NULL_HANDLE)
      static_cast<void>(granit_bind_group_destroy(renderer, context.compute_group));
    if (context.view != GRANIT_NULL_HANDLE)
      static_cast<void>(granit_texture_view_destroy(renderer, context.view));
    if (context.texture != GRANIT_NULL_HANDLE)
      static_cast<void>(granit_texture_destroy(renderer, context.texture));
    if (context.buffer != GRANIT_NULL_HANDLE)
      static_cast<void>(granit_buffer_destroy(renderer, context.buffer));
  }
}

std::uint64_t run_operations(granit_renderer renderer, thread_context& context,
                             benchmark_case selected, const options& config,
                             const pipeline_fixture* pipelines, std::atomic_bool& failed) {
  std::uint64_t local_checksum{};
  const std::array<std::byte, 4> value{};
  if (selected == benchmark_case::queue_submit_batch) {
    const auto submit_begin = clock_type::now();
    const auto result = granit_command_recorder_submit_batch(
        renderer, context.submit_recorders.data(),
        static_cast<std::uint32_t>(context.submit_recorders.size()));
    const auto submit_end = clock_type::now();
    context.submit_latencies.push_back(
        std::chrono::duration<double, std::nano>{submit_end - submit_begin}.count() /
        static_cast<double>(config.iterations));
    if (result != GRANIT_SUCCESS)
      failed.store(true, std::memory_order_relaxed);
    return static_cast<std::uint64_t>(result);
  }
  if (selected == benchmark_case::batch_buffer_upload ||
      selected == benchmark_case::batch_texture_upload) {
    granit_result result = GRANIT_SUCCESS;
    for (std::uint64_t index = 0; index < config.iterations; ++index) {
      if (selected == benchmark_case::batch_buffer_upload) {
        result = granit_upload_batch_write_buffer(renderer, context.upload_batch, context.buffer, 0,
                                                  context.data.data(), context.data.size());
      } else {
        const granit_texture_data_layout layout{0, context.texture_width * 4,
                                                context.texture_height};
        const granit_texture_write_region region{0,
                                                 0,
                                                 1,
                                                 GRANIT_TEXTURE_ASPECT_COLOR_BIT,
                                                 0,
                                                 0,
                                                 0,
                                                 context.texture_width,
                                                 context.texture_height,
                                                 1};
        result = granit_upload_batch_write_texture(renderer, context.upload_batch, context.texture,
                                                   context.data.data(), context.data.size(),
                                                   &layout, &region);
      }
      if (result != GRANIT_SUCCESS)
        break;
    }
    if (result == GRANIT_SUCCESS)
      result = granit_upload_batch_submit(renderer, context.upload_batch);
    if (result != GRANIT_SUCCESS)
      failed.store(true, std::memory_order_relaxed);
    return static_cast<std::uint64_t>(result);
  }
  for (std::uint64_t index = 0; index < config.iterations; ++index) {
    granit_result result{};
    switch (selected) {
    case benchmark_case::invalid_lookup:
      result = granit_buffer_write(renderer, UINT64_C(0x0200000100000001), 0, value.data(),
                                   value.size());
      if (result != GRANIT_ERROR_INVALID_HANDLE) {
        failed.store(true, std::memory_order_relaxed);
        return local_checksum;
      }
      break;
    case benchmark_case::create_destroy: {
      granit_buffer buffer{GRANIT_NULL_HANDLE};
      result = create_upload_buffer(renderer, config.buffer_size, buffer);
      if (result != GRANIT_SUCCESS) {
        failed.store(true, std::memory_order_relaxed);
        return local_checksum;
      }
      local_checksum ^= buffer;
      if (granit_buffer_destroy(renderer, buffer) != GRANIT_SUCCESS) {
        failed.store(true, std::memory_order_relaxed);
        return local_checksum;
      }
      break;
    }
    case benchmark_case::independent_write:
      result = granit_buffer_write(renderer, context.buffer, 0, context.data.data(),
                                   context.data.size());
      if (result != GRANIT_SUCCESS) {
        failed.store(true, std::memory_order_relaxed);
        return local_checksum;
      }
      break;
    case benchmark_case::staging_buffer_upload:
      result = granit_buffer_write(renderer, context.buffer, 0, context.data.data(),
                                   context.data.size());
      if (result != GRANIT_SUCCESS) {
        failed.store(true, std::memory_order_relaxed);
        return local_checksum;
      }
      break;
    case benchmark_case::staging_texture_upload: {
      const granit_texture_data_layout layout{0, context.texture_width * 4, context.texture_height};
      const granit_texture_write_region region{0,
                                               0,
                                               1,
                                               GRANIT_TEXTURE_ASPECT_COLOR_BIT,
                                               0,
                                               0,
                                               0,
                                               context.texture_width,
                                               context.texture_height,
                                               1};
      result = granit_texture_write(renderer, context.texture, context.data.data(),
                                    context.data.size(), &layout, &region);
      if (result != GRANIT_SUCCESS) {
        failed.store(true, std::memory_order_relaxed);
        return local_checksum;
      }
      break;
    }
    case benchmark_case::recorder_create_destroy: {
      granit_command_recorder recorder{GRANIT_NULL_HANDLE};
      result = create_recorder(renderer, recorder);
      if (result != GRANIT_SUCCESS) {
        failed.store(true, std::memory_order_relaxed);
        return local_checksum;
      }
      local_checksum ^= recorder;
      if (granit_command_recorder_destroy(renderer, recorder) != GRANIT_SUCCESS) {
        failed.store(true, std::memory_order_relaxed);
        return local_checksum;
      }
      break;
    }
    case benchmark_case::empty_record:
      result = granit_command_recorder_begin(renderer, context.recorder);
      if (result == GRANIT_SUCCESS)
        result = granit_command_recorder_end(renderer, context.recorder);
      if (result == GRANIT_SUCCESS)
        result = granit_command_recorder_reset(renderer, context.recorder);
      if (result != GRANIT_SUCCESS) {
        failed.store(true, std::memory_order_relaxed);
        return local_checksum;
      }
      break;
    case benchmark_case::buffer_record:
      result = granit_command_recorder_begin(renderer, context.recorder);
      for (std::uint32_t command = 0; result == GRANIT_SUCCESS && command < config.commands;
           ++command) {
        result = granit_command_recorder_fill_buffer(renderer, context.recorder, context.buffer, 0,
                                                     config.buffer_size, command);
      }
      if (result == GRANIT_SUCCESS)
        result = granit_command_recorder_end(renderer, context.recorder);
      if (result == GRANIT_SUCCESS)
        result = granit_command_recorder_reset(renderer, context.recorder);
      if (result != GRANIT_SUCCESS) {
        failed.store(true, std::memory_order_relaxed);
        return local_checksum;
      }
      break;
    case benchmark_case::mixed_pipeline_record: {
      result = granit_command_recorder_begin(renderer, context.recorder);
      const granit_viewport viewport{0, 0, 32, 32, 0, 1};
      const granit_scissor scissor{0, 0, 32, 32};
      granit_color_attachment_desc color = GRANIT_COLOR_ATTACHMENT_DESC_INIT;
      color.view = context.view;
      granit_rendering_desc rendering = GRANIT_RENDERING_DESC_INIT;
      rendering.color_attachment_count = 1;
      rendering.color_attachments = &color;
      rendering.area.width = 32;
      rendering.area.height = 32;
      for (std::uint32_t command = 0; result == GRANIT_SUCCESS && command < config.commands;
           ++command) {
        result = granit_command_recorder_bind_graphics_pipeline(renderer, context.recorder,
                                                                pipelines->graphics_pipeline);
        if (result == GRANIT_SUCCESS)
          result =
              granit_command_recorder_set_viewports(renderer, context.recorder, 0, &viewport, 1);
        if (result == GRANIT_SUCCESS)
          result = granit_command_recorder_set_scissors(renderer, context.recorder, 0, &scissor, 1);
        if (result == GRANIT_SUCCESS)
          result = granit_command_recorder_begin_rendering(renderer, context.recorder, &rendering);
        if (result == GRANIT_SUCCESS)
          result = granit_command_recorder_draw(renderer, context.recorder, 3, 1, 0, 0);
        if (result == GRANIT_SUCCESS)
          result = granit_command_recorder_end_rendering(renderer, context.recorder);
        if (result == GRANIT_SUCCESS)
          result = granit_command_recorder_bind_compute_pipeline(renderer, context.recorder,
                                                                 pipelines->compute_pipeline);
        if (result == GRANIT_SUCCESS) {
          const granit_bind_groups_desc bind_desc{
              GRANIT_BIND_GROUPS_DESC_VERSION_1_SIZE, 0, &context.compute_group, 1, 0, nullptr};
          result = granit_command_recorder_bind_compute_groups(
              renderer, context.recorder, pipelines->compute_layout, &bind_desc);
        }
        if (result == GRANIT_SUCCESS)
          result = granit_command_recorder_dispatch(renderer, context.recorder, 1, 1, 1);
      }
      if (result == GRANIT_SUCCESS)
        result = granit_command_recorder_end(renderer, context.recorder);
      if (result == GRANIT_SUCCESS)
        result = granit_command_recorder_reset(renderer, context.recorder);
      if (result != GRANIT_SUCCESS) {
        failed.store(true, std::memory_order_relaxed);
        return local_checksum;
      }
      break;
    }
    case benchmark_case::queue_submit: {
      const auto submit_begin = clock_type::now();
      result = granit_command_recorder_submit(
          renderer, context.submit_recorders[static_cast<std::size_t>(index)]);
      const auto submit_end = clock_type::now();
      context.submit_latencies.push_back(
          std::chrono::duration<double, std::nano>{submit_end - submit_begin}.count());
      if (result != GRANIT_SUCCESS) {
        failed.store(true, std::memory_order_relaxed);
        return local_checksum;
      }
      break;
    }
    case benchmark_case::queue_submit_batch:
    case benchmark_case::batch_buffer_upload:
    case benchmark_case::batch_texture_upload:
      break;
    }
    local_checksum += static_cast<std::uint64_t>(result);
  }
  return local_checksum;
}

double run_sample(granit_renderer renderer, benchmark_case selected, const options& config,
                  const pipeline_fixture* pipelines, std::vector<double>* operation_latencies,
                  bool& succeeded) {
  auto contexts = make_contexts(renderer, selected, config, pipelines);
  if (contexts.size() != config.threads) {
    succeeded = false;
    return 0;
  }
  std::atomic_uint32_t ready{};
  std::atomic_bool start{};
  std::atomic_bool failed{};
  std::vector<std::thread> workers;
  workers.reserve(config.threads);
  for (std::uint32_t index = 0; index < config.threads; ++index) {
    workers.emplace_back([&, index] {
      ready.fetch_add(1, std::memory_order_release);
      while (!start.load(std::memory_order_acquire))
        std::this_thread::yield();
      const auto value =
          run_operations(renderer, contexts[index], selected, config, pipelines, failed);
      checksum.fetch_xor(value, std::memory_order_relaxed);
    });
  }
  while (ready.load(std::memory_order_acquire) != config.threads)
    std::this_thread::yield();
  const auto begin = clock_type::now();
  start.store(true, std::memory_order_release);
  for (auto& worker : workers)
    worker.join();
  const auto end = clock_type::now();
  if (operation_latencies != nullptr) {
    for (const auto& context : contexts) {
      operation_latencies->insert(operation_latencies->end(), context.submit_latencies.begin(),
                                  context.submit_latencies.end());
    }
  }
  destroy_contexts(renderer, contexts);
  succeeded = !failed.load(std::memory_order_relaxed);
  return std::chrono::duration<double, std::nano>{end - begin}.count();
}

double percentile(const std::vector<double>& sorted, double fraction) {
  const auto rank = std::ceil(fraction * static_cast<double>(sorted.size()));
  const auto index = static_cast<std::size_t>(std::max(1.0, rank)) - 1;
  return sorted[index];
}

bool run_case(granit_renderer renderer, std::string_view name, benchmark_case selected,
              const options& config, const pipeline_fixture* pipelines = nullptr) {
  bool succeeded{true};
  for (std::uint32_t index = 0; index < config.warmup; ++index)
    static_cast<void>(run_sample(renderer, selected, config, pipelines, nullptr, succeeded));
  if (!succeeded)
    return false;
  std::vector<double> samples;
  samples.reserve(config.samples);
  std::vector<double> operation_latencies;
  if (selected == benchmark_case::queue_submit || selected == benchmark_case::queue_submit_batch) {
    operation_latencies.reserve(static_cast<std::size_t>(config.threads) *
                                static_cast<std::size_t>(config.iterations) * config.samples);
  }
  double total_ns{};
  const auto operations =
      static_cast<double>(config.threads) * static_cast<double>(config.iterations);
  for (std::uint32_t index = 0; index < config.samples; ++index) {
    const auto elapsed = run_sample(renderer, selected, config, pipelines,
                                    selected == benchmark_case::queue_submit ||
                                            selected == benchmark_case::queue_submit_batch
                                        ? &operation_latencies
                                        : nullptr,
                                    succeeded);
    if (!succeeded)
      return false;
    total_ns += elapsed;
    samples.push_back(elapsed / operations);
  }
  std::sort(samples.begin(), samples.end());
  std::sort(operation_latencies.begin(), operation_latencies.end());
  const auto& distribution = operation_latencies.empty() ? samples : operation_latencies;
  const auto total_operations = operations * static_cast<double>(config.samples);
  std::cout << "1," << name << ',' << config.threads << ',' << config.iterations << ','
            << config.samples << ',' << static_cast<std::uint64_t>(total_ns) << ','
            << total_ns / total_operations << ',' << percentile(distribution, 0.50) << ','
            << percentile(distribution, 0.95) << ',' << percentile(distribution, 0.99) << ','
            << total_operations * 1'000'000'000.0 / total_ns << '\n';
  return true;
}

bool selected(std::string_view requested, std::string_view name) {
  return requested == "all" || requested == name;
}

std::string cpu_name() {
#ifdef _WIN32
  char* value{};
  std::size_t size{};
  if (_dupenv_s(&value, &size, "PROCESSOR_IDENTIFIER") != 0 || value == nullptr)
    return "unknown";
  std::string result{value};
  std::free(value);
  return result;
#else
  const auto* value = std::getenv("PROCESSOR_IDENTIFIER");
  return value == nullptr ? "unknown" : value;
#endif
}

} // namespace

int main(int argc, char** argv) {
  options config;
  if (!parse_options(argc, argv, config)) {
    for (int index = 1; index < argc; ++index) {
      if (std::string_view{argv[index]} == "--help")
        return 0;
    }
    return 2;
  }
  constexpr std::string_view cases[]{
      "invalid_lookup",          "create_destroy",         "independent_write",
      "recorder_create_destroy", "empty_record",           "buffer_record",
      "mixed_pipeline_record",   "queue_submit",           "queue_submit_batch",
      "staging_buffer_upload",   "staging_texture_upload", "batch_buffer_upload",
      "batch_texture_upload"};
  if (config.case_name != "all" &&
      std::find(std::begin(cases), std::end(cases), config.case_name) == std::end(cases)) {
    std::cerr << "未知 benchmark 用例：" << config.case_name << '\n';
    return 2;
  }

  granit_renderer_desc desc = GRANIT_RENDERER_DESC_INIT;
  desc.application_name = "Granit Renderer Benchmarks";
  desc.application_name_length = 26;
  desc.frames_in_flight = config.frames_in_flight;
  granit_renderer renderer{GRANIT_NULL_HANDLE};
  const auto create_result = granit_renderer_create(&desc, &renderer);
  if (create_result != GRANIT_SUCCESS) {
    std::cerr << "无法创建 Renderer：" << granit_result_message(create_result) << '\n';
    return 3;
  }

  std::cout << std::setprecision(10) << "# clock=steady_clock\n"
#ifdef NDEBUG
            << "# build_type=Release\n"
#else
            << "# build_type=Debug\n"
#endif
            << "# hardware_concurrency=" << std::thread::hardware_concurrency() << '\n'
            << "# compiler=" << GRANIT_BENCHMARK_COMPILER << '\n'
            << "# revision=" << GRANIT_BENCHMARK_REVISION << '\n'
            << "# link_mode=" << GRANIT_BENCHMARK_LINK_MODE << '\n'
            << "# system=" << GRANIT_BENCHMARK_SYSTEM << '\n'
            << "# cpu=" << cpu_name() << '\n'
            << "# buffer_size=" << config.buffer_size << '\n'
            << "# commands=" << config.commands << '\n'
            << "# submissions=" << config.submissions << '\n'
            << "# frames_in_flight=" << config.frames_in_flight << '\n'
            << "# uploads=" << config.uploads << '\n'
            << "schema,name,threads,iterations,samples,total_ns,ns_per_op,p50_ns,p95_ns,p99_ns,"
               "ops_per_second\n";

  bool succeeded{true};
  pipeline_fixture pipelines;
  if (selected(config.case_name, "mixed_pipeline_record")) {
    const auto pipeline_result = create_pipeline_fixture(renderer, pipelines);
    if (pipeline_result != GRANIT_SUCCESS) {
      std::cerr << "无法创建混合 Pipeline 基准资源：" << granit_result_message(pipeline_result)
                << '\n';
      static_cast<void>(granit_renderer_destroy(renderer));
      return 4;
    }
  }
  if (selected(config.case_name, "invalid_lookup"))
    succeeded &= run_case(renderer, "invalid_lookup", benchmark_case::invalid_lookup, config);
  if (selected(config.case_name, "create_destroy"))
    succeeded &= run_case(renderer, "create_destroy", benchmark_case::create_destroy, config);
  if (selected(config.case_name, "independent_write"))
    succeeded &= run_case(renderer, "independent_write", benchmark_case::independent_write, config);
  if (selected(config.case_name, "recorder_create_destroy"))
    succeeded &= run_case(renderer, "recorder_create_destroy",
                          benchmark_case::recorder_create_destroy, config);
  if (selected(config.case_name, "empty_record"))
    succeeded &= run_case(renderer, "empty_record", benchmark_case::empty_record, config);
  if (selected(config.case_name, "buffer_record"))
    succeeded &= run_case(renderer, "buffer_record", benchmark_case::buffer_record, config);
  if (selected(config.case_name, "mixed_pipeline_record")) {
    succeeded &= run_case(renderer, "mixed_pipeline_record", benchmark_case::mixed_pipeline_record,
                          config, &pipelines);
  }
  if (selected(config.case_name, "queue_submit")) {
    auto submit_config = config;
    submit_config.iterations = config.submissions;
    succeeded &= run_case(renderer, "queue_submit", benchmark_case::queue_submit, submit_config);
  }
  if (selected(config.case_name, "queue_submit_batch")) {
    auto submit_config = config;
    submit_config.iterations = config.submissions;
    succeeded &=
        run_case(renderer, "queue_submit_batch", benchmark_case::queue_submit_batch, submit_config);
  }
  if (selected(config.case_name, "staging_buffer_upload")) {
    auto upload_config = config;
    upload_config.iterations = config.uploads;
    succeeded &= run_case(renderer, "staging_buffer_upload", benchmark_case::staging_buffer_upload,
                          upload_config);
  }
  if (selected(config.case_name, "staging_texture_upload")) {
    auto upload_config = config;
    upload_config.iterations = config.uploads;
    succeeded &= run_case(renderer, "staging_texture_upload",
                          benchmark_case::staging_texture_upload, upload_config);
  }
  if (selected(config.case_name, "batch_buffer_upload")) {
    auto upload_config = config;
    upload_config.iterations = config.uploads;
    succeeded &= run_case(renderer, "batch_buffer_upload", benchmark_case::batch_buffer_upload,
                          upload_config);
  }
  if (selected(config.case_name, "batch_texture_upload")) {
    auto upload_config = config;
    upload_config.iterations = config.uploads;
    succeeded &= run_case(renderer, "batch_texture_upload", benchmark_case::batch_texture_upload,
                          upload_config);
  }
  destroy_pipeline_fixture(renderer, pipelines);
  const auto destroy_result = granit_renderer_destroy(renderer);
  std::cerr << "checksum=" << checksum.load(std::memory_order_relaxed) << '\n';
  if (!succeeded || destroy_result != GRANIT_SUCCESS) {
    std::cerr << "Renderer benchmark 执行失败\n";
    return 1;
  }
  return 0;
}
