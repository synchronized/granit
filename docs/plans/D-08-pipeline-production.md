<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# D-08：Graphics Pipeline 完整状态、缓存与重载边界

## 元数据

- 设计状态：已确认
- 实现状态：实现中（D-08A1、D-08A2、D-08A3 已完成）
- 路线图任务：D-08
- 优先级：P1
- 前置依赖：D-03、D-05、D-06、D-07
- 后续依赖：阶段六性能任务、Asset 与 Scene 高层模块

## 背景

当前 Graphics Pipeline 只支持最小三角形：无 Vertex Attribute Layout，Primitive、Rasterization、
Depth/Stencil 和 Blend 状态大多由后端写死。如果先实现 Pipeline Cache，后续扩展描述字段时仍需
重做缓存键和兼容校验。因此 D-08 先稳定常用 Pipeline 状态，再接入缓存和并发创建边界。

## 分步实施

### D-08A：Graphics Pipeline 常用状态

新增后端无关的值类型：

- Vertex Format、Vertex Attribute、Vertex Buffer Layout 和 per-vertex/per-instance 步进模式；
- Primitive Topology、Front Face、Cull Mode 和 Polygon Mode；
- Depth Test、Depth Write、Compare Operation；
- 每个颜色附件独立的 Blend Factor、Blend Operation 和 Color Write Mask。

Viewport 和 Scissor 继续保持动态状态。第一版不加入 Tessellation、Geometry Shader、Logic Op、
Conservative Rasterization、Depth Bias、独立模板正反面状态和自定义 Sample Mask。

Graphics Pipeline 描述继续包含 `struct_size`。新增字段追加在现有版本末尾；旧尺寸采用当前默认值：

- Triangle List；
- Fill、无剔除、逆时针正面；
- 有深度格式时启用深度测试和写入，比较操作为 Less Or Equal；
- 禁用 Blend，RGBA 全部写入。

### D-08B：Pipeline Cache

- Pipeline Cache 是 Renderer 内部共享对象，Graphics 与 Compute Pipeline 共用。
- 初始版本允许传入一段只借用到调用结束的缓存数据，并通过“查询大小→填充缓冲区”导出。
- 缓存数据是设备和驱动相关的临时加速数据，不属于稳定资产格式；加载不兼容数据时忽略并返回
  明确诊断，不影响 Pipeline 正常创建。
- 缓存导入导出采用成对 API，内存始终由调用者分配，避免跨 DLL 释放。
- Pipeline Cache 自身不替代 Granit 的描述哈希；后续内存级 Pipeline 对象复用需要单独测量。

### D-08C：并发与异步创建边界

- Pipeline 同步创建函数必须允许从用户工作线程并发调用。
- 第一版不内置线程池、不返回 Future，也不要求用户接入 Granit 任务系统。
- Renderer 只在访问共享 Vulkan Pipeline Cache 时串行化必要区段，不持有 Registry 全局锁执行
  昂贵的 Pipeline 编译。
- 建立并发创建压力测试后，再决定是否提供可取消的异步任务 API。

### D-08D：Shader 热重载边界

- Shader、Pipeline Layout、Bind Group Layout 和 Pipeline 都保持不可变。
- 热重载由上层创建新 Shader 和新 Pipeline，成功后原子替换上层引用；失败时继续使用旧 Pipeline。
- Granit 不监视文件、不决定资源路径，也不在 Renderer 核心中加入 Asset 数据库。
- 已录制或在途 Recorder 继续持有旧 Pipeline，按真实 GPU 完成点安全退役。
- Shader 反射布局变化时必须重新创建相关 Layout 与 Bind Group，不能原地覆盖不兼容对象。

## ABI 与验证

- 公共枚举不复用 Vulkan 数值；C ABI 只使用定宽整数、显式结构和指针加数量。
- Vertex Attribute location 不得重复，offset 与 format 必须落在 stride 内。
- Buffer binding 数、attribute 数、stride 和格式必须校验设备限制。
- Blend 状态数量必须为零（使用默认值）或与颜色附件数量一致。
- 无深度格式时禁止启用深度测试或写入。
- 所有输入数组由创建函数复制，调用返回后不保留调用者指针。

## 测试与验收

- C11 和 C++20 分层公共头文件独立编译，安装 FILE_SET 保持正确。
- Validation Layer 下以真实 Vertex Buffer 绘制窗口三角形。
- 覆盖 per-instance 输入、背面剔除、深度开关、Alpha Blend 和非法状态组合。
- Pipeline Cache 可导出、重新导入，并在缓存为空或不兼容时安全回退。
- 多线程并发创建 Graphics/Compute Pipeline，不发生 Registry 锁反转或对象提前销毁。
- Clang 共享库、Visual Studio 共享库和 Clang 静态库三套矩阵通过。

## 实施顺序

1. D-08A1 / 已完成：Vertex Format、Attribute 和 Buffer Layout。
2. D-08A2 / 已完成：Primitive 与 Rasterization 状态。
3. D-08A3 / 已完成：Depth/Stencil 基础状态和 Color Blend。
4. 增加可见的窗口 Vertex Buffer 三角形示例。
5. D-08B：Pipeline Cache 导入导出。
6. D-08C：并发创建测试与锁粒度调整。
7. D-08D：记录并验证不可变对象热替换边界。
