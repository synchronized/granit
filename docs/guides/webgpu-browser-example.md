<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 运行浏览器 WebGPU 示例

浏览器构建同时提供正式的 `granit_model_viewer_web` 和自动化
`granit_web_platform_smoke`。两者复用同一个 Model Viewer Core；正式目标默认从 Khronos 加载
Flight Helmet，Smoke 使用仓库内的小型确定性 Fixture。动态 Uniform、纹理传输和帧生命周期的
测试图形只会由 Smoke 目标呈现，正式 Model Viewer 不会在模型出现前显示测试方块。浏览器端不会
接触 WebGPU 原生句柄。

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

模型下载后，页面会按文档解析、Buffer、图片、材质、网格、节点和 GPU 资源阶段显示真实进度。
阶段边界通过 Emscripten Asyncify 让出浏览器事件循环，因此页面仍可重绘并响应 Cancel；取消后
返回稳定的 `GRANIT_ERROR_CANCELLED`，已经创建的临时资源会随事务回滚。

若初始化失败，请确认浏览器已启用 WebGPU，并从开发者工具控制台查看
`GRANIT_DIAGNOSTIC` 与 `GRANIT_STATUS` 日志。Renderer 初始化超过 30 秒会以
`failed:renderer-timeout` 明确结束，不会永久停留在加载界面。信息级诊断使用普通 Console 日志，
只有警告与错误进入错误输出。

浏览器端当前会明确关闭 Timestamp Query 能力，避免把 Adapter 的 `timestamp-query` 暴露误判为
支持任意 `CommandEncoder` 写入。看到 `commandEncoder.writeTimestamp is not a function` 表示仍在
运行旧构建产物；重新构建后停止并重启 HTTP 服务，再使用 `Ctrl+F5` 强制刷新页面。

## 自动验证

仓库浏览器测试会启动无头 Chrome，验证 Renderer 生命周期、共享 Fixture、资源传输、Mipmap、
分阶段进度、加载取消、错误回滚以及键盘和鼠标输入转发：

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
