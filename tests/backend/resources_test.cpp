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

class fake_texture_view_resource final : public granit::detail::backend_texture_view_resource {
public:
  explicit fake_texture_view_resource(bool& destroyed) noexcept : destroyed_(destroyed) {}
  ~fake_texture_view_resource() override { destroyed_ = true; }

private:
  bool& destroyed_;
};

class fake_texture_resource final : public granit::detail::backend_texture_resource {
public:
  explicit fake_texture_resource(bool& destroyed) noexcept : destroyed_(destroyed) {}
  ~fake_texture_resource() override { destroyed_ = true; }

private:
  bool& destroyed_;
};

class fake_buffer_resource final : public granit::detail::backend_buffer_resource {
public:
  explicit fake_buffer_resource(bool& destroyed) noexcept : destroyed_(destroyed) {}
  ~fake_buffer_resource() override { destroyed_ = true; }

private:
  bool& destroyed_;
};

class fake_shader_resource final : public granit::detail::backend_shader_resource {
public:
  explicit fake_shader_resource(bool& destroyed) noexcept : destroyed_(destroyed) {}
  ~fake_shader_resource() override { destroyed_ = true; }

private:
  bool& destroyed_;
};

class fake_bind_group_layout_resource final
    : public granit::detail::backend_bind_group_layout_resource {
public:
  explicit fake_bind_group_layout_resource(bool& destroyed) noexcept : destroyed_(destroyed) {}
  ~fake_bind_group_layout_resource() override { destroyed_ = true; }

private:
  bool& destroyed_;
};

class fake_bind_group_resource final : public granit::detail::backend_bind_group_resource {
public:
  explicit fake_bind_group_resource(bool& destroyed) noexcept : destroyed_(destroyed) {}
  ~fake_bind_group_resource() override { destroyed_ = true; }

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

TEST_CASE("纹理视图通过后端抽象正确销毁") {
  bool destroyed = false;
  {
    std::unique_ptr<granit::detail::backend_texture_view_resource> resource =
        std::make_unique<fake_texture_view_resource>(destroyed);
    CHECK_FALSE(destroyed);
  }
  CHECK(destroyed);
}

TEST_CASE("纹理通过后端抽象正确销毁") {
  bool destroyed = false;
  {
    std::unique_ptr<granit::detail::backend_texture_resource> resource =
        std::make_unique<fake_texture_resource>(destroyed);
    CHECK_FALSE(destroyed);
  }
  CHECK(destroyed);
}

TEST_CASE("缓冲区通过后端抽象正确销毁") {
  bool destroyed = false;
  {
    std::unique_ptr<granit::detail::backend_buffer_resource> resource =
        std::make_unique<fake_buffer_resource>(destroyed);
    CHECK_FALSE(destroyed);
  }
  CHECK(destroyed);
}

TEST_CASE("着色器通过后端抽象正确销毁") {
  bool destroyed = false;
  {
    std::unique_ptr<granit::detail::backend_shader_resource> resource =
        std::make_unique<fake_shader_resource>(destroyed);
    CHECK_FALSE(destroyed);
  }
  CHECK(destroyed);
}

TEST_CASE("绑定组布局通过后端抽象正确销毁") {
  bool destroyed = false;
  {
    std::unique_ptr<granit::detail::backend_bind_group_layout_resource> resource =
        std::make_unique<fake_bind_group_layout_resource>(destroyed);
    CHECK_FALSE(destroyed);
  }
  CHECK(destroyed);
}

TEST_CASE("绑定组通过后端抽象正确销毁") {
  bool destroyed = false;
  {
    std::unique_ptr<granit::detail::backend_bind_group_resource> resource =
        std::make_unique<fake_bind_group_resource>(destroyed);
    CHECK_FALSE(destroyed);
  }
  CHECK(destroyed);
}
