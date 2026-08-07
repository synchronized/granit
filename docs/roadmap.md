<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 路线图

路线图用于约束实现顺序和阶段边界，不构成版本或发布日期承诺。项目进入稳定版本前，公共 API
和 ABI 仍可根据实现验证调整；每次调整都必须同步 C API、C++ 包装、测试和文档。

## 当前进度

| 阶段 | 状态 | 说明 |
| --- | --- | --- |
| 一、工程与 ABI 基础 | 基本完成 | 构建、公共接口分层、句柄、测试体系已建立 |
| 二、Vulkan 与窗口输出基础 | 基本完成 | Renderer、Win32 Surface、Swapchain 生命周期已实现 |
| 三、GPU 资源基础 | 下一阶段 | 内存策略及 Buffer、Texture、Sampler 尚未实现 |
| 四、命令与帧同步 | 未开始 | Command Recorder、同步和 acquire/present 尚未实现 |
| 五、基础渲染 | 未开始 | Shader、Pipeline 和 Dynamic Rendering 尚未实现 |
| 六、多线程与性能 | 未开始 | 在真实访问模式形成后细化 |
| 七、稳定化与跨平台 | 持续进行 | Linux 窗口系统和 ABI 稳定策略留待后续 |

## 阶段一：工程与 ABI 基础

**状态：基本完成。**

### 已交付

- C11 ABI、C++20 RAII 包装、共享库导出和可选静态构建。
- 64 位整数句柄，以及类型、generation 和 Renderer domain 校验。
- 结果码、错误文本、描述结构 `struct_size` 兼容规则。
- CMake preset、安装包导出、严格警告和格式配置。
- Unity 纯 C API 测试、Catch2 C++/内部测试和独立公共头编译测试。

### 后续补充

- 日志回调和自定义分配器接口。
- 安装后 C/C++ consumer 自动化测试。
- 公共 API/ABI 版本和兼容策略。

### 验收标准

- 动态库和静态库均可由 C 与 C++ consumer 使用。
- 公共头文件不包含 Vulkan 或平台 SDK 头文件。
- 动态库只导出明确声明的 C ABI 符号。

## 阶段二：Vulkan 与窗口输出基础

**状态：基本完成。**

### 已交付

- Vulkan Loader、Vulkan 1.3 Instance、物理设备筛选、逻辑设备和 graphics queue。
- 独立 Instance/Device Volk 函数表及后端错误映射。
- Win32 Surface，以及平台扩展和呈现能力的按需启用。
- Swapchain 创建、查询、原子重建、呈现模式回退和级联销毁。
- Registry 共享所有权和 Renderer 局部资源锁；Vulkan 创建和销毁不占用 Registry 锁。

### 暂不包含

- Swapchain 图像获取和呈现。
- Linux、macOS 或移动平台 Surface。
- 面向普通用户的 Vulkan 原生互操作。

### 验收标准

- 能在真实 Win32 窗口创建、重建并销毁 Swapchain。
- 错误 Renderer domain、资源类型和失效句柄均被拒绝。
- 销毁 Renderer 或 Surface 时按正确顺序清理所有子对象。

## 阶段三：GPU 资源基础

**状态：下一阶段。前置依赖：阶段一和阶段二。**

### 目标与交付物

1. 确定内存分配策略，并决定使用自研分配层还是引入 Vulkan Memory Allocator。
2. 定义平台无关的资源用途、内存位置、像素格式和尺寸类型。
3. 实现 Buffer 创建、销毁、映射和初始数据上传。
4. 实现 Texture、Texture View 和 Sampler 生命周期。
5. 将 Swapchain 图像建立为内部非拥有 Texture/View，不能由普通资源销毁接口释放。
6. 建立延迟销毁基础，避免释放仍被 GPU 使用的资源。

### 设计约束

- 公共 API 不出现 `VkBuffer`、`VkImage`、Vulkan usage flag 或内存类型索引。
- 描述结构使用 Granit 自己的定宽枚举和位标志。
- 同一 Renderer 的资源可从不同线程创建；同一资源的写操作必须定义同步要求。
- 高频上传不能设计成每小段数据一次动态库调用，应支持批量或上传上下文。

### 验收标准

- 覆盖创建、映射、上传、错误 domain、重复销毁和父对象级联销毁。
- 至少验证 host-visible Buffer 写入及 device-local Buffer 上传。
- Texture 格式和用途经过能力检查，错误组合返回稳定的 Granit 结果码。

### 暂不包含

- Shader、Pipeline 和渲染命令。
- 面向使用者的线程池或任务系统。

## 阶段四：命令与帧同步

**状态：未开始。前置依赖：阶段三的 Buffer、Texture View 和延迟销毁基础。**

### 目标与交付物

- Command Pool 和线程局部 Command Recorder。
- Fence、Semaphore 及每帧上下文的内部抽象。
- 可配置 frames-in-flight 和 Queue 提交串行化。
- 资源状态转换和必要的同步信息记录。
- Swapchain acquire、提交、present 和 out-of-date 重建流程。
- 窗口最小化、Surface 丢失和 Device Lost 的恢复边界。

### 线程模型

- 不同 Command Recorder 可以在不同线程并行记录。
- Vulkan Queue 的外部同步由 Granit 内部保证。
- Registry 锁只管理身份和所有权，不承担 GPU 同步。
- Renderer/Surface/Swapchain 销毁不得与对应资源操作并发；如需放宽，必须增加明确的关闭状态。

### 验收标准

- 多帧循环可以稳定 acquire、提交和 present。
- resize、最小化和恢复不会泄漏资源或使用旧 Swapchain 图像。
- 同一 Renderer 的多线程命令记录不依赖一把全局大锁。

## 阶段五：基础渲染

**状态：未开始。前置依赖：阶段三和阶段四。**

### 目标与交付物

- Shader 模块和离线/运行时着色器输入策略。
- Graphics Pipeline、资源绑定和 Pipeline Layout 抽象。
- 基于 Vulkan 1.3 Dynamic Rendering 的 Render Target 和绘制流程。
- Viewport、Scissor、Clear、Vertex/Index Buffer 和基础 Draw API。
- 最小三角形、清屏和离屏渲染示例。

### 验收标准

- 示例仅使用 Granit 公共接口，不包含 Vulkan 头文件。
- 动态库调用粒度适合命令批量记录，不为每个底层状态产生不必要的 ABI 往返。
- Validation Layer 下完成常规渲染和退出，不产生生命周期或同步错误。

## 阶段六：多线程与性能

**状态：未开始。前置依赖：形成可测量的完整帧路径。**

### 目标与交付物

- 并行命令记录、资源创建和上传压力测试。
- 句柄表、资源锁、Queue 锁和延迟销毁队列的基准测试。
- 根据测量结果调整锁粒度、缓存和批量 API。
- 评估是否需要内部线程池；默认不要求使用者采用 Granit 的任务系统。

### 验收标准

- 明确记录所有公开对象的线程安全级别。
- 不同资源上的独立操作不存在无意义的全局串行化。
- 性能调整有可复现基准数据，不以经验猜测替代测量。

## 阶段七：稳定化与跨平台

**状态：持续进行。**

### 目标与交付物

- Wayland、XCB 或其他经过确认的平台 Surface 实现。
- ABI 兼容检查、导出符号检查和旧描述结构回归测试。
- 日志、诊断、崩溃定位和 Device Lost 报告能力。
- 安装、包管理器集成和真实外部项目验证。
- 明确标记为不稳定的高级 Vulkan 互操作接口评估。

### 验收标准

- 支持的平台拥有构建、运行和窗口重建测试。
- 普通用户路径始终不依赖 Vulkan SDK。
- 稳定版本发布前形成书面的 API/ABI 兼容承诺。

## 近期执行顺序

1. 调研并确定 GPU 内存分配方案。
2. 设计公共 Buffer API 和内部 Buffer 资源对象。
3. 实现 Buffer 映射、上传、句柄归属和生命周期测试。
4. 在 Buffer 经验基础上设计 Texture、Texture View 和 Sampler。
5. 再进入 Command Recorder、帧同步和 Swapchain acquire/present。

若实现过程中发现前置抽象不足，应先更新本路线图和对应设计文档，再扩大公共 API。
