<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 从 0.19 迁移到 0.20

0.20.0 统一了 Frame 信息查询，收敛了仓库 Shader 资产的生成与加载路径，并重组了示例目录。
项目仍处于 0.x；Consumer 应重新编译，并把 CMake 请求版本更新为 0.20。

## 修改 Frame 信息查询

C API 不再要求与 Frame 重复关联的 Swapchain 参数。把 0.19 调用：

```c
granit_frame_info info = GRANIT_FRAME_INFO_INIT;
granit_result result = granit_frame_get_info(renderer, swapchain, frame, &info);
```

改为：

```c
granit_frame_info info = GRANIT_FRAME_INFO_INIT;
granit_result result = granit_frame_get_info(renderer, frame, &info);
```

声明现位于 `<granit/renderer/frame_context.h>`，Frame 值类型位于
`<granit/renderer/frame.h>`。`granit_frame_get_slot_info` 是开发期间的临时符号，不属于 0.20
ABI；若代码曾跟随开发分支使用它，也应改为新的 `granit_frame_get_info`。

C++ 的 `acquired_frame::query_info(frame_info&)` 签名没有变化，只需重新编译。已经调用
`frame_context::begin()` 的代码可直接使用返回的 `frame_recording::frame_slot()`，无需再查询一次。

## 部署 Shader Asset

发布 Shader 资产使用三件套：`.grshader` 清单，以及按后端使用的同名 `.grshader.spv` 或
`.grshader.wgsl` sidecar。运行时读取清单和当前后端 sidecar 后，继续调用
`granit_shader_create_from_asset`；该接口不执行文件 I/O。

构建生成的功能变体应写入构建目录，不应把每个组合的产物作为手写源码维护。材质系统根据纹理、
阴影、IBL 和灯光等功能选择资产 ID；部署时复制实际引用的清单及目标后端 sidecar。原始
`granit_shader_create` 仍可接收调用方管理的 SPIR-V/WGSL 字节，接口语义没有变化。

ShaderTools 的 HLSL 编译描述在尾部新增可选 `defines` 和 `define_count`。零初始化描述并设置
`struct_size` 的 0.19 源码可以直接重新编译；需要生成变体时，为每项定义使用
`GRANIT_SHADER_TOOLS_DEFINE_INIT`，名称会排序并进入缓存身份。

## 更新示例路径

仓库内示例代码已归入 `examples/samples/model_viewer` 和 `examples/samples/imgui`。跨示例的 glTF、
SDL3、ImGui、视觉比较和浏览器资源支撑位于 `examples/common`，浏览器验收位于 `tests/web`。
引用旧 `examples/platform` 或根 `web` 路径的仓库脚本需要同步更新；安装包目标和普通 Consumer
不受示例源码移动影响。
