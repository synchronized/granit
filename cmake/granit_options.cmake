option(BUILD_SHARED_LIBS "构建共享库" ON)
option(GRANIT_BUILD_TESTING "构建 Granit 测试" ON)
option(GRANIT_BUILD_EXAMPLES "构建 Granit 示例" ${PROJECT_IS_TOP_LEVEL})
option(GRANIT_BUILD_BENCHMARKS "构建 Granit benchmark 程序" OFF)
option(GRANIT_BUILD_TOOLS "构建 Granit 离线工具" OFF)
option(GRANIT_BUILD_ASSET_TOOLS "构建并安装 AssetTools SDK" OFF)
option(GRANIT_BUILD_INTEGRATION_SDL3 "构建 SDL3 可选集成组件" OFF)
option(GRANIT_ENABLE_WINDOW_SDL3 "在 Window component 中启用 SDL3 后端" ${GRANIT_BUILD_EXAMPLES})
option(GRANIT_BUILD_INTEGRATION_IMGUI "构建 ImGui 可选集成组件" OFF)
option(GRANIT_BUILD_WEB_IMGUI_EXAMPLE "构建浏览器 SDL3 + ImGui 示例" ON)
option(GRANIT_ENABLE_XCB "在 Linux 上启用 XCB Surface 与 Window 后端" ON)
option(GRANIT_ENABLE_WAYLAND "在 Linux 上启用 Wayland Surface 与 Window 后端" ON)

# 第三方依赖获取策略：全局默认 + 每依赖覆盖。
#   system   —— 只用 find_package，找不到即报错（离线 / 发行版打包）
#   auto     —— 先 find_package，找不到再 FetchContent 下载锁定版本（默认）
#   download —— 强制 FetchContent 下载锁定版本，忽略系统包（可复现构建）
set(GRANIT_DEPENDENCY_POLICY "auto" CACHE STRING
    "第三方依赖默认获取策略：system、auto 或 download")
set_property(CACHE GRANIT_DEPENDENCY_POLICY PROPERTY STRINGS system auto download)
set(GRANIT_DEPENDENCY_SDL3 "" CACHE STRING
    "SDL3 获取策略覆盖（空 = 跟随 GRANIT_DEPENDENCY_POLICY）")
set(GRANIT_DEPENDENCY_IMGUI "" CACHE STRING
    "ImGui 获取策略覆盖（空 = 跟随 GRANIT_DEPENDENCY_POLICY）")
set(GRANIT_DEPENDENCY_CGLTF "" CACHE STRING
    "cgltf 获取策略覆盖（空 = 跟随 GRANIT_DEPENDENCY_POLICY）")
set(GRANIT_DEPENDENCY_STB "" CACHE STRING
    "stb 获取策略覆盖（空 = 跟随 GRANIT_DEPENDENCY_POLICY）")
