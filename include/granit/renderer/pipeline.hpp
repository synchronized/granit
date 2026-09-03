// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_PIPELINE_HPP_
#define GRANIT_PIPELINE_HPP_

#include <array>
#include <optional>
#include <span>
#include <utility>

#include <granit/core/result.hpp>
#include <granit/renderer/pipeline.h>
#include <granit/renderer/resource_types.hpp>

namespace granit {

enum class binding_type : std::uint32_t {
  uniform_buffer = GRANIT_BINDING_TYPE_UNIFORM_BUFFER,
  storage_buffer = GRANIT_BINDING_TYPE_STORAGE_BUFFER,
  sampled_texture = GRANIT_BINDING_TYPE_SAMPLED_TEXTURE,
  sampled_texture_cube = GRANIT_BINDING_TYPE_SAMPLED_TEXTURE_CUBE,
  storage_texture = GRANIT_BINDING_TYPE_STORAGE_TEXTURE,
  sampler = GRANIT_BINDING_TYPE_SAMPLER,
  comparison_sampler = GRANIT_BINDING_TYPE_COMPARISON_SAMPLER,
  sampled_depth_texture = GRANIT_BINDING_TYPE_SAMPLED_DEPTH_TEXTURE,
  dynamic_uniform_buffer = GRANIT_BINDING_TYPE_DYNAMIC_UNIFORM_BUFFER,
};

enum class shader_stage_flags : std::uint32_t {
  vertex = GRANIT_SHADER_STAGE_VERTEX_BIT,
  fragment = GRANIT_SHADER_STAGE_FRAGMENT_BIT,
  compute = GRANIT_SHADER_STAGE_COMPUTE_BIT,
};

[[nodiscard]] constexpr shader_stage_flags operator|(shader_stage_flags left,
                                                     shader_stage_flags right) noexcept {
  return static_cast<shader_stage_flags>(static_cast<std::uint32_t>(left) |
                                         static_cast<std::uint32_t>(right));
}

struct bind_group_layout_entry {
  std::uint32_t binding{};
  binding_type type{binding_type::uniform_buffer};
  std::uint32_t array_count{1};
  shader_stage_flags visibility{shader_stage_flags::vertex};
};

class bind_group_layout {
public:
  bind_group_layout() = default;
  ~bind_group_layout() { static_cast<void>(reset()); }
  bind_group_layout(const bind_group_layout&) = delete;
  bind_group_layout& operator=(const bind_group_layout&) = delete;
  bind_group_layout(bind_group_layout&& other) noexcept
      : renderer_(std::exchange(other.renderer_, GRANIT_NULL_HANDLE)),
        handle_(std::exchange(other.handle_, GRANIT_NULL_HANDLE)) {}
  bind_group_layout& operator=(bind_group_layout&& other) noexcept {
    if (this != &other) {
      static_cast<void>(reset());
      renderer_ = std::exchange(other.renderer_, GRANIT_NULL_HANDLE);
      handle_ = std::exchange(other.handle_, GRANIT_NULL_HANDLE);
    }
    return *this;
  }
  [[nodiscard]] result initialize(granit_renderer renderer,
                                  std::span<const bind_group_layout_entry> entries) noexcept;
  [[nodiscard]] result reset() noexcept;
  [[nodiscard]] bool valid() const noexcept { return handle_ != GRANIT_NULL_HANDLE; }
  [[nodiscard]] granit_bind_group_layout native_handle() const noexcept { return handle_; }

private:
  granit_renderer renderer_{GRANIT_NULL_HANDLE};
  granit_bind_group_layout handle_{GRANIT_NULL_HANDLE};
};

struct bind_group_entry {
  std::uint32_t binding{};
  std::uint32_t array_element{};
  granit_handle resource{GRANIT_NULL_HANDLE};
  std::uint64_t offset{};
  std::uint64_t size{GRANIT_WHOLE_SIZE};
};

class bind_group {
public:
  bind_group() = default;
  ~bind_group() { static_cast<void>(reset()); }
  bind_group(const bind_group&) = delete;
  bind_group& operator=(const bind_group&) = delete;
  bind_group(bind_group&& other) noexcept
      : renderer_(std::exchange(other.renderer_, GRANIT_NULL_HANDLE)),
        handle_(std::exchange(other.handle_, GRANIT_NULL_HANDLE)) {}
  bind_group& operator=(bind_group&& other) noexcept {
    if (this != &other) {
      static_cast<void>(reset());
      renderer_ = std::exchange(other.renderer_, GRANIT_NULL_HANDLE);
      handle_ = std::exchange(other.handle_, GRANIT_NULL_HANDLE);
    }
    return *this;
  }
  [[nodiscard]] result initialize(granit_renderer renderer, granit_bind_group_layout layout,
                                  std::span<const bind_group_entry> entries) noexcept;
  [[nodiscard]] result reset() noexcept;
  [[nodiscard]] bool valid() const noexcept { return handle_ != GRANIT_NULL_HANDLE; }
  [[nodiscard]] granit_bind_group native_handle() const noexcept { return handle_; }

private:
  granit_renderer renderer_{GRANIT_NULL_HANDLE};
  granit_bind_group handle_{GRANIT_NULL_HANDLE};
};

class pipeline_layout {
public:
  pipeline_layout() = default;
  ~pipeline_layout() { static_cast<void>(reset()); }
  pipeline_layout(const pipeline_layout&) = delete;
  pipeline_layout& operator=(const pipeline_layout&) = delete;
  pipeline_layout(pipeline_layout&& other) noexcept
      : renderer_(std::exchange(other.renderer_, GRANIT_NULL_HANDLE)),
        handle_(std::exchange(other.handle_, GRANIT_NULL_HANDLE)) {}
  pipeline_layout& operator=(pipeline_layout&& other) noexcept {
    if (this != &other) {
      static_cast<void>(reset());
      renderer_ = std::exchange(other.renderer_, GRANIT_NULL_HANDLE);
      handle_ = std::exchange(other.handle_, GRANIT_NULL_HANDLE);
    }
    return *this;
  }
  [[nodiscard]] result
  initialize(granit_renderer renderer,
             std::span<const granit_bind_group_layout> bind_group_layouts = {}) noexcept;
  [[nodiscard]] result reset() noexcept;
  [[nodiscard]] bool valid() const noexcept { return handle_ != GRANIT_NULL_HANDLE; }
  [[nodiscard]] granit_pipeline_layout native_handle() const noexcept { return handle_; }

private:
  granit_renderer renderer_{GRANIT_NULL_HANDLE};
  granit_pipeline_layout handle_{GRANIT_NULL_HANDLE};
};

enum class vertex_format : std::uint32_t {
  float32 = GRANIT_VERTEX_FORMAT_FLOAT32,
  float32x2 = GRANIT_VERTEX_FORMAT_FLOAT32X2,
  float32x3 = GRANIT_VERTEX_FORMAT_FLOAT32X3,
  float32x4 = GRANIT_VERTEX_FORMAT_FLOAT32X4,
  uint32 = GRANIT_VERTEX_FORMAT_UINT32,
  uint32x2 = GRANIT_VERTEX_FORMAT_UINT32X2,
  uint32x3 = GRANIT_VERTEX_FORMAT_UINT32X3,
  uint32x4 = GRANIT_VERTEX_FORMAT_UINT32X4,
  sint32 = GRANIT_VERTEX_FORMAT_SINT32,
  sint32x2 = GRANIT_VERTEX_FORMAT_SINT32X2,
  sint32x3 = GRANIT_VERTEX_FORMAT_SINT32X3,
  sint32x4 = GRANIT_VERTEX_FORMAT_SINT32X4,
};

enum class vertex_step_mode : std::uint32_t {
  vertex = GRANIT_VERTEX_STEP_MODE_VERTEX,
  instance = GRANIT_VERTEX_STEP_MODE_INSTANCE,
};

struct vertex_attribute {
  std::uint32_t location{};
  vertex_format format{vertex_format::float32};
  std::uint32_t offset{};
  std::uint32_t reserved{};
};

struct vertex_buffer_layout {
  std::uint32_t stride{};
  vertex_step_mode step_mode{vertex_step_mode::vertex};
  std::span<const vertex_attribute> attributes;
};

enum class primitive_topology : std::uint32_t {
  point_list = GRANIT_PRIMITIVE_TOPOLOGY_POINT_LIST,
  line_list = GRANIT_PRIMITIVE_TOPOLOGY_LINE_LIST,
  line_strip = GRANIT_PRIMITIVE_TOPOLOGY_LINE_STRIP,
  triangle_list = GRANIT_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
  triangle_strip = GRANIT_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
};

enum class front_face : std::uint32_t {
  counter_clockwise = GRANIT_FRONT_FACE_COUNTER_CLOCKWISE,
  clockwise = GRANIT_FRONT_FACE_CLOCKWISE,
};

enum class cull_mode : std::uint32_t {
  none = GRANIT_CULL_MODE_NONE,
  front = GRANIT_CULL_MODE_FRONT,
  back = GRANIT_CULL_MODE_BACK,
  front_and_back = GRANIT_CULL_MODE_FRONT_AND_BACK,
};

enum class polygon_mode : std::uint32_t {
  fill = GRANIT_POLYGON_MODE_FILL,
  line = GRANIT_POLYGON_MODE_LINE,
  point = GRANIT_POLYGON_MODE_POINT,
};

struct primitive_state {
  primitive_topology topology{primitive_topology::triangle_list};
  front_face front{front_face::counter_clockwise};
  cull_mode cull{cull_mode::none};
  polygon_mode polygon{polygon_mode::fill};
};

struct depth_state {
  bool test_enabled{};
  bool write_enabled{};
  compare_operation compare{compare_operation::less_equal};
};

struct depth_bias_state {
  float constant_factor{};
  float slope_factor{};
  float clamp{};
};

enum class blend_factor : std::uint32_t {
  zero = GRANIT_BLEND_FACTOR_ZERO,
  one = GRANIT_BLEND_FACTOR_ONE,
  source_color = GRANIT_BLEND_FACTOR_SOURCE_COLOR,
  one_minus_source_color = GRANIT_BLEND_FACTOR_ONE_MINUS_SOURCE_COLOR,
  source_alpha = GRANIT_BLEND_FACTOR_SOURCE_ALPHA,
  one_minus_source_alpha = GRANIT_BLEND_FACTOR_ONE_MINUS_SOURCE_ALPHA,
  destination_color = GRANIT_BLEND_FACTOR_DESTINATION_COLOR,
  one_minus_destination_color = GRANIT_BLEND_FACTOR_ONE_MINUS_DESTINATION_COLOR,
  destination_alpha = GRANIT_BLEND_FACTOR_DESTINATION_ALPHA,
  one_minus_destination_alpha = GRANIT_BLEND_FACTOR_ONE_MINUS_DESTINATION_ALPHA,
};

enum class blend_operation : std::uint32_t {
  add = GRANIT_BLEND_OPERATION_ADD,
  subtract = GRANIT_BLEND_OPERATION_SUBTRACT,
  reverse_subtract = GRANIT_BLEND_OPERATION_REVERSE_SUBTRACT,
  min = GRANIT_BLEND_OPERATION_MIN,
  max = GRANIT_BLEND_OPERATION_MAX,
};

enum class color_write_mask : std::uint32_t {
  none = 0,
  red = GRANIT_COLOR_WRITE_RED_BIT,
  green = GRANIT_COLOR_WRITE_GREEN_BIT,
  blue = GRANIT_COLOR_WRITE_BLUE_BIT,
  alpha = GRANIT_COLOR_WRITE_ALPHA_BIT,
  all = GRANIT_COLOR_WRITE_ALL_BITS,
};

[[nodiscard]] constexpr color_write_mask operator|(color_write_mask left,
                                                   color_write_mask right) noexcept {
  return static_cast<color_write_mask>(static_cast<std::uint32_t>(left) |
                                       static_cast<std::uint32_t>(right));
}

struct color_blend_state {
  bool enabled{};
  blend_factor source_color_factor{blend_factor::one};
  blend_factor destination_color_factor{blend_factor::zero};
  blend_operation color_operation{blend_operation::add};
  blend_factor source_alpha_factor{blend_factor::one};
  blend_factor destination_alpha_factor{blend_factor::zero};
  blend_operation alpha_operation{blend_operation::add};
  color_write_mask write_mask{color_write_mask::all};
};

struct graphics_pipeline_desc {
  granit_pipeline_layout layout{GRANIT_NULL_HANDLE};
  granit_shader vertex_shader{GRANIT_NULL_HANDLE};
  granit_shader fragment_shader{GRANIT_NULL_HANDLE};
  std::span<const texture_format> color_formats;
  texture_format depth_stencil_format{texture_format::undefined};
  sample_count samples{sample_count::one};
  std::span<const vertex_buffer_layout> vertex_buffers;
  primitive_state primitive;
  std::optional<depth_state> depth;
  std::span<const color_blend_state> color_blends;
  std::optional<depth_bias_state> depth_bias;
};

class graphics_pipeline {
public:
  graphics_pipeline() = default;
  ~graphics_pipeline() { static_cast<void>(reset()); }
  graphics_pipeline(const graphics_pipeline&) = delete;
  graphics_pipeline& operator=(const graphics_pipeline&) = delete;
  graphics_pipeline(graphics_pipeline&& other) noexcept
      : renderer_(std::exchange(other.renderer_, GRANIT_NULL_HANDLE)),
        handle_(std::exchange(other.handle_, GRANIT_NULL_HANDLE)) {}
  graphics_pipeline& operator=(graphics_pipeline&& other) noexcept {
    if (this != &other) {
      static_cast<void>(reset());
      renderer_ = std::exchange(other.renderer_, GRANIT_NULL_HANDLE);
      handle_ = std::exchange(other.handle_, GRANIT_NULL_HANDLE);
    }
    return *this;
  }
  [[nodiscard]] result initialize(granit_renderer renderer,
                                  const graphics_pipeline_desc& desc) noexcept;
  [[nodiscard]] result reset() noexcept;
  [[nodiscard]] bool valid() const noexcept { return handle_ != GRANIT_NULL_HANDLE; }
  [[nodiscard]] granit_graphics_pipeline native_handle() const noexcept { return handle_; }

private:
  granit_renderer renderer_{GRANIT_NULL_HANDLE};
  granit_graphics_pipeline handle_{GRANIT_NULL_HANDLE};
};

struct compute_pipeline_desc {
  granit_pipeline_layout layout{GRANIT_NULL_HANDLE};
  granit_shader compute_shader{GRANIT_NULL_HANDLE};
};

/** 无异常、move-only 的 Compute Pipeline RAII 包装。 */
class compute_pipeline {
public:
  compute_pipeline() = default;
  ~compute_pipeline() { static_cast<void>(reset()); }
  compute_pipeline(const compute_pipeline&) = delete;
  compute_pipeline& operator=(const compute_pipeline&) = delete;
  compute_pipeline(compute_pipeline&& other) noexcept
      : renderer_(std::exchange(other.renderer_, GRANIT_NULL_HANDLE)),
        handle_(std::exchange(other.handle_, GRANIT_NULL_HANDLE)) {}
  compute_pipeline& operator=(compute_pipeline&& other) noexcept {
    if (this != &other) {
      static_cast<void>(reset());
      renderer_ = std::exchange(other.renderer_, GRANIT_NULL_HANDLE);
      handle_ = std::exchange(other.handle_, GRANIT_NULL_HANDLE);
    }
    return *this;
  }
  [[nodiscard]] result initialize(granit_renderer renderer,
                                  const compute_pipeline_desc& desc) noexcept;
  [[nodiscard]] result reset() noexcept;
  [[nodiscard]] bool valid() const noexcept { return handle_ != GRANIT_NULL_HANDLE; }
  [[nodiscard]] granit_compute_pipeline native_handle() const noexcept { return handle_; }

private:
  granit_renderer renderer_{GRANIT_NULL_HANDLE};
  granit_compute_pipeline handle_{GRANIT_NULL_HANDLE};
};

inline result
bind_group_layout::initialize(granit_renderer renderer,
                              std::span<const bind_group_layout_entry> entries) noexcept {
  if (valid() || entries.size() > UINT32_MAX)
    return result::invalid_argument;
  if (renderer == GRANIT_NULL_HANDLE)
    return result::invalid_handle;
  static_assert(sizeof(bind_group_layout_entry) == sizeof(granit_bind_group_layout_entry));
  const granit_bind_group_layout_desc desc{
      .struct_size = GRANIT_BIND_GROUP_LAYOUT_DESC_VERSION_1_SIZE,
      .entry_count = static_cast<std::uint32_t>(entries.size()),
      .entries = reinterpret_cast<const granit_bind_group_layout_entry*>(entries.data()),
      .reserved = 0};
  const auto value = granit_bind_group_layout_create(renderer, &desc, &handle_);
  if (value == GRANIT_SUCCESS)
    renderer_ = renderer;
  return from_native(value);
}

inline result bind_group_layout::reset() noexcept {
  if (!valid())
    return result::success;
  const auto renderer = std::exchange(renderer_, GRANIT_NULL_HANDLE);
  const auto handle = std::exchange(handle_, GRANIT_NULL_HANDLE);
  return from_native(granit_bind_group_layout_destroy(renderer, handle));
}

inline result bind_group::initialize(granit_renderer renderer, granit_bind_group_layout layout,
                                     std::span<const bind_group_entry> entries) noexcept {
  if (valid() || entries.size() > UINT32_MAX)
    return result::invalid_argument;
  if (renderer == GRANIT_NULL_HANDLE || layout == GRANIT_NULL_HANDLE)
    return result::invalid_handle;
  static_assert(sizeof(bind_group_entry) == sizeof(granit_bind_group_entry));
  const granit_bind_group_desc desc{
      .struct_size = GRANIT_BIND_GROUP_DESC_VERSION_1_SIZE,
      .entry_count = static_cast<std::uint32_t>(entries.size()),
      .layout = layout,
      .entries = reinterpret_cast<const granit_bind_group_entry*>(entries.data()),
      .reserved = 0};
  const auto value = granit_bind_group_create(renderer, &desc, &handle_);
  if (value == GRANIT_SUCCESS)
    renderer_ = renderer;
  return from_native(value);
}

inline result bind_group::reset() noexcept {
  if (!valid())
    return result::success;
  const auto renderer = std::exchange(renderer_, GRANIT_NULL_HANDLE);
  const auto handle = std::exchange(handle_, GRANIT_NULL_HANDLE);
  return from_native(granit_bind_group_destroy(renderer, handle));
}

inline result
pipeline_layout::initialize(granit_renderer renderer,
                            std::span<const granit_bind_group_layout> bind_group_layouts) noexcept {
  if (valid() || bind_group_layouts.size() > UINT32_MAX)
    return result::invalid_argument;
  if (renderer == GRANIT_NULL_HANDLE)
    return result::invalid_handle;
  const granit_pipeline_layout_desc desc{.struct_size = GRANIT_PIPELINE_LAYOUT_DESC_VERSION_1_SIZE,
                                         .bind_group_layout_count =
                                             static_cast<std::uint32_t>(bind_group_layouts.size()),
                                         .bind_group_layouts = bind_group_layouts.data(),
                                         .reserved = 0};
  const auto value = granit_pipeline_layout_create(renderer, &desc, &handle_);
  if (value == GRANIT_SUCCESS)
    renderer_ = renderer;
  return from_native(value);
}

inline result pipeline_layout::reset() noexcept {
  if (!valid())
    return result::success;
  const auto renderer = std::exchange(renderer_, GRANIT_NULL_HANDLE);
  const auto handle = std::exchange(handle_, GRANIT_NULL_HANDLE);
  return from_native(granit_pipeline_layout_destroy(renderer, handle));
}

inline result graphics_pipeline::initialize(granit_renderer renderer,
                                            const graphics_pipeline_desc& desc) noexcept {
  if (valid() || desc.color_formats.size() > UINT32_MAX ||
      desc.vertex_buffers.size() > UINT32_MAX || desc.color_blends.size() > UINT32_MAX)
    return result::invalid_argument;
  if (renderer == GRANIT_NULL_HANDLE)
    return result::invalid_handle;
  static_assert(sizeof(vertex_attribute) == sizeof(granit_vertex_attribute));
  if (desc.vertex_buffers.size() > 16)
    return result::invalid_argument;
  std::array<granit_vertex_buffer_layout, 16> vertex_buffers{};
  for (std::size_t index = 0; index < desc.vertex_buffers.size(); ++index) {
    const auto& source = desc.vertex_buffers[index];
    if (source.attributes.size() > UINT32_MAX)
      return result::invalid_argument;
    vertex_buffers[index] = {
        .stride = source.stride,
        .step_mode = static_cast<granit_vertex_step_mode>(source.step_mode),
        .attribute_count = static_cast<std::uint32_t>(source.attributes.size()),
        .reserved = 0,
        .attributes = reinterpret_cast<const granit_vertex_attribute*>(source.attributes.data())};
  }
  if (desc.color_blends.size() > 8)
    return result::invalid_argument;
  std::array<granit_color_blend_state, 8> color_blends{};
  for (std::size_t index = 0; index < desc.color_blends.size(); ++index) {
    const auto& source = desc.color_blends[index];
    color_blends[index] = {
        .enabled = source.enabled ? UINT32_C(1) : UINT32_C(0),
        .source_color_factor = static_cast<granit_blend_factor>(source.source_color_factor),
        .destination_color_factor =
            static_cast<granit_blend_factor>(source.destination_color_factor),
        .color_operation = static_cast<granit_blend_operation>(source.color_operation),
        .source_alpha_factor = static_cast<granit_blend_factor>(source.source_alpha_factor),
        .destination_alpha_factor =
            static_cast<granit_blend_factor>(source.destination_alpha_factor),
        .alpha_operation = static_cast<granit_blend_operation>(source.alpha_operation),
        .write_mask = static_cast<granit_color_write_mask>(source.write_mask)};
  }
  granit_depth_state depth{};
  const granit_depth_state* depth_pointer = nullptr;
  if (desc.depth) {
    depth = {.test_enabled = desc.depth->test_enabled ? UINT32_C(1) : UINT32_C(0),
             .write_enabled = desc.depth->write_enabled ? UINT32_C(1) : UINT32_C(0),
             .compare = static_cast<granit_compare_operation>(desc.depth->compare),
             .reserved = 0};
    depth_pointer = &depth;
  }
  granit_depth_bias_state depth_bias{};
  const granit_depth_bias_state* depth_bias_pointer = nullptr;
  if (desc.depth_bias) {
    depth_bias = {.constant_factor = desc.depth_bias->constant_factor,
                  .slope_factor = desc.depth_bias->slope_factor,
                  .clamp = desc.depth_bias->clamp,
                  .reserved = 0};
    depth_bias_pointer = &depth_bias;
  }
  const auto* formats = reinterpret_cast<const granit_texture_format*>(desc.color_formats.data());
  const granit_graphics_pipeline_desc native{
      .struct_size = GRANIT_GRAPHICS_PIPELINE_DESC_VERSION_5_SIZE,
      .reserved = 0,
      .layout = desc.layout,
      .vertex_shader = desc.vertex_shader,
      .fragment_shader = desc.fragment_shader,
      .color_format_count = static_cast<std::uint32_t>(desc.color_formats.size()),
      .color_formats = formats,
      .depth_stencil_format = static_cast<granit_texture_format>(desc.depth_stencil_format),
      .sample_count = static_cast<granit_sample_count>(desc.samples),
      .reserved_2 = 0,
      .vertex_buffer_layout_count = static_cast<std::uint32_t>(desc.vertex_buffers.size()),
      .reserved_3 = 0,
      .vertex_buffer_layouts = vertex_buffers.data(),
      .primitive = {.topology = static_cast<granit_primitive_topology>(desc.primitive.topology),
                    .front_face = static_cast<granit_front_face>(desc.primitive.front),
                    .cull_mode = static_cast<granit_cull_mode>(desc.primitive.cull),
                    .polygon_mode = static_cast<granit_polygon_mode>(desc.primitive.polygon)},
      .depth = depth_pointer,
      .color_blend_count = static_cast<std::uint32_t>(desc.color_blends.size()),
      .reserved_4 = 0,
      .color_blends = color_blends.data(),
      .depth_bias = depth_bias_pointer};
  const auto value = granit_graphics_pipeline_create(renderer, &native, &handle_);
  if (value == GRANIT_SUCCESS)
    renderer_ = renderer;
  return from_native(value);
}

inline result graphics_pipeline::reset() noexcept {
  if (!valid())
    return result::success;
  const auto renderer = std::exchange(renderer_, GRANIT_NULL_HANDLE);
  const auto handle = std::exchange(handle_, GRANIT_NULL_HANDLE);
  return from_native(granit_graphics_pipeline_destroy(renderer, handle));
}

inline result compute_pipeline::initialize(granit_renderer renderer,
                                           const compute_pipeline_desc& desc) noexcept {
  if (valid())
    return result::invalid_argument;
  if (renderer == GRANIT_NULL_HANDLE)
    return result::invalid_handle;
  const granit_compute_pipeline_desc native{.struct_size =
                                                GRANIT_COMPUTE_PIPELINE_DESC_VERSION_1_SIZE,
                                            .reserved = 0,
                                            .layout = desc.layout,
                                            .compute_shader = desc.compute_shader,
                                            .reserved_2 = 0};
  const auto value = granit_compute_pipeline_create(renderer, &native, &handle_);
  if (value == GRANIT_SUCCESS)
    renderer_ = renderer;
  return from_native(value);
}

inline result compute_pipeline::reset() noexcept {
  if (!valid())
    return result::success;
  const auto renderer = std::exchange(renderer_, GRANIT_NULL_HANDLE);
  const auto handle = std::exchange(handle_, GRANIT_NULL_HANDLE);
  return from_native(granit_compute_pipeline_destroy(renderer, handle));
}

} // namespace granit

#endif
