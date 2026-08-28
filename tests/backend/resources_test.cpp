// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "backend/access.h"
#include "backend/queue.h"
#include "backend/resources.h"
#include "backend/upload.h"

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

class fake_pipeline_layout_resource final
    : public granit::detail::backend_pipeline_layout_resource {
public:
  explicit fake_pipeline_layout_resource(bool& destroyed) noexcept : destroyed_(destroyed) {}
  ~fake_pipeline_layout_resource() override { destroyed_ = true; }

private:
  bool& destroyed_;
};

class fake_graphics_pipeline_resource final
    : public granit::detail::backend_graphics_pipeline_resource {
public:
  explicit fake_graphics_pipeline_resource(bool& destroyed) noexcept : destroyed_(destroyed) {}
  ~fake_graphics_pipeline_resource() override { destroyed_ = true; }

private:
  bool& destroyed_;
};

class fake_compute_pipeline_resource final
    : public granit::detail::backend_compute_pipeline_resource {
public:
  explicit fake_compute_pipeline_resource(bool& destroyed) noexcept : destroyed_(destroyed) {}
  ~fake_compute_pipeline_resource() override { destroyed_ = true; }

private:
  bool& destroyed_;
};

class fake_command_recorder_resource final
    : public granit::detail::backend_command_recorder_resource {
public:
  explicit fake_command_recorder_resource(bool& destroyed) noexcept : destroyed_(destroyed) {}
  ~fake_command_recorder_resource() override { destroyed_ = true; }

private:
  bool& destroyed_;
};

class fake_surface_resource final : public granit::detail::backend_surface_resource {
public:
  explicit fake_surface_resource(bool& destroyed) noexcept : destroyed_(destroyed) {}
  ~fake_surface_resource() override { destroyed_ = true; }

private:
  bool& destroyed_;
};

class fake_swapchain_resource final : public granit::detail::backend_swapchain_resource {
public:
  explicit fake_swapchain_resource(bool& destroyed) noexcept : destroyed_(destroyed) {}
  ~fake_swapchain_resource() override { destroyed_ = true; }

private:
  bool& destroyed_;
};

class fake_timestamp_query_pool_resource final
    : public granit::detail::backend_timestamp_query_pool_resource {
public:
  explicit fake_timestamp_query_pool_resource(bool& destroyed) noexcept : destroyed_(destroyed) {}
  ~fake_timestamp_query_pool_resource() override { destroyed_ = true; }

private:
  bool& destroyed_;
};

class fake_queue final : public granit::detail::backend_queue {
public:
  granit_result
  submit_command_recorder(granit::detail::backend_command_recorder_resource&,
                          granit::detail::submission_serial& submitted_serial) override {
    submitted_serial = 1;
    return GRANIT_SUCCESS;
  }

  granit_result submit_command_recorders(
      std::span<granit::detail::backend_command_recorder_resource* const> recorders,
      granit::detail::submission_serial& submitted_serial) override {
    submitted_serial = recorders.size();
    return recorders.empty() ? GRANIT_ERROR_INVALID_ARGUMENT : GRANIT_SUCCESS;
  }

  granit_result
  wait_command_recorder(granit::detail::backend_command_recorder_resource&) noexcept override {
    return GRANIT_SUCCESS;
  }

  granit_result wait_for_all_submissions() noexcept override { return GRANIT_SUCCESS; }

  granit_result
  submit_swapchain_frame(granit::detail::backend_command_recorder_resource&,
                         granit::detail::backend_swapchain_resource&, std::uint32_t image_index,
                         std::size_t slot_index,
                         granit::detail::submission_serial& submitted_serial) override {
    submitted_serial = image_index + slot_index + 1;
    return GRANIT_SUCCESS;
  }

  granit_result present_swapchain_frame(granit::detail::backend_swapchain_resource&, std::uint32_t,
                                        std::size_t, bool& needs_recreate) override {
    needs_recreate = false;
    return GRANIT_SUCCESS;
  }

  granit_result cancel_swapchain_frame(granit::detail::backend_swapchain_resource&, std::uint32_t,
                                       std::size_t, bool& needs_recreate) override {
    needs_recreate = false;
    return GRANIT_SUCCESS;
  }

  granit_result wait_for_present_idle() noexcept override { return GRANIT_SUCCESS; }
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

TEST_CASE("管线布局通过后端抽象正确销毁") {
  bool destroyed = false;
  {
    std::unique_ptr<granit::detail::backend_pipeline_layout_resource> resource =
        std::make_unique<fake_pipeline_layout_resource>(destroyed);
    CHECK_FALSE(destroyed);
  }
  CHECK(destroyed);
}

TEST_CASE("图形管线通过后端抽象正确销毁") {
  bool destroyed = false;
  {
    std::unique_ptr<granit::detail::backend_graphics_pipeline_resource> resource =
        std::make_unique<fake_graphics_pipeline_resource>(destroyed);
    CHECK_FALSE(destroyed);
  }
  CHECK(destroyed);
}

TEST_CASE("计算管线通过后端抽象正确销毁") {
  bool destroyed = false;
  {
    std::unique_ptr<granit::detail::backend_compute_pipeline_resource> resource =
        std::make_unique<fake_compute_pipeline_resource>(destroyed);
    CHECK_FALSE(destroyed);
  }
  CHECK(destroyed);
}

TEST_CASE("命令记录器通过后端抽象正确销毁") {
  bool destroyed = false;
  {
    std::unique_ptr<granit::detail::backend_command_recorder_resource> resource =
        std::make_unique<fake_command_recorder_resource>(destroyed);
    CHECK_FALSE(destroyed);
  }
  CHECK(destroyed);
}

TEST_CASE("Surface 通过后端抽象正确销毁") {
  bool destroyed = false;
  {
    std::unique_ptr<granit::detail::backend_surface_resource> resource =
        std::make_unique<fake_surface_resource>(destroyed);
    CHECK_FALSE(destroyed);
  }
  CHECK(destroyed);
}

TEST_CASE("Swapchain 通过后端抽象正确销毁") {
  bool destroyed = false;
  {
    std::unique_ptr<granit::detail::backend_swapchain_resource> resource =
        std::make_unique<fake_swapchain_resource>(destroyed);
    CHECK_FALSE(destroyed);
  }
  CHECK(destroyed);
}

TEST_CASE("时间戳查询池通过后端抽象正确销毁") {
  bool destroyed = false;
  {
    std::unique_ptr<granit::detail::backend_timestamp_query_pool_resource> resource =
        std::make_unique<fake_timestamp_query_pool_resource>(destroyed);
    CHECK_FALSE(destroyed);
  }
  CHECK(destroyed);
}

TEST_CASE("上传批次描述不依赖具体后端类型") {
  bool destroyed = false;
  fake_buffer_resource buffer{destroyed};
  const granit::detail::backend_upload_operation upload{
      .type = granit::detail::backend_upload_type::buffer,
      .buffer = &buffer,
      .destination_offset = 16,
      .size = 32,
  };
  CHECK(upload.buffer == &buffer);
  CHECK(upload.destination_offset == 16);
  CHECK(upload.size == 32);
}

TEST_CASE("绑定访问描述只引用后端抽象资源") {
  bool buffer_destroyed = false;
  bool texture_destroyed = false;
  fake_buffer_resource buffer{buffer_destroyed};
  fake_texture_resource texture{texture_destroyed};
  const granit::detail::backend_buffer_access buffer_access{
      .buffer = &buffer,
      .type = granit::detail::backend_buffer_access_type::storage_read_write,
  };
  const granit::detail::backend_texture_access texture_access{
      .texture = &texture,
      .range = {.aspect = GRANIT_TEXTURE_ASPECT_COLOR_BIT,
                .base_mip_level = 0,
                .mip_level_count = 1,
                .base_array_layer = 0,
                .array_layer_count = 1},
      .format = GRANIT_TEXTURE_FORMAT_RGBA8_UNORM,
      .type = granit::detail::backend_texture_access_type::sampled_read,
  };
  CHECK(buffer_access.buffer == &buffer);
  CHECK(texture_access.texture == &texture);
  CHECK(texture_access.range.aspect == GRANIT_TEXTURE_ASPECT_COLOR_BIT);
}

TEST_CASE("队列契约使用后端命令对象和统一提交序列") {
  bool destroyed = false;
  fake_command_recorder_resource recorder{destroyed};
  fake_queue queue;
  granit::detail::submission_serial serial{};
  CHECK(queue.submit_command_recorder(recorder, serial) == GRANIT_SUCCESS);
  CHECK(serial == 1);
  granit::detail::backend_command_recorder_resource* recorders[]{&recorder};
  CHECK(queue.submit_command_recorders(recorders, serial) == GRANIT_SUCCESS);
  CHECK(serial == 1);
  CHECK(queue.wait_command_recorder(recorder) == GRANIT_SUCCESS);

  fake_swapchain_resource swapchain{destroyed};
  bool needs_recreate = true;
  CHECK(queue.submit_swapchain_frame(recorder, swapchain, 2, 3, serial) == GRANIT_SUCCESS);
  CHECK(serial == 6);
  CHECK(queue.present_swapchain_frame(swapchain, 2, 3, needs_recreate) == GRANIT_SUCCESS);
  CHECK_FALSE(needs_recreate);
  CHECK(queue.cancel_swapchain_frame(swapchain, 2, 3, needs_recreate) == GRANIT_SUCCESS);
  CHECK(queue.wait_for_present_idle() == GRANIT_SUCCESS);
}
