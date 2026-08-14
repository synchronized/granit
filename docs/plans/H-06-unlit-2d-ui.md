<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# H-06：Unlit、2D 与 UI 渲染路径

## 状态

- 路线图任务：H-06
- 优先级：P2
- 状态：进行中；H-06A～H-06B 已完成，H-06C 第一阶段进行中
- 必需依赖：H-01 Render Graph、H-02 Material、H-07 首版参考管线
- 后续依赖：文字渲染、编辑器覆盖层和调试绘制

## 定位

H-06 提供不参与场景光照的 Unlit 渲染路径，服务简单 3D、Sprite、UI 图片、Gizmo 和调试绘制。
它不是低质量 PBR，也不使用布林–冯作为 UI 回退。PBR 仍通过质量分级降低阴影、IBL、光源数量和
纹理成本；无需受光照的内容才选择 Unlit。

Unlit 的最小颜色模型为“纹理采样 × 顶点颜色 × 材质颜色”，可选 Alpha Cutoff 和透明混合，不读取
法线、Metallic、Roughness、光源、阴影或 IBL。该路径复用通用 Material、Mesh、Render Graph 与
Renderer，不建立第二套资源或句柄系统。

## 合成位置

Unlit 3D 与屏幕 UI 使用同一基础着色模型，但颜色空间和合成阶段不同：

```text
Shadow / PBR / Unlit 3D
          -> HDR 场景
          -> Tone Mapping
          -> UI / Sprite / Debug Overlay
          -> Present
```

- Unlit 3D 默认写入 HDR 场景目标，可使用深度测试，并随场景一起进入 Tone Mapping。
- UI、普通 Sprite 和调试覆盖层默认在 Tone Mapping 后写入显示空间目标，避免受到曝光影响。
- 发光 HUD 或需要融入 HDR 的覆盖层必须显式选择 Tone Mapping 前扩展点，不能由系统猜测。
- UI Pass 默认关闭深度写入，使用预乘 Alpha 混合，并支持 Scissor 裁剪。

## 模块边界

H-06 首版负责：

- Unlit 材质参数、Shader 变体和 Pipeline 状态。
- 纹理图片、顶点颜色、Alpha Cutoff、预乘 Alpha 与不透明模式。
- 正交投影、屏幕空间矩形和可选的简单 3D Mesh 输入。
- Scissor 裁剪、稳定提交顺序和相邻兼容 Draw 的批处理。
- 可独立加入 Render Graph 的 Unlit Pass，以及 H-07 Tone Mapping 后的 UI 扩展点。

H-06 首版不负责：

- 控件树、布局、焦点、输入法、事件分发、动画和皮肤系统。
- 字体解析、字形整形、双向文本、换行和富文本排版。
- 资产数据库、Texture Atlas 打包器或编辑器资源导入。
- 布林–冯材质模型；只有出现明确的旧资产兼容需求后才单独评估。
- 默认依赖 Bindless；首版必须能够使用现有 Bind Group 路径运行。

完整 UI 框架属于引擎或独立上层模块。H-06 只提供高效绘制已经布局完成的矩形、图像、字形和
简单几何所需的渲染基础。

## 建议数据与批处理边界

上层提交不可变的逐帧 Draw List，每项至少包含变换或屏幕矩形、UV 范围、颜色、纹理、Sampler、
裁剪矩形、层级和混合模式。实现可以在保持透明顺序与裁剪语义的前提下合并相邻兼容项，生成共享
Vertex/Index Buffer 或实例数据，并以较少的动态库调用完成录制。

首版不承诺跨不透明/透明边界全局重排。透明内容以调用方提供的稳定顺序为准；只有同一层级、
Pipeline、纹理绑定和 Scissor 兼容时才进行安全合批。后续 Bindless 优化必须由真实 Draw 数量、
绑定切换和 CPU 录制基线触发。

## 实施顺序

1. **H-06A：Unlit 基础材质**——定义颜色、纹理、Alpha 模式和简单 3D/正交输入契约，完成不透明
   与 Alpha Cutoff 离屏像素回归。
2. **H-06B：透明与裁剪**——实现预乘 Alpha、稳定透明顺序、Scissor 和边界测试，验证显示空间
   颜色不会重复进行 Tone Mapping 或 sRGB 编码。
3. **H-06C：Sprite/UI Draw List 与批处理**——建立粗粒度逐帧提交接口、动态几何上传和相邻兼容
   Draw 合批，并记录 100、1,000、10,000 个矩形的 CPU/GPU 基线。
4. **H-06D：参考管线集成**——在 H-07 增加 Tone Mapping 后 UI Pass 扩展点，同时保留独立
   Render Graph 使用方式；覆盖窗口 Resize、多 View 和离屏目标。
5. **H-06E：调试绘制与文字渲染评估**——在基础批处理稳定后评估线框、Gizmo、字形 Atlas 和
   外部文字整形库；未经单独设计，不把完整 UI 或字体系统并入本任务。

## H-06A～H-06B 当前进度

- 已建立共享 Unlit HLSL，基础公式为材质颜色乘以可选纹理和可选顶点色。
- 已建立 `unlit_opaque` 与 `unlit_alpha_cutoff` 材质 Pass；两者均关闭混合，Cutoff 使用
  `clip(alpha - alpha_cutoff)`，不改变不透明像素颜色。
- 基础材质常量布局固定为 32 字节，包含 `base_color` 和 `alpha_cutoff`。HLSL 已预留纹理与顶点色
  编译开关，但基础包暂不声明资源参数，避免当前包级必填语义把无纹理材质变成 `NOT_READY`。
- 已加入独立 Unlit Pass 录制接口，复用 Mesh、Material、Frame/Object 绑定和 Renderer Recorder。
- SPIR-V 反射、材质包构建以及 Opaque/Alpha Cutoff 离屏像素回归均已通过，H-06A 完成。
- 已加入 `unlit_transparent` Pass，使用预乘 Alpha、关闭深度写入并保留深度测试；透明黑清屏使
  输出 Alpha 可继续参与后续合成。
- Unlit Pass 支持显式 Load/Clear 与 Scissor；连续提交按录制顺序合成，不进行破坏透明语义的
  隐式重排。
- RGBA8 UNORM 像素回归覆盖两层透明混合、Scissor 边界及线性字节输出，确认该独立显示空间路径
  不额外执行 Tone Mapping 或 sRGB 编码，H-06B 完成。

## H-06C 当前进度

- 已建立内部 `ui_vertex`，固定为位置、UV 与 RGBA8 UNORM 顶点色，不进入公共 ABI。
- 已建立拥有逐帧顶点、索引和 Draw Item 的 UI Draw List；追加局部索引时统一转换为列表全局索引。
- 合批只发生在相邻且 Texture、Sampler、Scissor、层级完全一致的 Item 之间，不跨项重排透明内容。
- 已覆盖索引修正、稳定顺序、相邻合批、状态切换、无效几何、空列表和列表复用测试。
- 已建立逐帧 GPU 几何上传对象，使用可增长并复用容量的 Upload Vertex/Index Buffer；空列表不会
  反复释放容量，移动和析构负责句柄生命周期。首版先采用直接可写内存，是否改为暂存复制由基线决定。
- 已加入 `unlit_ui` 材质 Pass 与 UI 顶点入口：以 `uint32` 输入 RGBA8 顶点色并在 Shader 中解包，
  因而无需为首版扩展公共 Vertex Format ABI。
- UI Pass 在单个 Rendering 区域中绑定一次共享 Vertex/Index Buffer，再按 Batch 更新 Scissor 并录制
  Indexed Draw；离屏像素回归已覆盖稳定透明叠加、顶点色和裁剪边界。
- 已拆出独立 `unlit_ui` 材质包，声明 Base Color Texture 与 Sampler，不让纯色 Unlit 材质被迫提供
  UI 资源；UI Shader 计算“材质颜色 × 纹理 × 顶点色”。
- 每个 Batch 显式更新 Texture/Sampler Bind Group，并通过两张纹理的透明叠加像素回归验证绑定切换。
  当前实现优先闭合语义，后续基线将决定是否引入按资源组合缓存的 Bind Group，避免凭经验复杂化。
- 下一步补充 100、1,000、10,000 个矩形的 CPU/GPU 基线和 Draw/绑定统计，完成 H-06C 验收。

## 验收标准

- 公共头文件和材质描述不暴露 Vulkan 类型。
- 同一 Unlit 基础路径可覆盖简单 3D、Sprite 与 UI 图片，不复制资源系统。
- UI 默认在 Tone Mapping 后合成，并通过像素测试验证 Alpha 和颜色空间。
- Scissor、透明顺序、空 Draw List、无效资源和生命周期边界均有测试。
- 批处理减少动态库调用和 Draw/绑定次数，并提供可复现基线，而不是仅凭经验引入复杂缓存。
- 初级用户可通过默认 Pipeline 提交 UI，中高级用户可将 Unlit Pass 插入自定义 Render Graph，
  高级用户仍可直接通过 Renderer 实现完全自定义路径。
