// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "gltf/loader.h"

#include "gltf/resource_uri.h"

#define CGLTF_IMPLEMENTATION
#include <cgltf.h>

#include <algorithm>
#include <cstring>
#include <limits>
#include <memory>
#include <new>
#include <utility>

namespace granit::example::gltf {
namespace {

using data_owner = std::unique_ptr<cgltf_data, decltype(&cgltf_free)>;

load_result failure(load_error error, const char* diagnostic) { return {error, diagnostic}; }

load_result map_parse_error(cgltf_result result) {
  switch (result) {
  case cgltf_result_data_too_short:
    return failure(load_error::truncated_data, "glTF 文档被截断");
  case cgltf_result_out_of_memory:
    return failure(load_error::out_of_memory, "解析 glTF 时内存不足");
  default:
    return failure(load_error::invalid_document, "glTF 文档格式无效");
  }
}

bool to_index(const void* pointer, const void* base, std::size_t count, std::size_t stride,
              std::uint32_t& output) {
  if (pointer == nullptr)
    return false;
  const auto offset = static_cast<const std::byte*>(pointer) - static_cast<const std::byte*>(base);
  if (offset < 0 || offset % static_cast<std::ptrdiff_t>(stride) != 0)
    return false;
  const auto index = static_cast<std::size_t>(offset) / stride;
  if (index >= count || index > std::numeric_limits<std::uint32_t>::max())
    return false;
  output = static_cast<std::uint32_t>(index);
  return true;
}

load_result load_external_buffers(cgltf_data& data, const resource_resolver* resolver,
                                  std::vector<std::vector<std::byte>>& storage) {
  storage.reserve(data.buffers_count);
  for (cgltf_size index = 0; index < data.buffers_count; ++index) {
    auto& buffer = data.buffers[index];
    if (buffer.data != nullptr)
      continue;
    if (buffer.uri == nullptr || resolver == nullptr)
      return failure(load_error::missing_resource, "glTF 外部 Buffer 缺失");
    std::string path;
    if (!normalize_resource_uri(buffer.uri, path))
      return failure(load_error::invalid_resource_uri, "glTF Buffer URI 不安全");
    storage.emplace_back();
    if (!resolver->resolve(path, storage.back()))
      return failure(load_error::missing_resource, "无法解析 glTF 外部 Buffer");
    if (storage.back().size() < buffer.size)
      return failure(load_error::truncated_data, "glTF 外部 Buffer 被截断");
    buffer.data = storage.back().data();
  }
  return {};
}

const cgltf_accessor* find_attribute(const cgltf_primitive& primitive, cgltf_attribute_type type,
                                     cgltf_int set = 0) {
  for (cgltf_size index = 0; index < primitive.attributes_count; ++index) {
    const auto& attribute = primitive.attributes[index];
    if (attribute.type == type && attribute.index == set)
      return attribute.data;
  }
  return nullptr;
}

bool supported_float_accessor(const cgltf_accessor& accessor, cgltf_type type,
                              cgltf_attribute_type semantic) {
  if (accessor.type != type || accessor.is_sparse || accessor.buffer_view == nullptr)
    return false;
  if (semantic == cgltf_attribute_type_position)
    return accessor.component_type == cgltf_component_type_r_32f && !accessor.normalized;
  if (semantic == cgltf_attribute_type_normal || semantic == cgltf_attribute_type_tangent) {
    return accessor.component_type == cgltf_component_type_r_32f ||
           ((accessor.component_type == cgltf_component_type_r_8 ||
             accessor.component_type == cgltf_component_type_r_16) &&
            accessor.normalized);
  }
  return accessor.component_type == cgltf_component_type_r_32f ||
         ((accessor.component_type == cgltf_component_type_r_8u ||
           accessor.component_type == cgltf_component_type_r_16u) &&
          accessor.normalized);
}

template <typename Value, std::size_t Components>
bool read_float_accessor(const cgltf_accessor& accessor, std::vector<Value>& values) {
  values.resize(accessor.count);
  for (cgltf_size index = 0; index < accessor.count; ++index) {
    float components[Components]{};
    if (!cgltf_accessor_read_float(&accessor, index, components, Components))
      return false;
    std::memcpy(&values[index], components, sizeof(components));
  }
  return true;
}

load_result convert_primitive(const cgltf_data& data, const cgltf_primitive& source,
                              primitive& target) {
  if (source.type != cgltf_primitive_type_triangles || source.targets_count != 0 ||
      source.has_draco_mesh_compression)
    return failure(load_error::unsupported_feature, "仅支持未压缩的 Triangle Primitive");
  for (cgltf_size index = 0; index < source.attributes_count; ++index) {
    const auto& attribute = source.attributes[index];
    const bool supported =
        attribute.type == cgltf_attribute_type_position ||
        attribute.type == cgltf_attribute_type_normal ||
        attribute.type == cgltf_attribute_type_tangent ||
        (attribute.type == cgltf_attribute_type_texcoord && attribute.index == 0);
    if (!supported)
      return failure(load_error::unsupported_feature, "Primitive 包含首版不支持的顶点语义");
  }

  const auto* positions = find_attribute(source, cgltf_attribute_type_position);
  const auto* normals = find_attribute(source, cgltf_attribute_type_normal);
  if (positions == nullptr || normals == nullptr)
    return failure(load_error::unsupported_feature, "Primitive 必须包含 Position 与 Normal");
  if (!supported_float_accessor(*positions, cgltf_type_vec3, cgltf_attribute_type_position) ||
      !supported_float_accessor(*normals, cgltf_type_vec3, cgltf_attribute_type_normal) ||
      positions->count != normals->count)
    return failure(load_error::unsupported_feature, "Position 或 Normal Accessor 格式不受支持");
  if (!read_float_accessor<math::float3, 3>(*positions, target.positions) ||
      !read_float_accessor<math::float3, 3>(*normals, target.normals))
    return failure(load_error::accessor_out_of_bounds, "读取 Position 或 Normal Accessor 失败");

  if (const auto* tangents = find_attribute(source, cgltf_attribute_type_tangent)) {
    if (tangents->count != positions->count ||
        !supported_float_accessor(*tangents, cgltf_type_vec4, cgltf_attribute_type_tangent) ||
        !read_float_accessor<math::float4, 4>(*tangents, target.tangents))
      return failure(load_error::unsupported_feature, "Tangent Accessor 格式不受支持");
  }
  if (const auto* coordinates = find_attribute(source, cgltf_attribute_type_texcoord)) {
    if (coordinates->count != positions->count ||
        !supported_float_accessor(*coordinates, cgltf_type_vec2, cgltf_attribute_type_texcoord) ||
        !read_float_accessor<math::float2, 2>(*coordinates, target.texture_coordinates))
      return failure(load_error::unsupported_feature, "UV0 Accessor 格式不受支持");
  }

  if (positions->count > std::numeric_limits<std::uint32_t>::max())
    return failure(load_error::numeric_overflow, "Primitive 顶点数量超过 uint32 范围");
  if (source.indices != nullptr) {
    const auto& indices = *source.indices;
    const bool supported = indices.type == cgltf_type_scalar && !indices.is_sparse &&
                           !indices.normalized && indices.buffer_view != nullptr &&
                           (indices.component_type == cgltf_component_type_r_8u ||
                            indices.component_type == cgltf_component_type_r_16u ||
                            indices.component_type == cgltf_component_type_r_32u);
    if (!supported)
      return failure(load_error::unsupported_feature, "Index Accessor 格式不受支持");
    target.indices.reserve(indices.count);
    for (cgltf_size index = 0; index < indices.count; ++index) {
      const auto value = cgltf_accessor_read_index(&indices, index);
      if (value >= positions->count || value > std::numeric_limits<std::uint32_t>::max())
        return failure(load_error::accessor_out_of_bounds, "Primitive 索引超出顶点范围");
      target.indices.push_back(static_cast<std::uint32_t>(value));
    }
  } else {
    target.indices.reserve(positions->count);
    for (cgltf_size index = 0; index < positions->count; ++index)
      target.indices.push_back(static_cast<std::uint32_t>(index));
  }

  if (source.material != nullptr &&
      !to_index(source.material, data.materials, data.materials_count, sizeof(cgltf_material),
                target.material))
    return failure(load_error::invalid_document, "Primitive Material 索引无效");
  for (const auto position : target.positions) {
    if (!target.local_bounds.valid) {
      target.local_bounds = {.minimum = position, .maximum = position, .valid = true};
      continue;
    }
    target.local_bounds.minimum.x = std::min(target.local_bounds.minimum.x, position.x);
    target.local_bounds.minimum.y = std::min(target.local_bounds.minimum.y, position.y);
    target.local_bounds.minimum.z = std::min(target.local_bounds.minimum.z, position.z);
    target.local_bounds.maximum.x = std::max(target.local_bounds.maximum.x, position.x);
    target.local_bounds.maximum.y = std::max(target.local_bounds.maximum.y, position.y);
    target.local_bounds.maximum.z = std::max(target.local_bounds.maximum.z, position.z);
  }
  return {};
}

load_result convert_meshes(const cgltf_data& data, scene& output) {
  output.meshes.reserve(data.meshes_count);
  for (cgltf_size mesh_index = 0; mesh_index < data.meshes_count; ++mesh_index) {
    const auto& source = data.meshes[mesh_index];
    if (source.weights_count != 0 || source.target_names_count != 0)
      return failure(load_error::unsupported_feature, "首版加载器不支持 Morph Target");
    mesh target;
    if (source.name != nullptr)
      target.name = source.name;
    target.primitives.reserve(source.primitives_count);
    for (cgltf_size primitive_index = 0; primitive_index < source.primitives_count;
         ++primitive_index) {
      target.primitives.emplace_back();
      if (auto result = convert_primitive(data, source.primitives[primitive_index],
                                          target.primitives.back());
          !result)
        return result;
    }
    output.meshes.push_back(std::move(target));
  }
  return {};
}

load_result convert_nodes(const cgltf_data& data, scene& output) {
  if (data.nodes_count > std::numeric_limits<std::uint32_t>::max())
    return failure(load_error::numeric_overflow, "glTF Node 数量超过索引范围");
  output.nodes.reserve(data.nodes_count);
  for (cgltf_size index = 0; index < data.nodes_count; ++index) {
    const auto& source = data.nodes[index];
    if (source.skin != nullptr || source.weights_count != 0)
      return failure(load_error::unsupported_feature, "首版加载器不支持 Skin 或 Morph Weight");
    node target;
    if (source.name != nullptr)
      target.name = source.name;
    if (source.parent != nullptr &&
        !to_index(source.parent, data.nodes, data.nodes_count, sizeof(cgltf_node), target.parent))
      return failure(load_error::invalid_document, "glTF Node 父索引无效");
    if (source.mesh != nullptr &&
        !to_index(source.mesh, data.meshes, data.meshes_count, sizeof(cgltf_mesh), target.mesh))
      return failure(load_error::invalid_document, "glTF Node Mesh 索引无效");
    target.children.reserve(source.children_count);
    for (cgltf_size child = 0; child < source.children_count; ++child) {
      std::uint32_t child_index{};
      if (!to_index(source.children[child], data.nodes, data.nodes_count, sizeof(cgltf_node),
                    child_index))
        return failure(load_error::invalid_document, "glTF 子 Node 索引无效");
      target.children.push_back(child_index);
    }
    cgltf_node_transform_local(&source, target.local_transform.elements);
    cgltf_node_transform_world(&source, target.world_transform.elements);
    output.nodes.push_back(std::move(target));
  }

  const auto* selected_scene = data.scene;
  if (selected_scene == nullptr && data.scenes_count != 0)
    selected_scene = &data.scenes[0];
  if (selected_scene != nullptr) {
    output.roots.reserve(selected_scene->nodes_count);
    for (cgltf_size root = 0; root < selected_scene->nodes_count; ++root) {
      std::uint32_t root_index{};
      if (!to_index(selected_scene->nodes[root], data.nodes, data.nodes_count, sizeof(cgltf_node),
                    root_index))
        return failure(load_error::invalid_document, "glTF Scene 根 Node 索引无效");
      output.roots.push_back(root_index);
    }
  }
  return {};
}

} // namespace

load_result load(std::span<const std::byte> document, const resource_resolver* resolver,
                 scene& output) {
  if (document.empty())
    return failure(load_error::truncated_data, "glTF 文档为空");
  try {
    cgltf_data* raw_data = nullptr;
    const cgltf_options options{};
    const auto parse_result = cgltf_parse(&options, document.data(), document.size(), &raw_data);
    if (parse_result != cgltf_result_success)
      return map_parse_error(parse_result);
    data_owner data(raw_data, &cgltf_free);

    std::vector<std::vector<std::byte>> external_buffers;
    if (auto result = load_external_buffers(*data, resolver, external_buffers); !result)
      return result;
    const auto validation = cgltf_validate(data.get());
    if (validation != cgltf_result_success)
      return map_parse_error(validation);
    if (data->animations_count != 0 || data->skins_count != 0)
      return failure(load_error::unsupported_feature, "首版加载器不支持 Animation 或 Skin");

    scene candidate;
    if (auto result = convert_meshes(*data, candidate); !result)
      return result;
    if (auto result = convert_nodes(*data, candidate); !result)
      return result;
    output = std::move(candidate);
    return {};
  } catch (const std::bad_alloc&) {
    return failure(load_error::out_of_memory, "加载 glTF 时内存不足");
  }
}

} // namespace granit::example::gltf
