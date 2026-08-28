<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 运行 WebGPU 浏览器三角形示例

该示例使用 Granit 公共 C API 完成 WebGPU Renderer 初始化、Canvas Surface、Swapchain、WGSL
Shader、Graphics Pipeline、Command Recorder、Queue Submit 和 Present。浏览器端不会接触 WebGPU
原生句柄。

## 前置条件

- 已安装并激活 Emscripten `5.0.6`。
- 浏览器支持 WebGPU；本地开发可使用较新的 Chrome 或 Edge。
- 使用静态 HTTP 服务器访问产物，不能直接双击 HTML 文件。

Windows 已安装但尚未导入 emsdk 环境时，先执行：

```powershell
& D:\path\to\emsdk\emsdk_env.ps1
```

## 构建

```powershell
cmake --preset emscripten-release
cmake --build --preset emscripten-release
```

构建产物位于：

```text
build/emscripten-release/web/granit_webgpu_triangle_example.html
```

## 运行

可使用任意静态 HTTP 服务器。例如：

```powershell
python -m http.server 8000 --directory build/emscripten-release/web
```

然后访问：

```text
http://127.0.0.1:8000/granit_webgpu_triangle_example.html
```

页面应显示黑色背景和绿色三角形。若初始化失败，请确认浏览器已启用 WebGPU，并从开发者工具控制台
查看 `GRANIT_DIAGNOSTIC` 与 `GRANIT_STATUS` 日志。

## 自动验证

仓库浏览器测试会启动无头 Chrome，验证 Renderer 生命周期、键盘和鼠标输入转发；浏览器合成层
可读时还会验证中心绿色像素和角落黑色像素。Linux SwiftShader 若中心返回透明像素，会明确记录跳过
截图颜色断言，绘制结果继续由 WebGPU 插件离屏回读测试覆盖：

```powershell
cd web/tests
npm ci
$env:CHROME_PATH = "C:\Program Files\Google\Chrome\Application\chrome.exe"
npm test -- ../../build/emscripten-release/web
```
