<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 10：完成跨后端 Model Viewer

本章把 09 的同步单模型程序扩展为完整 Model Viewer。最终程序在桌面 Vulkan 和浏览器 WebGPU 上
共用 CPU Scene、GPU Scene、相机、渲染提交与状态机；平台壳层只处理窗口事件、文件或网络输入和
逐帧调度。

完成后可以加载 Flight Helmet 或其他 glTF/GLB，检查节点与 PBR 材质，调整环境光和渲染质量，并
通过轨道相机观察模型。

## 1. 用状态机替换阻塞启动

09 在创建 Renderer 前同步读取模型。完整应用把过程拆为可观察且可取消的阶段：

```text
platform_ready
  → renderer_pending
  → asset_loading
  → gpu_upload
  → ready
  ↘ failed
```

桌面入口可以在线程中读取文件和生成 GPU 上传计划；浏览器入口使用 Fetch 获取主文档及其相对资源。
两条路径最终都向 `application_core` 提交同一种 CPU Scene 和 `gpu_scene_plan`，渲染代码不判断平台。

GPU 对象仍只在拥有 Renderer 的线程创建。后台任务只处理自有字节、CPU Scene 和不含句柄的上传
计划，从而避免在线程或动态库边界传递临时 GPU 状态。

## 2. 复用 Application Core

Application Core 集中拥有以下长期状态：

- glTF CPU Scene 与 GPU Scene；
- 轨道相机、选中节点和材质编辑状态；
- 环境贴图和环境旋转、强度；
- 性能历史与当前渲染质量；
- 当前阶段、失败结果和诊断。

平台壳每帧提交后端无关输入，Core 返回不可变 `frame_packet`。Packet 保存当前帧的 Snapshot、Draw
Binding、环境参数、Canvas 数据和输出尺寸，不借用下一帧会修改的容器。

```text
Window/Input → application_tick_input → Application Core
Application Core → frame_packet → Frame Executor → Render Pipeline
```

这种边界允许桌面直接执行 Packet，也允许应用以后把 Packet 交给独立渲染线程。

## 3. 加入轨道相机和检查器

轨道相机根据模型 Bounds 自动聚焦，并提供旋转、平移、缩放、Home 和重新聚焦。输入先经过统一
Window 事件，再由桌面 SDL3 或 Web 适配器转换成同一种 `viewer_input`；UI 捕获指针时，相机不会
同时响应拖动。

ImGui 检查器展示：

- Scene 节点树与当前选中对象；
- Base Color、Metallic、Roughness、Normal、Occlusion 和 Emissive；
- 实际纹理预览与 Sampler 信息；
- 方向光、环境强度、环境旋转和曝光；
- MSAA、FXAA、Specular AA、各向异性及实际生效值；
- CPU、GPU、Present 和帧槽等待历史。

材质编辑先更新 GPU 参数，成功后才同步 CPU Scene。失败不会让 Inspector 与画面分别保存不同值。

## 4. 环境光与完整 PBR

没有外部环境资产时，程序使用内置低分辨率摄影棚环境；传入 `.grenv` 后，Render Pipeline 使用其中
的 Irradiance Cube、Prefiltered Specular Cube 和 BRDF LUT。环境资源由 Granit 公共接口加载，KTX2
辅助解析只用于示例验收，不形成第二套运行时环境格式。

GPU Scene 为每个 glTF Material 建立标准 PBR Material Instance，并处理纹理颜色空间、默认纹理、
Mipmap、Sampler 去重、Normal Scale、Occlusion Strength 和 Emissive Factor。缺少可选纹理时使用
符合材质语义的默认值。

## 5. 桌面与浏览器入口

完整源码位于
[`examples/tutorials/10_model_viewer`](../../examples/tutorials/10_model_viewer)。面向使用者的目标是：

- 桌面：`granit_tutorial_10_model_viewer`
- 浏览器：`granit_tutorial_10_model_viewer_web`
- 离屏验收：`granit_tutorial_10_model_viewer_offscreen_acceptance`

模型获取、桌面参数、环境资产、性能采样、截图和浏览器服务步骤统一由
[Model Viewer 运行指南](../guides/model-viewer.md)维护，本章不复制会随工具变化的命令清单。

浏览器页面可用 `?model=<URL>` 覆盖默认 Flight Helmet。主文档、Buffer 和 Image 必须由 HTTP(S)
提供并允许跨域访问；页面不能通过 `file://` 直接运行。

## 6. 验收与所有权

迁移后的自动验证继续覆盖：

- glTF URI、解析、GPU Scene 计划和事务式上传；
- 轨道相机、查看器状态、材质面板和性能历史；
- 桌面参数、SDL3 输入和呈现恢复策略；
- 固定模型离屏截图与像素比较；
- 浏览器 Fetch、输入、Resize、多帧提交和显式资源释放。

退出时先停止资产任务和帧生产，再释放 Packet、Scene、Canvas、Environment、GPU Scene 和 Render
Pipeline，最后销毁 Swapchain、Renderer 与平台对象。浏览器异步回调也必须在对象销毁前失效，不能
访问已经释放的用户数据。

至此，教程路线完成了从空窗口到真实跨后端 PBR 工具的完整闭环。

[上一章：加载 glTF 模型](09-model-loading.md) · [返回教程目录](README.md)
