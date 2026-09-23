<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# RenderPipeline component 契约

本文汇总 `RenderPipeline` component 的所有权、错误、结构扩展和依赖边界。渲染阶段、材质参数、
几何格式与文字缓存等具体行为仍以各专题参考页为准。

## Component 与依赖

- CMake component 名为 `RenderPipeline`，公共目标为 `granit::render_pipeline`。
- 该目标依赖核心 `granit::granit`；使用者不需要链接或包含 `src/` 内的 Render Graph、材质、场景
  和光照实现目标。
- 公共 C ABI 位于 `granit/pipeline/*.h`，使用 `GRANIT_RENDER_PIPELINE_API` 从独立动态库导出。
- RenderPipeline 的稳定等级独立于 Core；当前 0.x 接口可检测但尚未冻结。

## 公共渲染资产

安装 `RenderPipeline` component 时会同时安装公共渲染资产。安装包、`FetchContent` 和
`add_subdirectory` 三种消费方式都提供同名 CMake 变量 `granit_RENDER_PIPELINE_ASSET_DIR`；它指向
不依赖 Granit 私有源码目录的资产根目录。标准 PBR Shader Library 位于
`libraries/pbr_standard.grshlib`，标准 PBR Material 位于 `materials/pbr_standard.grmat`。运行时
部署集合为：

```text
libraries/pbr_standard.grshlib
materials/pbr_standard.grmat
```

Library 同时包含 Vulkan 与 WebGPU 载荷，Renderer 在运行时选择；`.grshaderobj` 及 sidecar 只属于
离线工具链中间结果。变量只负责定位资产，不改变 Core 的资源边界；应用仍负责读取、嵌入或通过
自己的资产系统提供相应字节。构建树变量指向 Granit 二进制目录中的资产副本，因此上游不需要
推导源码目录。

Render Pipeline 内部的 Tone Mapping、Shadow、Canvas 和 Debug Draw 也使用 Shader Library。
这些私有 Library 随 component 内嵌，不属于安装资产，也不要求应用加载。普通本机构建从 Shader
输入确定性生成归档并校验快照；无法执行宿主 Shader Tool 的交叉构建使用同一份已校验快照。

`environments` 子目录同时提供 GRENV v3 环境资产。其所有权、完整性校验和逐帧借用规则见
[Environment Map](environment-map.md)。

## 对象所有权

| 对象 | 自身拥有 | 借用及调用方责任 |
|---|---|---|
| Render Pipeline | 内建 Shader Library、Shader、默认 IBL、缓存和临时 GPU 资源 | Renderer、创建回调及 `user_data` |
| Mesh | 复制后的布局与绘制范围 | Vertex/Index Buffer |
| Material | 参数状态和 GPU 实例 | 参数引用的 Texture View 与 Sampler |
| Scene Snapshot | View、Renderable 和光源的值数据副本 | 不借用外部场景对象；`payload` 只是应用值 |
| Canvas Draw List | 顶点、索引、Item 和内部录制缓存 | Item 引用的 Texture View 与 Sampler |
| Debug Draw List | 调试命令、内部白纹理和录制缓存 | 转换目标、Recorder 与附件 |
| Text Draw List | 字形 Run 与字形值数据 | 转换期间使用的 Text Atlas 和 Canvas |
| Text Atlas | 字形元数据、页面 Texture、View 与 Sampler | 上传调用期间的 R8 位图数据 |

所有对象都属于创建它们的 Renderer。跨 Renderer 使用、类型错误、已销毁句柄及重复销毁返回
`GRANIT_ERROR_INVALID_HANDLE`。借用资源必须至少存活到最后一次使用完成；销毁上层对象不会销毁
表中列出的借用对象。

## 输入内存与回调

- Scene、Mesh、Draw List、Text Atlas 和 Material 创建/更新接口会在返回前复制需要保留的数组、
  归档或位图数据；输入指针只在调用期间有效。
- Render Pipeline 的 Draw Binding、输出数组与渲染描述只在 `render` 调用期间借用。
- 录制回调同步发生在 `render` 调用内。回调上下文、数组和临时 Texture View 仅在本次回调有效，
  不得保存，也不得结束、提交或销毁传入 Recorder。
- 创建时的回调函数和 `user_data` 必须覆盖 Pipeline 全生命周期；同一 Pipeline 不得从回调递归
  渲染。

## 错误与事务边界

- 参数、数组长度、格式、范围、保留字段或状态不合法时返回
  `GRANIT_ERROR_INVALID_ARGUMENT`；合法但当前后端不支持的请求返回
  `GRANIT_ERROR_UNSUPPORTED`。
- 同一 Render Pipeline 的并发 `render` 返回 `GRANIT_ERROR_NOT_READY`；Text Atlas 查询尚未上传的
  字形也返回该结果。调用者不应把两者解释为同一业务状态。
- Material 批量更新具有事务性，失败时保留旧参数。Pipeline 回调返回首个错误时终止本次渲染，
  未完成的 Recorder 不会提交。
- Atlas 达到页数上限或内部容量分配失败返回 `GRANIT_ERROR_OUT_OF_MEMORY`。
- 除各专题明确说明的部分追加操作外，创建失败不产生句柄，调用者只在成功后读取输出。

## 描述结构扩展

- 调用者应使用对应 `*_INIT` 宏，使 `struct_size`、保留字段和默认值与当前头文件匹配。
- 每个带 `struct_size` 的公开结构都提供固定的 `*_VERSION_1_SIZE`。库按最低版本尺寸校验，
  只读取 `struct_size` 覆盖的已知字段；未知尾部被忽略，保留字段必须为零。
- 兼容扩展只能在结构末尾追加字段，不得重排、改型或重新解释已有字段。稳定前若确需破坏性
  调整，必须按[版本与兼容策略](compatibility.md)更新版本、快照和迁移说明。

## 坐标约定

RenderPipeline、低层 Renderer、Canvas 和纹理统一遵循[坐标系统约定](coordinates.md)。调用方提交的
View Projection 不需要根据 Renderer backend 修改；后端负责 Viewport 方向和正面绕序转换。图片
解码或外部纹理需要转换时应在上传路径处理，不能通过修改相机投影补偿。

## 线程安全

不同的不可变对象可以并行读取，不同 Draw List 或 Material 可以由不同线程独立构建或更新。同一
可变对象的 clear、append、update、upload、record、render、统计与销毁需要调用方外部同步；任何
对象销毁都不得与正在使用它的渲染或转换调用并发。
