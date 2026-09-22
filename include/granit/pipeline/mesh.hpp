// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_PIPELINE_MESH_HPP_
#define GRANIT_PIPELINE_MESH_HPP_

#include <granit/core/result.hpp>
#include <granit/pipeline/mesh.h>
#include <granit/renderer/buffer.hpp>
#include <granit/renderer/pipeline.hpp>
#include <granit/renderer/renderer.hpp>

#include <cstdint>
#include <limits>
#include <new>
#include <span>
#include <utility>
#include <vector>

namespace granit {

class mesh;

/** 不拥有 Mesh，只在来源 Mesh 的有效期内使用。 */
class mesh_ref {
public:
  mesh_ref() = default;

  [[nodiscard]] constexpr bool valid() const noexcept { return handle_ != GRANIT_NULL_HANDLE; }
  [[nodiscard]] constexpr explicit operator bool() const noexcept { return valid(); }
  [[nodiscard]] constexpr granit_mesh native_handle() const noexcept { return handle_; }
  [[nodiscard]] static constexpr mesh_ref from_native(granit_mesh handle) noexcept {
    return mesh_ref{handle};
  }

private:
  friend class mesh;

  explicit constexpr mesh_ref(granit_mesh handle) noexcept : handle_(handle) {}

  granit_mesh handle_{GRANIT_NULL_HANDLE};
};

struct mesh_vertex_buffer {
  buffer_ref buffer;
  std::uint64_t offset{};
  vertex_buffer_layout layout;
};

struct mesh_desc {
  primitive_topology topology{primitive_topology::triangle_list};
  std::span<const mesh_vertex_buffer> vertex_buffers;
  buffer_ref index_buffer;
  std::uint64_t index_buffer_offset{};
  index_type index_format{index_type::uint16};
  std::uint32_t vertex_count{};
  std::uint32_t index_count{};
  std::uint32_t instance_count{1};
  std::uint32_t first_vertex{};
  std::uint32_t first_index{};
  std::int32_t vertex_offset{};
  std::uint32_t first_instance{};
};

/** 公共 Mesh C ABI 的轻量 move-only RAII 包装。 */
class mesh {
public:
  mesh() = default;
  ~mesh() { static_cast<void>(reset()); }
  mesh(const mesh&) = delete;
  mesh& operator=(const mesh&) = delete;
  mesh(mesh&& other) noexcept
      : renderer_(std::exchange(other.renderer_, GRANIT_NULL_HANDLE)),
        handle_(std::exchange(other.handle_, GRANIT_NULL_HANDLE)) {}
  mesh& operator=(mesh&& other) noexcept {
    if (this != &other) {
      static_cast<void>(reset());
      renderer_ = std::exchange(other.renderer_, GRANIT_NULL_HANDLE);
      handle_ = std::exchange(other.handle_, GRANIT_NULL_HANDLE);
    }
    return *this;
  }

  [[nodiscard]] result initialize(renderer_ref owner, const granit_mesh_desc& desc) noexcept {
    const auto renderer = owner.native_handle();
    if (valid())
      return result::invalid_argument;
    const auto value = from_native(granit_mesh_create(renderer, &desc, &handle_));
    if (value.ok())
      renderer_ = renderer;
    return value;
  }
  [[nodiscard]] result initialize(renderer& owner, const mesh_desc& desc) noexcept {
    if (desc.vertex_buffers.size() > std::numeric_limits<std::uint32_t>::max())
      return result::invalid_argument;
    try {
      std::vector<std::vector<granit_vertex_attribute>> attributes;
      std::vector<granit_mesh_vertex_buffer> vertex_buffers;
      attributes.reserve(desc.vertex_buffers.size());
      vertex_buffers.reserve(desc.vertex_buffers.size());
      for (const auto& binding : desc.vertex_buffers) {
        if (binding.layout.attributes.size() > std::numeric_limits<std::uint32_t>::max())
          return result::invalid_argument;
        auto& native_attributes = attributes.emplace_back();
        native_attributes.reserve(binding.layout.attributes.size());
        for (const auto& attribute : binding.layout.attributes) {
          native_attributes.push_back(
              {.location = attribute.location,
               .format = static_cast<granit_vertex_format>(attribute.format),
               .offset = attribute.offset,
               .reserved = 0});
        }
        vertex_buffers.push_back(
            {.buffer = binding.buffer.native_handle(),
             .offset = binding.offset,
             .layout = {.stride = binding.layout.stride,
                        .step_mode = static_cast<granit_vertex_step_mode>(binding.layout.step_mode),
                        .attribute_count = static_cast<std::uint32_t>(native_attributes.size()),
                        .reserved = 0,
                        .attributes = native_attributes.data()}});
      }
      const granit_mesh_desc native{
          .struct_size = GRANIT_MESH_DESC_VERSION_1_SIZE,
          .topology = static_cast<granit_primitive_topology>(desc.topology),
          .vertex_buffers = vertex_buffers.data(),
          .vertex_buffer_count = static_cast<std::uint32_t>(vertex_buffers.size()),
          .indexed = desc.index_buffer.valid() ? 1U : 0U,
          .index_buffer = desc.index_buffer.native_handle(),
          .index_buffer_offset = desc.index_buffer_offset,
          .index_type = static_cast<granit_index_type>(desc.index_format),
          .vertex_count = desc.vertex_count,
          .index_count = desc.index_count,
          .instance_count = desc.instance_count,
          .first_vertex = desc.first_vertex,
          .first_index = desc.first_index,
          .vertex_offset = desc.vertex_offset,
          .first_instance = desc.first_instance,
          .reserved = 0,
      };
      return initialize(owner.ref(), native);
    } catch (const std::bad_alloc&) {
      return result::out_of_memory;
    } catch (...) {
      return result::internal;
    }
  }
  [[nodiscard]] result reset() noexcept {
    if (!valid())
      return result::success;
    const auto renderer = std::exchange(renderer_, GRANIT_NULL_HANDLE);
    const auto handle = std::exchange(handle_, GRANIT_NULL_HANDLE);
    return from_native(granit_mesh_destroy(renderer, handle));
  }
  [[nodiscard]] bool valid() const noexcept { return handle_ != GRANIT_NULL_HANDLE; }
  [[nodiscard]] constexpr mesh_ref ref() const noexcept { return mesh_ref{handle_}; }
  [[nodiscard]] granit_mesh native_handle() const noexcept { return handle_; }

private:
  granit_renderer renderer_ = GRANIT_NULL_HANDLE;
  granit_mesh handle_ = GRANIT_NULL_HANDLE;
};

} // namespace granit

#endif
