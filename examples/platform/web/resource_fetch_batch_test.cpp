// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "resource_fetch_batch.h"

#include <catch2/catch_all.hpp>

#include <array>
#include <cstddef>

namespace granit::example::model_viewer {

TEST_CASE("Web 资源请求批次只在全部完成后提交") {
  web::resource_fetch_batch batch;
  CHECK(batch.add("mesh.bin", "assets/mesh.bin"));
  CHECK(batch.add("textures/base.png", "assets/textures/base.png"));
  CHECK(batch.status() == web::resource_fetch_batch_status::idle);

  const std::array mesh{std::byte{1}};
  const std::array texture{std::byte{2}, std::byte{3}};
  const auto& entries = batch.entries();
  const auto mesh_generation = entries[0].request->begin(entries[0].url);
  const auto texture_generation = entries[1].request->begin(entries[1].url);
  CHECK(batch.status() == web::resource_fetch_batch_status::pending);
  CHECK(entries[0].request->complete(mesh_generation, mesh));

  web::resource_bundle bundle;
  CHECK_FALSE(batch.commit(bundle));
  CHECK(entries[1].request->complete(texture_generation, texture));
  CHECK(batch.status() == web::resource_fetch_batch_status::ready);
  CHECK(batch.commit(bundle));
  CHECK(bundle.contains("mesh.bin"));
  CHECK(bundle.contains("textures/base.png"));
}

TEST_CASE("Web 资源请求批次拒绝重复路径和失败请求") {
  web::resource_fetch_batch batch;
  CHECK(batch.add("./mesh.bin", "assets/mesh.bin"));
  CHECK_FALSE(batch.add("mesh.bin", "assets/duplicate.bin"));
  CHECK_FALSE(batch.add("../escape.bin", "assets/escape.bin"));
  CHECK_FALSE(batch.add("other.bin", {}));

  const auto& request = batch.entries().front().request;
  const auto generation = request->begin(batch.entries().front().url);
  CHECK(request->fail(generation, "测试失败"));
  CHECK(batch.status() == web::resource_fetch_batch_status::failed);

  web::resource_bundle bundle;
  CHECK_FALSE(batch.commit(bundle));
  CHECK(bundle.size() == 0);
}

} // namespace granit::example::model_viewer
