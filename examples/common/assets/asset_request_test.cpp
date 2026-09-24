// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "assets/asset_request.h"

#include <catch2/catch_all.hpp>

#include <array>
#include <vector>

namespace assets = granit::example::assets;

TEST_CASE("资产请求只接受当前 generation", "[example][assets]") {
  assets::asset_request request;
  const auto first = assets::asset_request_writer::begin(request, "first.glb");
  const auto second = assets::asset_request_writer::begin(request, "second.glb");
  constexpr std::array bytes{std::byte{1}, std::byte{2}};

  CHECK_FALSE(assets::asset_request_writer::complete(request, first, bytes));
  CHECK(assets::asset_request_writer::complete(request, second, bytes));
  CHECK(request.status() == assets::asset_request_status::ready);
  CHECK(request.location() == "second.glb");
  CHECK(request.bytes() == std::vector<std::byte>(bytes.begin(), bytes.end()));
  CHECK_FALSE(assets::asset_request_writer::fail(
      request, second, assets::asset_request_error::transport_error, "迟到的失败"));
}

TEST_CASE("资产请求报告进度、失败和取消", "[example][assets]") {
  assets::asset_request request;
  const auto generation = assets::asset_request_writer::begin(request, "asset.bin");
  assets::asset_request_writer::progress(request, generation, 4, 8);
  CHECK(request.progress().received_bytes == 4);
  REQUIRE(request.progress().total_bytes);
  CHECK(*request.progress().total_bytes == 8);

  request.cancel();
  CHECK(request.status() == assets::asset_request_status::cancelled);
  CHECK(request.error() == assets::asset_request_error::cancelled);
  constexpr std::array bytes{std::byte{3}};
  CHECK_FALSE(assets::asset_request_writer::complete(request, generation, bytes));

  const auto failed_generation = assets::asset_request_writer::begin(request, "missing.bin");
  REQUIRE(assets::asset_request_writer::fail(
      request, failed_generation, assets::asset_request_error::io_error, "读取资产失败"));
  CHECK(request.status() == assets::asset_request_status::failed);
  CHECK(request.error() == assets::asset_request_error::io_error);
  CHECK(request.diagnostic() == "读取资产失败");

  request.reset();
  CHECK(request.status() == assets::asset_request_status::idle);
  CHECK(request.error() == assets::asset_request_error::none);
  CHECK(request.location().empty());
  CHECK(request.bytes().empty());
}
