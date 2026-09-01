// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "gltf/loader.h"

#include "gltf/image_decoder.h"
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
    if (buffer.uri == nullptr && index == 0 && data.bin != nullptr &&
        buffer.size <= data.bin_size) {
      buffer.data = const_cast<void*>(data.bin);
      continue;
    }
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

bool range_fits(std::size_t offset, std::size_t size, std::size_t available) {
  return offset <= available && size <= available - offset;
}

load_result validate_buffer_ranges(const cgltf_data& data) {
  for (cgltf_size index = 0; index < data.buffer_views_count; ++index) {
    const auto& view = data.buffer_views[index];
    if (view.buffer == nullptr || view.buffer->data == nullptr ||
        !range_fits(view.offset, view.size, view.buffer->size))
      return failure(load_error::accessor_out_of_bounds, "BufferView 超出 Buffer 范围");
  }
  for (cgltf_size index = 0; index < data.accessors_count; ++index) {
    const auto& accessor = data.accessors[index];
    if (accessor.buffer_view == nullptr || accessor.is_sparse)
      continue;
    const auto element_size = cgltf_calc_size(accessor.type, accessor.component_type);
    if (element_size == 0 || accessor.stride < element_size ||
        !range_fits(accessor.offset, element_size, accessor.buffer_view->size))
      return failure(load_error::accessor_out_of_bounds, "Accessor 起始范围无效");
    if (accessor.count > 1) {
      const auto remaining = accessor.buffer_view->size - accessor.offset - element_size;
      if (accessor.count - 1 > remaining / accessor.stride)
        return failure(load_error::accessor_out_of_bounds, "Accessor 末尾超出 BufferView");
    }
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

  if (source.material != nullptr && !to_index(source.material, data.materials, data.materials_count,
                                              sizeof(cgltf_material), target.material))
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
      if (auto result =
              convert_primitive(data, source.primitives[primitive_index], target.primitives.back());
          !result)
        return result;
    }
    output.meshes.push_back(std::move(target));
  }
  return {};
}

load_result convert_texture_reference(const cgltf_data& data, const cgltf_texture_view& source,
                                      texture_reference& target) {
  if (source.texture == nullptr)
    return {};
  if (source.texcoord != 0 || source.has_transform || source.texture->has_basisu ||
      source.texture->has_webp)
    return failure(load_error::unsupported_feature, "纹理变换、UV1、BasisU 或 WebP 暂不支持");
  if (!to_index(source.texture->image, data.images, data.images_count, sizeof(cgltf_image),
                target.image))
    return failure(load_error::invalid_document, "Texture Image 索引无效");
  if (source.texture->sampler != nullptr &&
      !to_index(source.texture->sampler, data.samplers, data.samplers_count, sizeof(cgltf_sampler),
                target.sampler))
    return failure(load_error::invalid_document, "Texture Sampler 索引无效");
  return {};
}

load_result convert_materials(const cgltf_data& data, scene& output) {
  output.materials.reserve(data.materials_count);
  for (cgltf_size index = 0; index < data.materials_count; ++index) {
    const auto& source = data.materials[index];
    if (source.has_pbr_specular_glossiness || source.has_clearcoat || source.has_transmission ||
        source.has_volume || source.has_sheen || source.has_iridescence ||
        source.has_diffuse_transmission || source.has_anisotropy || source.has_dispersion)
      return failure(load_error::unsupported_feature, "Material 使用了首版不支持的 PBR 扩展");
    material target;
    if (source.name != nullptr)
      target.name = source.name;
    if (source.has_pbr_metallic_roughness) {
      const auto& pbr = source.pbr_metallic_roughness;
      target.base_color = {pbr.base_color_factor[0], pbr.base_color_factor[1],
                           pbr.base_color_factor[2], pbr.base_color_factor[3]};
      target.metallic = pbr.metallic_factor;
      target.roughness = pbr.roughness_factor;
      if (auto result =
              convert_texture_reference(data, pbr.base_color_texture, target.base_color_texture);
          !result)
        return result;
      if (auto result = convert_texture_reference(data, pbr.metallic_roughness_texture,
                                                  target.metallic_roughness_texture);
          !result)
        return result;
    }
    target.normal_scale = source.normal_texture.scale;
    target.occlusion_strength = source.occlusion_texture.scale;
    target.emissive = {source.emissive_factor[0], source.emissive_factor[1],
                       source.emissive_factor[2]};
    if (source.has_emissive_strength) {
      target.emissive.x *= source.emissive_strength.emissive_strength;
      target.emissive.y *= source.emissive_strength.emissive_strength;
      target.emissive.z *= source.emissive_strength.emissive_strength;
    }
    if (auto result = convert_texture_reference(data, source.normal_texture, target.normal_texture);
        !result)
      return result;
    if (auto result =
            convert_texture_reference(data, source.occlusion_texture, target.occlusion_texture);
        !result)
      return result;
    if (auto result =
            convert_texture_reference(data, source.emissive_texture, target.emissive_texture);
        !result)
      return result;
    output.materials.push_back(std::move(target));
  }
  return {};
}

load_result convert_samplers(const cgltf_data& data, scene& output) {
  output.samplers.reserve(data.samplers_count);
  for (cgltf_size index = 0; index < data.samplers_count; ++index) {
    const auto& source = data.samplers[index];
    output.samplers.push_back({.mag_filter = static_cast<std::uint32_t>(source.mag_filter),
                               .min_filter = static_cast<std::uint32_t>(source.min_filter),
                               .wrap_u = static_cast<std::uint32_t>(source.wrap_s),
                               .wrap_v = static_cast<std::uint32_t>(source.wrap_t)});
  }
  return {};
}

load_result convert_images(const cgltf_data& data, const resource_resolver* resolver,
                           scene& output) {
  output.images.reserve(data.images_count);
  for (cgltf_size index = 0; index < data.images_count; ++index) {
    const auto& source = data.images[index];
    std::vector<std::byte> owned_bytes;
    std::span<const std::byte> encoded;
    if (source.uri != nullptr) {
      if (resolver == nullptr)
        return failure(load_error::missing_resource, "glTF 外部 Image 缺失");
      std::string path;
      if (!normalize_resource_uri(source.uri, path))
        return failure(load_error::invalid_resource_uri, "glTF Image URI 不安全");
      if (!resolver->resolve(path, owned_bytes))
        return failure(load_error::missing_resource, "无法解析 glTF 外部 Image");
      encoded = owned_bytes;
    } else if (source.buffer_view != nullptr) {
      const auto* data_bytes =
          reinterpret_cast<const std::byte*>(cgltf_buffer_view_data(source.buffer_view));
      if (data_bytes == nullptr)
        return failure(load_error::accessor_out_of_bounds, "Image BufferView 无法访问");
      encoded = {data_bytes, source.buffer_view->size};
    } else {
      return failure(load_error::missing_resource, "Image 没有 URI 或 BufferView");
    }

    image target;
    if (source.name != nullptr)
      target.name = source.name;
    const auto decode_result = decode_image(encoded, target);
    if (decode_result == image_decode_error::numeric_overflow)
      return failure(load_error::numeric_overflow, "Image 尺寸溢出");
    if (decode_result == image_decode_error::out_of_memory)
      return failure(load_error::out_of_memory, "解码 Image 时内存不足");
    if (decode_result != image_decode_error::none)
      return failure(load_error::image_decode_failed, "Image 不是有效的 PNG/JPEG");
    if (source.name != nullptr)
      target.name = source.name;
    output.images.push_back(std::move(target));
  }
  return {};
}

bool has_texture(const texture_reference& reference) { return reference.image != invalid_index; }

load_result validate_material_inputs(const scene& output) {
  for (const auto& mesh : output.meshes) {
    for (const auto& primitive : mesh.primitives) {
      if (primitive.material == invalid_index)
        continue;
      if (primitive.material >= output.materials.size())
        return failure(load_error::invalid_document, "Primitive Material 超出 Scene 范围");
      const auto& material = output.materials[primitive.material];
      const bool uses_texture = has_texture(material.base_color_texture) ||
                                has_texture(material.metallic_roughness_texture) ||
                                has_texture(material.normal_texture) ||
                                has_texture(material.occlusion_texture) ||
                                has_texture(material.emissive_texture);
      if (uses_texture && primitive.texture_coordinates.empty())
        return failure(load_error::unsupported_feature, "使用纹理的 Primitive 必须包含 UV0");
      if (has_texture(material.normal_texture) && primitive.tangents.empty())
        return failure(load_error::unsupported_feature,
                       "使用 Normal Texture 的 Primitive 必须包含 Tangent");
    }
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
    if (auto result = validate_buffer_ranges(*data); !result)
      return result;
    const auto validation = cgltf_validate(data.get());
    if (validation != cgltf_result_success)
      return map_parse_error(validation);
    if (data->animations_count != 0 || data->skins_count != 0)
      return failure(load_error::unsupported_feature, "首版加载器不支持 Animation 或 Skin");

    scene candidate;
    if (auto result = convert_samplers(*data, candidate); !result)
      return result;
    if (auto result = convert_images(*data, resolver, candidate); !result)
      return result;
    if (auto result = convert_materials(*data, candidate); !result)
      return result;
    if (auto result = convert_meshes(*data, candidate); !result)
      return result;
    if (auto result = convert_nodes(*data, candidate); !result)
      return result;
    if (auto result = validate_material_inputs(candidate); !result)
      return result;
    output = std::move(candidate);
    return {};
  } catch (const std::bad_alloc&) {
    return failure(load_error::out_of_memory, "加载 glTF 时内存不足");
  }
}

} // namespace granit::example::gltf
