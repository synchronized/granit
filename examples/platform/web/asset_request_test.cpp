// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "asset_request.h"

#include <catch2/catch_all.hpp>

#include <array>
#include <vector>

namespace web = granit::example::model_viewer::web;

TEST_CASE("浏览器资产请求只接受当前 generation", "[example][model-viewer][web]") {
  web::asset_request request;
  const auto first = request.begin("first.glb");
  const auto second = request.begin("second.glb");
  constexpr std::array bytes{std::byte{1}, std::byte{2}};

  CHECK_FALSE(request.complete(first, bytes));
  CHECK(request.complete(second, bytes));
  CHECK(request.status() == web::asset_request_status::ready);
  CHECK(request.url() == "second.glb");
  CHECK(request.bytes() == std::vector<std::byte>(bytes.begin(), bytes.end()));
  CHECK_FALSE(request.fail(second, "迟到的失败"));
}

TEST_CASE("浏览器资产请求区分失败、取消和重置", "[example][model-viewer][web]") {
  web::asset_request request;
  const auto failed = request.begin("broken.glb");
  CHECK(request.fail(failed, "HTTP 404"));
  CHECK(request.status() == web::asset_request_status::failed);
  CHECK(request.diagnostic() == "HTTP 404");

  const auto cancelled = request.begin("cancelled.glb");
  request.cancel();
  CHECK(request.status() == web::asset_request_status::idle);
  constexpr std::array bytes{std::byte{3}};
  CHECK_FALSE(request.complete(cancelled, bytes));

  static_cast<void>(request.begin("ready.glb"));
  request.reset();
  CHECK(request.status() == web::asset_request_status::idle);
  CHECK(request.url().empty());
  CHECK(request.bytes().empty());
  CHECK(request.diagnostic().empty());
}

TEST_CASE("浏览器资产请求拒绝空成功响应", "[example][model-viewer][web]") {
  web::asset_request request;
  const auto generation = request.begin("empty.glb");
  CHECK_FALSE(request.complete(generation, {}));
  CHECK(request.status() == web::asset_request_status::pending);
}
