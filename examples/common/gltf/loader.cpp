// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "gltf/loader.h"

#include "gltf/resource_uri.h"

#define CGLTF_IMPLEMENTATION
#include <cgltf.h>

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
    if (auto result = convert_nodes(*data, candidate); !result)
      return result;
    output = std::move(candidate);
    return {};
  } catch (const std::bad_alloc&) {
    return failure(load_error::out_of_memory, "加载 glTF 时内存不足");
  }
}

} // namespace granit::example::gltf
