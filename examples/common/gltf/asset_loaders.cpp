// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "gltf/asset_loaders.h"

#include "assets/asset_manager.h"
#include "assets/memory_resource_resolver.h"
#include "gltf/document_manifest.h"
#include "gltf/image_decoder.h"
#include "gltf/importer.h"
#include "gltf/scene.h"

#include <memory>
#include <new>
#include <typeindex>

namespace granit::example::gltf {
namespace {

assets::asset_error map_manifest_error(document_manifest_error error) noexcept {
  switch (error) {
  case document_manifest_error::none:
    return assets::asset_error::none;
  case document_manifest_error::out_of_memory:
    return assets::asset_error::out_of_memory;
  case document_manifest_error::invalid_document:
  case document_manifest_error::truncated_data:
  case document_manifest_error::invalid_resource_uri:
    return assets::asset_error::invalid_data;
  }
  return assets::asset_error::internal;
}

assets::asset_error map_import_error(import_error error) noexcept {
  switch (error) {
  case import_error::none:
    return assets::asset_error::none;
  case import_error::out_of_memory:
    return assets::asset_error::out_of_memory;
  case import_error::cancelled:
    return assets::asset_error::cancelled;
  case import_error::invalid_document:
  case import_error::truncated_data:
  case import_error::invalid_resource_uri:
  case import_error::missing_resource:
  case import_error::accessor_out_of_bounds:
  case import_error::unsupported_feature:
  case import_error::image_decode_failed:
  case import_error::numeric_overflow:
    return assets::asset_error::invalid_data;
  }
  return assets::asset_error::internal;
}

class image_asset_loader final : public assets::asset_loader {
public:
  [[nodiscard]] std::type_index target_type() const noexcept override { return typeid(image); }

  [[nodiscard]] bool accepts(const assets::asset_location&,
                             std::span<const std::byte> bytes) const noexcept override {
    return !bytes.empty();
  }

  [[nodiscard]] assets::asset_decode_result
  decode(const assets::asset_location&, std::span<const std::byte> bytes,
         std::span<const assets::asset_dependency_data>) noexcept override {
    try {
      auto output = std::make_shared<image>();
      const auto result = decode_image(bytes, *output);
      if (result == image_decode_error::none)
        return {.error = assets::asset_error::none, .value = std::move(output), .diagnostic = {}};
      return {.error = result == image_decode_error::out_of_memory
                           ? assets::asset_error::out_of_memory
                           : assets::asset_error::invalid_data,
              .value = {},
              .diagnostic = "图片解码失败"};
    } catch (const std::bad_alloc&) {
      return {.error = assets::asset_error::out_of_memory,
              .value = {},
              .diagnostic = "无法分配图片资产"};
    } catch (...) {
      return {.error = assets::asset_error::internal,
              .value = {},
              .diagnostic = "图片 Loader 发生内部错误"};
    }
  }
};

class scene_asset_loader final : public assets::asset_loader {
public:
  [[nodiscard]] std::type_index target_type() const noexcept override { return typeid(scene); }

  [[nodiscard]] bool accepts(const assets::asset_location&,
                             std::span<const std::byte> bytes) const noexcept override {
    return !bytes.empty();
  }

  [[nodiscard]] assets::asset_discovery_result
  discover_dependencies(const assets::asset_location&,
                        std::span<const std::byte> bytes) noexcept override {
    std::vector<std::string> dependencies;
    const auto result = discover_external_resources(bytes, dependencies);
    return {.error = map_manifest_error(result.error),
            .dependencies = std::move(dependencies),
            .diagnostic = std::move(result.diagnostic)};
  }

  [[nodiscard]] assets::asset_decode_result
  decode(const assets::asset_location&, std::span<const std::byte> bytes,
         std::span<const assets::asset_dependency_data> dependencies) noexcept override {
    try {
      assets::memory_resource_resolver resolver;
      for (const auto& dependency : dependencies) {
        if (!resolver.insert(dependency.uri, dependency.bytes)) {
          return {.error = assets::asset_error::invalid_data,
                  .value = {},
                  .diagnostic = "glTF 依赖 URI 重复或无效"};
        }
      }
      auto output = std::make_shared<scene>();
      const auto result = import_scene(bytes, dependencies.empty() ? nullptr : &resolver, *output);
      if (result)
        return {.error = assets::asset_error::none, .value = std::move(output), .diagnostic = {}};
      return {.error = map_import_error(result.error),
              .value = {},
              .diagnostic = std::move(result.diagnostic)};
    } catch (const std::bad_alloc&) {
      return {.error = assets::asset_error::out_of_memory,
              .value = {},
              .diagnostic = "无法分配 glTF Scene"};
    } catch (...) {
      return {.error = assets::asset_error::internal,
              .value = {},
              .diagnostic = "glTF Loader 发生内部错误"};
    }
  }
};

} // namespace

granit::result register_standard_asset_loaders(assets::asset_manager& manager) noexcept {
  try {
    if (const auto result = manager.register_loader(std::make_shared<image_asset_loader>());
        result.failed())
      return result;
    return manager.register_loader(std::make_shared<scene_asset_loader>());
  } catch (const std::bad_alloc&) {
    return granit::result::out_of_memory;
  } catch (...) {
    return granit::result::internal;
  }
}

} // namespace granit::example::gltf
