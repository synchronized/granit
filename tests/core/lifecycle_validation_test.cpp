// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "core/lifecycle_validation.h"

#include <catch2/catch_all.hpp>

namespace {

using granit::detail::lifecycle_resource_type;
using granit::detail::lifecycle_snapshot;

TEST_CASE("生命周期快照按资源类型统计并保留创建序号", "[lifecycle][validation]") {
  lifecycle_snapshot snapshot;
  CHECK(snapshot.empty());

  snapshot.add(lifecycle_resource_type::buffer, UINT64_C(0x101), 7);
  snapshot.add(lifecycle_resource_type::texture, UINT64_C(0x202), 9);
  snapshot.add(lifecycle_resource_type::buffer, UINT64_C(0x303), 11);

  CHECK_FALSE(snapshot.empty());
  CHECK(snapshot.total_count() == 3);
  const auto& buffers = snapshot.summary(lifecycle_resource_type::buffer);
  CHECK(buffers.count == 2);
  REQUIRE(buffers.sample_count == 2);
  CHECK(buffers.samples[0].handle == UINT64_C(0x101));
  CHECK(buffers.samples[0].creation_sequence == 7);
  CHECK(buffers.samples[1].handle == UINT64_C(0x303));
  CHECK(snapshot.summary(lifecycle_resource_type::sampler).count == 0);
}

TEST_CASE("生命周期快照限制句柄样本但保留资源总数", "[lifecycle][validation]") {
  lifecycle_snapshot snapshot;
  constexpr auto extra_count = std::size_t{3};
  constexpr auto total = granit::detail::lifecycle_resource_summary::maximum_samples + extra_count;
  for (std::size_t index = 0; index < total; ++index) {
    snapshot.add(lifecycle_resource_type::sampler, static_cast<granit_handle>(index + 1),
                 static_cast<std::uint64_t>(index + 10));
  }

  const auto& samplers = snapshot.summary(lifecycle_resource_type::sampler);
  CHECK(samplers.count == total);
  CHECK(samplers.sample_count == samplers.maximum_samples);
  CHECK(snapshot.total_count() == total);
}

} // namespace
