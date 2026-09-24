// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_EXAMPLES_COMMON_ASSETS_ASSET_SYSTEM_H_
#define GRANIT_EXAMPLES_COMMON_ASSETS_ASSET_SYSTEM_H_

#include "assets/asset_request.h"

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

namespace granit::example::assets {

/** Asset System 内部挂载的类型安全标识；零值无效。 */
class asset_mount final {
public:
  asset_mount() = default;

  [[nodiscard]] bool valid() const noexcept { return value_ != 0; }
  friend bool operator==(asset_mount, asset_mount) = default;

private:
  friend class asset_system;
  explicit asset_mount(std::uint32_t value) : value_(value) {}

  std::uint32_t value_{};
};

struct asset_key {
  asset_mount mount;
  std::string_view path;
};

/** 示例私有的统一只读资产入口；平台来源不会进入业务资产 Key。 */
class asset_system final {
public:
  asset_system();
  ~asset_system();
  asset_system(const asset_system&) = delete;
  asset_system& operator=(const asset_system&) = delete;

  /** 初始化随程序部署的资产挂载；一个对象只允许成功初始化一次。 */
  [[nodiscard]] bool initialize(std::string_view executable_path);

  /** 挂载外部目录或 URL 根；失败时 output 保持不变。 */
  [[nodiscard]] bool mount(std::string root_location, asset_mount& output);

  /** 将一个外部文件或 URL 拆为挂载与逻辑路径；失败时两个输出保持不变。 */
  [[nodiscard]] bool mount_location(std::string location, asset_mount& output_mount,
                                    std::string& output_path);

  /** 发起完整 Blob 请求；无效 Key 也返回带诊断的失败请求。 */
  [[nodiscard]] std::shared_ptr<asset_request> request(asset_key key);

  /** 在调用线程发布后台 Source 已完成的结果。 */
  void poll();

  [[nodiscard]] asset_mount bundled() const noexcept { return bundled_; }

private:
  struct implementation;
  std::unique_ptr<implementation> implementation_;
  asset_mount bundled_;
};

} // namespace granit::example::assets

#endif // GRANIT_EXAMPLES_COMMON_ASSETS_ASSET_SYSTEM_H_
