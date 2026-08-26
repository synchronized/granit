// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "backend/resources.h"

#include <catch2/catch_all.hpp>

#include <memory>

namespace {

class fake_sampler_resource final : public granit::detail::backend_sampler_resource {
public:
  explicit fake_sampler_resource(bool& destroyed) noexcept : destroyed_(destroyed) {}
  ~fake_sampler_resource() override { destroyed_ = true; }

private:
  bool& destroyed_;
};

} // namespace

TEST_CASE("后端资源通过抽象所有权正确销毁") {
  bool destroyed = false;
  {
    std::unique_ptr<granit::detail::backend_sampler_resource> resource =
        std::make_unique<fake_sampler_resource>(destroyed);
    CHECK_FALSE(destroyed);
  }
  CHECK(destroyed);
}
