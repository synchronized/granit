// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "gltf/document_manifest.h"

#include "assets/resource_path.h"

#include <cgltf.h>

#include <algorithm>
#include <memory>
#include <new>
#include <string_view>
#include <utility>

namespace granit::example::gltf {
namespace {

using data_owner = std::unique_ptr<cgltf_data, decltype(&cgltf_free)>;

document_manifest_result failure(document_manifest_error error, const char* diagnostic) {
  return {error, diagnostic};
}

document_manifest_result map_parse_error(cgltf_result result) {
  switch (result) {
  case cgltf_result_data_too_short:
    return failure(document_manifest_error::truncated_data, "glTF 文档被截断");
  case cgltf_result_out_of_memory:
    return failure(document_manifest_error::out_of_memory, "解析 glTF 文档清单时内存不足");
  default:
    return failure(document_manifest_error::invalid_document, "glTF 文档格式无效");
  }
}

document_manifest_result append_external_uri(const char* uri, std::vector<std::string>& resources) {
  if (uri == nullptr || std::string_view{uri}.starts_with("data:"))
    return {};
  std::string normalized;
  if (!assets::normalize_resource_path(uri, normalized)) {
    return failure(document_manifest_error::invalid_resource_uri, "glTF 外部资源 URI 不安全");
  }
  if (std::ranges::find(resources, normalized) == resources.end())
    resources.push_back(std::move(normalized));
  return {};
}

} // namespace

document_manifest_result discover_external_resources(std::span<const std::byte> document,
                                                     std::vector<std::string>& output) {
  if (document.empty())
    return failure(document_manifest_error::truncated_data, "glTF 文档为空");

  cgltf_options options{};
  cgltf_data* raw_data = nullptr;
  const auto parse_result = cgltf_parse(&options, document.data(), document.size(), &raw_data);
  if (parse_result != cgltf_result_success)
    return map_parse_error(parse_result);
  data_owner data(raw_data, &cgltf_free);

  try {
    std::vector<std::string> candidate;
    candidate.reserve(data->buffers_count + data->images_count);
    for (cgltf_size index = 0; index < data->buffers_count; ++index) {
      if (auto result = append_external_uri(data->buffers[index].uri, candidate); !result)
        return result;
    }
    for (cgltf_size index = 0; index < data->images_count; ++index) {
      if (auto result = append_external_uri(data->images[index].uri, candidate); !result)
        return result;
    }
    output = std::move(candidate);
    return {};
  } catch (const std::bad_alloc&) {
    return failure(document_manifest_error::out_of_memory, "发现 glTF 外部资源时内存不足");
  }
}

} // namespace granit::example::gltf
