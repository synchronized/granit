<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# R-10：通用资源传输

## 状态

- 设计状态：已确认首期边界
- 实现状态：R-10A、R-10B1～R-10B2、R-10C0～R-10C3 已完成
- 路线图任务：R-10
- 优先级：P1
- 前置依赖：R-03～R-05、F-01～F-05、P-04

## 背景

当前已经具备 Buffer 区域复制、Texture 到 Buffer 复制、同步 Texture 写入和 Upload Batch，但读取
Texture 仍要求调用方手工创建 Readback Buffer、录制、提交、等待、映射并计算行布局。Texture
之间复制、通用 Buffer 到 Texture 命令和 mipmap 生成也没有公共入口。

R-10 补齐常用传输能力，但不把图片编码、资产导入或磁盘 I/O 放入 Renderer。

## 边界

- 公共描述继续使用 Granit 格式、区域、布局和整数句柄，不暴露 Vulkan 类型。
- 高级用户仍可使用 Command Recorder 组合异步流程；便利接口不能取代显式路径。
- 同步读取会等待 GPU，只面向截图、测试、工具和低频诊断，并在名称与文档中明确阻塞行为。
- PNG、JPEG、EXR 等编码由工具或上层资产模块负责；核心只输出带明确格式和行跨度的原始像素。
- 首版不隐式翻转 Y、不做颜色空间转换、不做通道重排，避免便利接口改变资源语义。
- 自动 mipmap 只处理明确支持的非压缩颜色格式；深度/模板和不支持过滤的格式明确失败。

## R-10A：原始像素同步读取

1. 增加后端无关的格式 Footprint 查询，返回块宽、块高和每块字节数。
2. 增加调用方提供目标内存的同步 Texture 区域读取接口，内部复用 Readback Buffer 和既有
   Texture-to-Buffer 状态跟踪。
3. 返回实际行跨度、行数、格式和尺寸；目标容量不足时返回所需大小，不写入部分数据。
4. 覆盖 RGBA8/BGRA8、R8、RG8、RGBA16F、Cube 面和非零 mip；深度/模板首期不支持。
5. 增加 C11、C++20、跨 Renderer、越界、容量查询和真实像素测试。

首期采用同步接口，是因为现有 Recorder 路径已经覆盖异步和批量组合。只有真实截图吞吐测量证明
同步等待成为问题，才增加独立 Readback Batch 或回调式完成对象。

## R-10B：显式复制命令

- 按需求补充 Texture-to-Texture 和 Buffer-to-Texture Recorder 命令。
- 区域显式指定源、目标 mip、数组层、aspect、偏移和范围。
- 录制时完成整批原子校验并保活资源，状态跟踪在渲染区域外生成传输屏障。
- Upload Batch 继续负责复制调用方 CPU 数据，不与 GPU 资源复制混为一个接口。

实施拆分：

1. R-10B1（已完成）：Texture-to-Texture 单区域复制、状态跟踪与真实像素测试。
2. R-10B2（已完成）：公开 Buffer-to-Texture Recorder 命令，复用既有布局与区域语义。
3. R-10B3：出现多区域批量需求后再扩展数组入口，避免未经测量扩大 ABI。

## R-10C：mipmap 生成

- 公共入口接收 Texture、base mip、level count 和数组层范围。
- 首版使用线性 Blit，仅在设备与格式能力明确支持时执行。
- 每一级依赖上一级，状态跟踪处理 Transfer Source/Transfer Destination 转换及最终用途恢复。
- Compute 降采样、法线重归一化、Alpha Coverage 保持和离线高质量滤波属于后续高级策略。

实现前确认当前整图状态缓存无法表达同一 Texture 不同 mip 的并行布局，因此先完成 R-10C0
子资源状态跟踪，再进入能力门禁与 Blit。详细设计见
[R-10C：Mipmap 生成](R-10C-mipmap-generation.md)。

## R-10D：重新评估条件

满足任一条件后，再设计异步 Readback Batch：

- 连续帧截图或视频采集出现可测量的 Queue 等待瓶颈。
- 编辑器拾取、GPU 调试或流式系统需要多个在途读取。
- 现有 Recorder + Readback Buffer 的样板代码在多个外部 Consumer 中重复出现。

## 实施顺序

1. R-10A1（已完成）：格式 Footprint 与纯 CPU 验证。
2. R-10A2（已完成）：同步 Texture Readback C ABI、C++ 包装和像素测试。
3. R-10A3（已完成）：新增原始截图示例与使用指南。
4. R-10B1（已完成）：补齐 Texture-to-Texture 显式 GPU 复制。
5. R-10B2（已完成）：补齐 Buffer-to-Texture 显式 GPU 复制。
6. R-10B3：出现真实批量需求后再增加多区域入口。
7. R-10C0（已完成）：将图像状态跟踪细化到 mip 与数组层范围。
8. R-10C1（已完成）：实现线性 Blit 格式能力门禁。
9. R-10C2（已完成）：实现 mipmap 生成命令。
10. R-10C3（已完成）：补齐非二次幂、数组层和失败路径验证。
11. R-10D：基于测量决定是否增加异步读取抽象。

## 验收标准

- 普通用户无需手工创建 Recorder 和 Readback Buffer，即可低频读取 Texture 原始像素。
- 高级显式路径仍可无额外抽象地组合批量异步复制。
- 格式、尺寸、行跨度、坐标原点、阻塞与所有权语义均有文档和测试。
- 新接口不引入图像编码库，不向公共头文件传播 Vulkan 依赖。
