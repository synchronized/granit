<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# F-11：Canvas 绑定缓存与多纹理录制

## 状态

- 设计状态：草案
- 实现状态：未开始
- 优先级：P1
- 前置依赖：F-10、D-03、H-02、H-06

## 背景与目标

F-10 的 Release 测量表明，ImGui 场景的持久映射几何上传和 flush 不是主要瓶颈。完全精简的三槽
帧循环中，Canvas Record 约为 0.104 ms；开启 Validation 后约为 0.353 ms。100 个兼容 Item 合并
为一个 Batch 时 GPU 时间约 0.033 ms，而 100 个交替纹理 Batch 约为 0.811 ms。

当前 `record_canvas_pass` 每帧创建 Frame/Object 绑定，并通过修改共享 Material 切换纹理。纹理变化
可能要求结束 Dynamic Rendering、更新 Material、重新获取 Draw State 和再次开始 Rendering。这既
增加 CPU 绑定构建成本，也放大多纹理 GPU 成本。

本任务目标是让 Canvas 复用逐帧绑定，并在开始 Rendering 前准备全部纹理资源，使录制期间只切换
已经存在的 Bind Group、Scissor 和 Draw，整帧保持一次 Dynamic Rendering 区间。

## 非目标

- 不实现全局 Bindless Resource Table；F-11 只缓存 Canvas 使用的显式 Bind Group。
- 不改变 Canvas Draw List 的公开几何、排序、裁剪或透明混合语义。
- 不把 Vulkan Descriptor Set、Pipeline Layout 或资源屏障暴露到公共 API。
- 不以增加 frames-in-flight 掩盖绑定或提交成本。
- 不在没有数据时改写通用 Material 系统或建立第二套 Material 运行时。
- 不优化 Swapchain acquire 的驱动等待；该指标继续独立报告。

## 已确认决策

### 先实现内部缓存，不扩大公共 ABI

Canvas 已能从创建描述获得 1～4 个帧槽，并从 Frame Context 接收真实 `frame_slot`。首版直接在
Canvas 内部状态中维护缓存，不新增公共句柄或用户可见生命周期。若其他渲染组件出现相同需求，再
根据共性提取内部通用缓存。

### 缓存按 Renderer、布局和资源身份区分

纹理绑定缓存的逻辑键至少包含：

- 所属 Renderer；
- Pipeline Layout 与绑定组位置；
- Texture View 句柄及其 generation；
- Sampler 句柄及其 generation；
- 影响绑定布局的 Material/Pass 变体。

不能把裸句柄值视为永久身份。资源销毁、句柄槽复用、Material 热重载、Pipeline Layout 变化或
Renderer 销毁后，旧缓存项必须失效。缓存应持有必要的内部资源引用，但不能改变公开资源所有权。

### 逐帧常量按真实帧槽复用

Frame/Object 常量及其 Bind Group 按 Canvas 的 `frame_slot_count` 建立槽数组。每次 record 只更新
当前真实槽位，且只在 Frame Context 已等待该槽完成后覆盖。自动槽位模式继续服务同步离屏路径，
实时窗口必须使用 Frame Context 返回的槽位。

### 资源准备发生在 Dynamic Rendering 之前

录制前扫描本帧 Batch，收集唯一的 Texture View 与 Sampler 组合，并完成：

1. 句柄与所属 Renderer 校验；
2. 必要的资源状态转换；
3. Bind Group 查询或创建；
4. Pipeline 与公共 Frame/Object 绑定准备。

进入 Dynamic Rendering 后只允许绑定已准备的组、设置 Scissor 和发出 Draw。纹理变化不得再触发
`end_rendering → material_update → begin_rendering`。

### 缓存必须有界并可回收

首版采用按 Canvas 实例所有的有界缓存。应记录最近使用序号，并在超过容量或资源失效时回收；默认
容量由代表性 ImGui/工具场景确定，不能无限保存用户曾经出现过的 Texture ID。Canvas 销毁时释放
全部缓存项，Renderer 级联销毁仍遵守现有句柄失效顺序。

## 实施顺序

1. **F-11A 基准与契约**：固化兼容单纹理、交替双纹理、多纹理和资源销毁重建基准；记录 CPU
   upload、binding prepare、record、submit 及 GPU 时间。
2. **F-11B 逐帧公共绑定**：按 1～4 个真实帧槽复用 Frame/Object 常量与 Bind Group，覆盖自动槽位
   和错误槽位路径。
3. **F-11C 纹理绑定缓存**：按资源身份缓存 Texture/Sampler Bind Group，处理 generation、跨
   Renderer、Material 重载和容量回收。
4. **F-11D 单 Rendering 区间**：把全部资源准备移到 begin rendering 前，纹理切换时只绑定已有
   Bind Group，不结束 Dynamic Rendering。
5. **F-11E ImGui 验证**：验证字体纹理、自定义 Texture ID、裁剪、窗口重排和资源销毁；重复测量
   1～4 个帧槽及 Validation 开关。
6. **F-11F 收尾**：更新 Canvas Reference、性能 Record 和路线图；共享/静态、Windows/Linux、
   C/C++ Consumer 与安装导出全部通过后合并。

## 测试与验收

- 单纹理兼容批次的输出、Batch 数和透明顺序不变。
- 多纹理帧只开始和结束一次 Dynamic Rendering；纹理切换不调用 Material Update。
- 覆盖 1、2、3、4 个帧槽，旧槽只在对应 GPU 工作完成后覆盖。
- 覆盖无效 Texture/Sampler、跨 Renderer、generation 失效和资源销毁后重建。
- 覆盖缓存命中、未命中、容量回收、Canvas/Renderer 销毁与 Material 热重载。
- Validation Layer 不报告 Descriptor、Layout、资源生命周期或同步错误。
- 100 个交替纹理 Batch 的 GPU p50 相比 F-10 基线 0.811 ms 明显下降，目标不高于 0.20 ms。
- ImGui Demo Release 三槽 Canvas Record 相比完整功能基线 0.353 ms 至少下降 30%。
- 不使单纹理兼容路径、Draw List 构建或几何上传 p95 回退超过 10%。
- 同时报告 FPS、CPU/GPU 时间、槽位等待和输入延迟风险，不以单一吞吐指标验收。

## 风险与未决问题

- Material Parameter 当前以 Material 为更新单位；若无法安全获取独立纹理 Bind Group，可能需要先
  提取内部不可变 Material Binding 模板，但仍不应扩大公共 ABI。
- 资源状态跟踪是否允许在一次 Recorder 中于 Rendering 前批量准备全部采样纹理，需要原型验证；
  若状态转换仍隐式依赖 Material Update，应先修正内部准备接口。
- 缓存持有内部资源引用可能延迟 Texture View/Sampler 的真实释放，需要明确容量上限和回收点。
- 四槽吞吐更高但潜在输入延迟更大；F-11 不修改 Renderer 默认槽数。
- 若多纹理场景最终仍受 Descriptor 更新上限限制，再单独恢复 D-09 Bindless 评估，不能在 F-11 中
  顺带引入全局 Bindless 架构。
