// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <catch2/catch_all.hpp>
#include <granit/renderer/renderer.hpp>
#include <granit/renderer/texture_asset.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace {

void write_u32(std::span<std::byte> bytes, size_t offset, uint32_t value) {
  for (uint32_t index = 0; index < 4; ++index)
    bytes[offset + index] = static_cast<std::byte>(value >> (index * 8U));
}

void write_u64(std::span<std::byte> bytes, size_t offset, uint64_t value) {
  for (uint32_t index = 0; index < 8; ++index)
    bytes[offset + index] = static_cast<std::byte>(value >> (index * 8U));
}

std::vector<std::byte> make_manifest() {
  constexpr size_t header_size = 80;
  constexpr size_t variant_size = 72;
  constexpr size_t subresource_size = 40;
  std::vector<std::byte> bytes(header_size + variant_size + subresource_size);
  constexpr std::array magic{std::byte{'G'}, std::byte{'R'}, std::byte{'N'}, std::byte{'T'},
                             std::byte{'E'}, std::byte{'X'}, std::byte{'A'}, std::byte{0}};
  std::ranges::copy(magic, bytes.begin());
  write_u32(bytes, 8, 1);
  write_u32(bytes, 12, header_size);
  write_u32(bytes, 16, 4);
  write_u32(bytes, 20, 4);
  write_u32(bytes, 24, 1);
  write_u32(bytes, 28, 1);
  write_u32(bytes, 32, 1);
  write_u32(bytes, 36, GRANIT_TEXTURE_DIMENSION_2D);
  write_u32(bytes, 40, 1);
  write_u32(bytes, 44, 1);
  bytes[48] = std::byte{1};

  write_u32(bytes, header_size, GRANIT_TEXTURE_FORMAT_RGBA8_SRGB);
  write_u32(bytes, header_size + 4,
            GRANIT_TEXTURE_USAGE_SAMPLED_BIT | GRANIT_TEXTURE_USAGE_TRANSFER_DESTINATION_BIT);
  write_u32(bytes, header_size + 8, 0);
  write_u32(bytes, header_size + 12, 1);
  write_u64(bytes, header_size + 16, 0);
  write_u64(bytes, header_size + 24, 64);
  constexpr std::array<uint8_t, 32> zero_payload_sha256{
      0xf5, 0xa5, 0xfd, 0x42, 0xd1, 0x6a, 0x20, 0x30, 0x27, 0x98, 0xef,
      0x6e, 0xd3, 0x09, 0x97, 0x9b, 0x43, 0x00, 0x3d, 0x23, 0x20, 0xd9,
      0xf0, 0xe8, 0xea, 0x98, 0x31, 0xa9, 0x27, 0x59, 0xfb, 0x4b};
  for (size_t index = 0; index < zero_payload_sha256.size(); ++index)
    bytes[header_size + 32 + index] = static_cast<std::byte>(zero_payload_sha256[index]);

  const auto subresource = header_size + variant_size;
  write_u32(bytes, subresource, 0);
  write_u32(bytes, subresource + 4, 0);
  write_u64(bytes, subresource + 8, 0);
  write_u64(bytes, subresource + 16, 64);
  write_u32(bytes, subresource + 24, 16);
  write_u32(bytes, subresource + 28, 4);
  return bytes;
}

bool unavailable(granit::result value) {
  return value == granit::result::backend_unavailable ||
         value == granit::result::incompatible_driver ||
         value == granit::result::no_suitable_device || value == granit::result::unsupported;
}

TEST_CASE("Texture Asset检查返回变体与子资源", "[texture_asset][inspect]") {
  const auto manifest = make_manifest();
  granit_texture_asset_info native = GRANIT_TEXTURE_ASSET_INFO_INIT;
  REQUIRE(granit_texture_asset_inspect(manifest.data(), manifest.size(), &native) ==
          GRANIT_SUCCESS);
  REQUIRE(native.variant_count == 1);
  REQUIRE(native.subresource_count == 1);
  granit_texture_asset_variant_info variant{};
  granit_texture_asset_subresource_info subresource{};
  native.variants = &variant;
  native.variant_capacity = 1;
  native.subresources = &subresource;
  native.subresource_capacity = 1;
  REQUIRE(granit_texture_asset_inspect(manifest.data(), manifest.size(), &native) ==
          GRANIT_SUCCESS);
  CHECK(native.width == 4);
  CHECK(native.height == 4);
  CHECK(variant.format == GRANIT_TEXTURE_FORMAT_RGBA8_SRGB);
  CHECK(subresource.data_size == 64);

  granit::texture_asset_info cpp_info;
  REQUIRE(granit::inspect_texture_asset(manifest, cpp_info) == granit::result::success);
  CHECK(cpp_info.variants.size() == 1);
  CHECK(cpp_info.subresources.size() == 1);
}

TEST_CASE("Texture Asset编码结果可确定性往返", "[texture_asset][encode]") {
  const auto source = make_manifest();
  granit::texture_asset_info info;
  REQUIRE(granit::inspect_texture_asset(source, info) == granit::result::success);
  std::vector<std::byte> first;
  std::vector<std::byte> second;
  REQUIRE(granit::encode_texture_asset(info, first) == granit::result::success);
  REQUIRE(granit::encode_texture_asset(info, second) == granit::result::success);
  CHECK(first == source);
  CHECK(second == first);

  granit_texture_asset_info native = GRANIT_TEXTURE_ASSET_INFO_INIT;
  std::uint64_t required_size = 1;
  CHECK(granit_texture_asset_encode(&native, nullptr, &required_size) ==
        GRANIT_ERROR_INVALID_ARGUMENT);
}

TEST_CASE("Texture Asset检查拒绝损坏布局和未知版本", "[texture_asset][validation]") {
  auto manifest = make_manifest();
  granit_texture_asset_info info = GRANIT_TEXTURE_ASSET_INFO_INIT;
  write_u32(manifest, 8, 2);
  CHECK(granit_texture_asset_inspect(manifest.data(), manifest.size(), &info) ==
        GRANIT_ERROR_UNSUPPORTED);
  write_u32(manifest, 8, 1);
  write_u64(manifest, 80 + 72 + 16, 63);
  CHECK(granit_texture_asset_inspect(manifest.data(), manifest.size(), &info) ==
        GRANIT_ERROR_INVALID_ARGUMENT);
}

TEST_CASE("Texture Asset按Renderer能力选择首个兼容变体", "[texture_asset][selection]") {
  granit::renderer renderer;
  const auto initialized = renderer.initialize();
  if (unavailable(initialized)) {
    WARN("当前环境没有可用 Renderer，跳过真实设备选择测试");
    return;
  }
  REQUIRE(initialized == granit::result::success);
  const auto manifest = make_manifest();
  granit::texture_asset_selection selection;
  REQUIRE(granit::select_texture_asset_variant(renderer.native_handle(), manifest, selection) ==
          granit::result::success);
  CHECK(selection.variant_index == 0);
  CHECK(selection.format == GRANIT_TEXTURE_FORMAT_RGBA8_SRGB);

  granit::texture_asset_selection_options storage;
  storage.required_usage = GRANIT_TEXTURE_USAGE_STORAGE_BIT;
  CHECK(granit::select_texture_asset_variant(renderer.native_handle(), manifest, selection,
                                             storage) == granit::result::unsupported);
}

TEST_CASE("Texture Asset逐Mip接口复用Upload Batch", "[texture_asset][upload]") {
  granit::renderer renderer;
  const auto initialized = renderer.initialize();
  if (unavailable(initialized)) {
    WARN("当前环境没有可用 Renderer，跳过真实上传测试");
    return;
  }
  REQUIRE(initialized == granit::result::success);
  const auto manifest = make_manifest();
  constexpr std::array<std::byte, 64> payload{};

  granit_texture_desc texture_desc = GRANIT_TEXTURE_DESC_INIT;
  texture_desc.format = GRANIT_TEXTURE_FORMAT_RGBA8_SRGB;
  texture_desc.usage = GRANIT_TEXTURE_USAGE_SAMPLED_BIT |
                       GRANIT_TEXTURE_USAGE_TRANSFER_DESTINATION_BIT |
                       GRANIT_TEXTURE_USAGE_TRANSFER_SOURCE_BIT;
  texture_desc.width = 4;
  texture_desc.height = 4;
  granit_texture texture = GRANIT_NULL_HANDLE;
  REQUIRE(granit_texture_create(renderer.native_handle(), &texture_desc, &texture) ==
          GRANIT_SUCCESS);
  granit_upload_batch_desc batch_desc = GRANIT_UPLOAD_BATCH_DESC_INIT;
  granit_upload_batch batch = GRANIT_NULL_HANDLE;
  REQUIRE(granit_upload_batch_create(renderer.native_handle(), &batch_desc, &batch) ==
          GRANIT_SUCCESS);
  REQUIRE(granit_upload_batch_write_texture_asset_mips(
              renderer.native_handle(), batch, texture, manifest.data(), manifest.size(),
              payload.data(), payload.size(), 0, 0, 1) == GRANIT_SUCCESS);
  CHECK(granit_upload_batch_submit(renderer.native_handle(), batch) == GRANIT_SUCCESS);
  CHECK(granit_upload_batch_destroy(renderer.native_handle(), batch) == GRANIT_SUCCESS);
  CHECK(granit_texture_destroy(renderer.native_handle(), texture) == GRANIT_SUCCESS);
}

TEST_CASE("Texture Asset逐Mip接口在复制前执行背压和摘要校验",
          "[texture_asset][upload][backpressure]") {
  granit::renderer renderer;
  const auto initialized = renderer.initialize();
  if (unavailable(initialized)) {
    WARN("当前环境没有可用 Renderer，跳过真实背压测试");
    return;
  }
  REQUIRE(initialized == granit::result::success);
  const auto manifest = make_manifest();
  std::array<std::byte, 64> payload{};
  granit_texture_desc texture_desc = GRANIT_TEXTURE_DESC_INIT;
  texture_desc.format = GRANIT_TEXTURE_FORMAT_RGBA8_SRGB;
  texture_desc.usage =
      GRANIT_TEXTURE_USAGE_SAMPLED_BIT | GRANIT_TEXTURE_USAGE_TRANSFER_DESTINATION_BIT;
  texture_desc.width = 4;
  texture_desc.height = 4;
  granit_texture texture = GRANIT_NULL_HANDLE;
  REQUIRE(granit_texture_create(renderer.native_handle(), &texture_desc, &texture) ==
          GRANIT_SUCCESS);
  granit_upload_batch_desc batch_desc = GRANIT_UPLOAD_BATCH_DESC_INIT;
  batch_desc.max_staged_bytes = 32;
  granit_upload_batch batch = GRANIT_NULL_HANDLE;
  REQUIRE(granit_upload_batch_create(renderer.native_handle(), &batch_desc, &batch) ==
          GRANIT_SUCCESS);
  CHECK(granit_upload_batch_write_texture_asset_mips(
            renderer.native_handle(), batch, texture, manifest.data(), manifest.size(),
            payload.data(), payload.size(), 0, 0, 1) == GRANIT_ERROR_NOT_READY);
  granit_upload_batch_info info = GRANIT_UPLOAD_BATCH_INFO_INIT;
  REQUIRE(granit_upload_batch_get_info(renderer.native_handle(), batch, &info) == GRANIT_SUCCESS);
  CHECK(info.operation_count == 0);
  CHECK(info.staged_bytes == 0);

  payload[0] = std::byte{1};
  CHECK(granit_upload_batch_write_texture_asset_mips(
            renderer.native_handle(), batch, texture, manifest.data(), manifest.size(),
            payload.data(), payload.size(), 0, 0, 1) == GRANIT_ERROR_INVALID_ARGUMENT);
  CHECK(granit_upload_batch_destroy(renderer.native_handle(), batch) == GRANIT_SUCCESS);
  CHECK(granit_texture_destroy(renderer.native_handle(), texture) == GRANIT_SUCCESS);
}

} // namespace
