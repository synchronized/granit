<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 运行浏览器 WebGPU 示例

浏览器构建同时提供正式的 `granit_model_viewer_web` 和自动化
`granit_web_platform_smoke`。两者复用同一个 Model Viewer Core；正式目标默认从 Khronos 加载
Flight Helmet，Smoke 使用仓库内的小型确定性 Fixture。浏览器端不会接触 WebGPU 原生句柄。

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

查看正式模型：

```text
http://127.0.0.1:8000/granit_model_viewer_web.html
```

可用 `model` 查询参数加载另一份 glTF/GLB。相对资源 URI 会以模型 URL 所在目录为基准解析：

```text
http://127.0.0.1:8000/granit_model_viewer_web.html?model=https%3A%2F%2Fexample.com%2Fmodel.gltf
```

远程服务器必须允许跨域访问模型及其外部 Buffer、纹理。页面必须通过 HTTP 服务打开，不能直接
双击 HTML 文件。

若初始化失败，请确认浏览器已启用 WebGPU，并从开发者工具控制台查看
`GRANIT_DIAGNOSTIC` 与 `GRANIT_STATUS` 日志。

## 自动验证

仓库浏览器测试会启动无头 Chrome，验证 Renderer 生命周期、共享 Fixture、资源传输、Mipmap、
键盘和鼠标输入转发：

```powershell
cd web/tests
npm ci
$env:CHROME_PATH = "C:\Program Files\Google\Chrome\Application\chrome.exe"
npm test -- ../../build/emscripten-release/web
```

正式目标也可用本地 Fixture 做快速回归：

```powershell
npm test -- ../../build/emscripten-release/web granit_model_viewer_web.html `
  model_viewer_fixture.gltf
```

模型查看器的共享能力和桌面运行方法见[跨后端模型查看器](model-viewer.md)。
