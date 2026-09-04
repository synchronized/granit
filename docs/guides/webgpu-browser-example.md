<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 运行浏览器 WebGPU 平台 Smoke

`granit_web_platform_smoke` 是 Emscripten WebGPU 的自动验证入口，不是单独的公开三角形示例。
它验证 Renderer、Canvas Surface、Swapchain、WGSL Pipeline、输入转发和 Model Viewer 共享应用
核心，浏览器端不会接触 WebGPU 原生句柄。

## 前置条件

- 已安装并激活 Emscripten `5.0.6`。
- 浏览器支持 WebGPU；本地开发可使用较新的 Chrome 或 Edge。
- 使用静态 HTTP 服务器访问产物，不能直接双击 HTML 文件。

Windows 已安装但尚未导入 emsdk 环境时，先执行：

```powershell
& D:\path\to\emsdk\emsdk_env.ps1
```

## 构建与运行

```powershell
cmake --preset emscripten-release
cmake --build --preset emscripten-release
python -m http.server 8000 --directory build/emscripten-release/web
```

然后访问：

```text
http://127.0.0.1:8000/granit_web_platform_smoke.html
```

若初始化失败，请确认浏览器已启用 WebGPU，并从开发者工具控制台查看
`GRANIT_DIAGNOSTIC` 与 `GRANIT_STATUS` 日志。

## 自动验证

仓库浏览器测试会启动无头 Chrome，验证 Renderer 生命周期、共享 Fixture、键盘和鼠标输入转发：

```powershell
cd web/tests
npm ci
$env:CHROME_PATH = "C:\Program Files\Google\Chrome\Application\chrome.exe"
npm test -- ../../build/emscripten-release/web
```

面向使用者的浏览器模型查看器仍属于 `model_viewer` 示例系列，使用说明见
[跨后端模型查看器](model-viewer.md)。
