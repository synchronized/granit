// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "assets/asset_system.h"
#include "assets/asset_system_resolver.h"

#include <catch2/catch_all.hpp>

#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <thread>
#include <vector>

namespace assets = granit::example::assets;

#if !defined(__EMSCRIPTEN__)
namespace {

class temporary_assets {
public:
  temporary_assets() {
    const auto unique = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    root = std::filesystem::temp_directory_path() / ("granit-asset-system-" + unique);
    REQUIRE(std::filesystem::create_directories(root / "bin" / "assets" / "tutorials"));
    REQUIRE(std::filesystem::create_directories(root / "content"));
    write(root / "bin" / "assets" / "tutorials" / "bundled.bin", {1, 2, 3});
    write(root / "content" / "external.bin", {4, 5, 6});
  }

  ~temporary_assets() {
    std::error_code error;
    std::filesystem::remove_all(root, error);
  }

  temporary_assets(const temporary_assets&) = delete;
  temporary_assets& operator=(const temporary_assets&) = delete;

  static void write(const std::filesystem::path& path, std::array<char, 3> bytes) {
    std::ofstream stream{path, std::ios::binary};
    REQUIRE(stream.good());
    stream.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
  }

  std::filesystem::path root;
};

} // namespace

TEST_CASE("Asset System 以统一请求读取打包和外部资产", "[example][assets][system]") {
  temporary_assets fixture;
  assets::asset_system system;
  REQUIRE(system.initialize((fixture.root / "bin" / "example").string()));
  REQUIRE(system.bundled().valid());

  const auto bundled = system.request({system.bundled(), "tutorials/./bundled.bin"});
  REQUIRE(bundled);
  CHECK(bundled->status() == assets::asset_request_status::ready);
  CHECK(bundled->bytes().size() == 3);

  assets::asset_system_resolver resolver{system, system.bundled(), "tutorials"};
  std::vector<std::byte> resolved;
  REQUIRE(resolver.resolve("bundled.bin", resolved));
  CHECK(resolved == bundled->bytes());
  const auto committed = resolved;
  CHECK_FALSE(resolver.resolve("../bundled.bin", resolved));
  CHECK(resolved == committed);

  assets::asset_mount content;
  REQUIRE(system.mount((fixture.root / "content").string(), content));
  const auto external = system.request({content, "external.bin"});
  REQUIRE(external);
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{5};
  while (external->status() == assets::asset_request_status::pending &&
         std::chrono::steady_clock::now() < deadline) {
    system.poll();
    std::this_thread::sleep_for(std::chrono::milliseconds{1});
  }
  CHECK(external->status() == assets::asset_request_status::ready);
  CHECK(external->bytes().size() == 3);

  assets::asset_mount file_mount;
  std::string file_path;
  REQUIRE(system.mount_location((fixture.root / "content" / "external.bin").string(), file_mount,
                                file_path));
  CHECK(file_mount.valid());
  CHECK(file_path == "external.bin");
}
#endif

TEST_CASE("Asset System 将无效 Key 转换为失败请求", "[example][assets][system]") {
  assets::asset_system system;
  REQUIRE(system.initialize("example"));

  const auto invalid_mount = system.request({{}, "fixture.bin"});
  REQUIRE(invalid_mount);
  CHECK(invalid_mount->status() == assets::asset_request_status::failed);
  CHECK(invalid_mount->error() == assets::asset_request_error::invalid_location);

  const auto invalid_path = system.request({system.bundled(), "../fixture.bin"});
  REQUIRE(invalid_path);
  CHECK(invalid_path->status() == assets::asset_request_status::failed);
  CHECK(invalid_path->error() == assets::asset_request_error::invalid_location);

  assets::asset_mount unchanged;
  std::string unchanged_path{"committed"};
  CHECK_FALSE(system.mount({}, unchanged));
  CHECK_FALSE(system.mount_location("https://example.com/model.gltf?token=secret", unchanged,
                                    unchanged_path));
  CHECK_FALSE(unchanged.valid());
  CHECK(unchanged_path == "committed");
}
