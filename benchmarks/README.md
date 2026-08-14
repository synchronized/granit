<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# Granit Benchmarks

Benchmark 是显式启用的开发目标，不属于普通测试、安装或公共 API。目录结构借鉴 `caors-core`，
但不依赖其使用的 CTrack；当前轻量 runner 只提供 Granit P-02 所需的预热、采样、百分位数、CLI
和 CSV 输出。

建议使用 Release preset：

```powershell
cmake --preset windows-clang-release -DGRANIT_BUILD_BENCHMARKS=ON
cmake --build --preset windows-clang-release --target granit_benchmarks
./build/windows-clang-release/bin/granit_benchmarks.exe
```

快速验证指定用例：

```powershell
./build/windows-clang-release/bin/granit_benchmarks.exe `
  --case find_hit --threads 1 --iterations 1000000 --samples 30 --warmup 5 --table-size 10000
```

使用 `--help` 查看全部参数。CSV 数据写入标准输出，checksum 等诊断写入标准错误；保存基线时应
分别重定向，避免诊断文字混入数据文件。

当前 P-02A 用例包括命中查询、错误类型、错误 domain、旧 generation 句柄，以及插入/删除槽位
复用。多线程模式为每个线程创建独立句柄表，不把当前非线程安全的内部 `handle_table` 当作共享表。

`granit_renderer_benchmarks` 用于需要 Vulkan Renderer 的基准，当前 P-02B 包含：

- `invalid_lookup`：并发执行会经过 Registry 全局锁的无效 Buffer 句柄查询。
- `create_destroy`：在同一 Renderer 上并发创建和销毁独立 upload Buffer。
- `independent_write`：每个线程写入自己的 upload Buffer，覆盖 Registry 查找和资源级锁路径。
- `recorder_create_destroy`：并发创建和销毁独立 Command Recorder。
- `empty_record`：复用独立 Recorder 执行 begin/end/reset 空录制周期。
- `buffer_record`：每个录制周期记录可配置数量的 Fill Buffer 命令，再 end/reset。
- `mixed_pipeline_record`：混合记录 Graphics、Dynamic Rendering 和 Compute 命令。
- `queue_submit`：逐个提交预录制的 Recorder，记录单次调用延迟分布。
- `queue_submit_batch`：一次批量提交同组 Recorder，延迟按 Recorder 数归一化。
- `staging_buffer_upload`、`staging_texture_upload`：同步 staging 上传固定成本。
- `batch_buffer_upload`、`batch_texture_upload`：将 `--uploads` 次写入合并到一个 Upload Batch，
  结果按单次写入归一化。

这些场景不对同一个 Buffer 进行无序并发写入，因为那不属于公开 API 支持的工作流。Renderer 环境
不可用时程序返回非零状态并在标准错误中说明原因。

`granit_render_graph_benchmarks` 用于 H-01E，包括纯 CPU `graph_compile`、直接空 Recorder
`direct_execute`、导入资源图 `graph_execute` 和瞬态 Buffer 图 `transient_execute`。使用
`--passes` 控制图规模，执行类结果表示每张图的成本，编译结果表示每次编译的成本。

`granit_pbr_benchmarks` 用于 H-03F，测量把显式 View、方向光和指定数量 Object 打包为 PBR 常量、
加入 Render Graph 并完成图编译的 CPU 成本。使用 `--objects` 控制每个 Pass 的对象数量；该用例
不包含 GPU 录制、提交和实际 Draw。

`granit_scene_benchmarks` 用于 H-04E，测量两个 View 共享场景数据时的快照复制、Frustum、层掩码、
稳定排序和方向光筛选总成本。使用 `--objects` 选择 100、1,000 或 10,000 对象；数据固定为约四分
之一位于 Frustum 外，并在两个层之间交错分布。

`granit_lighting_benchmarks` 用于 H-05E，首个用例隔离测量单 View 可见点光转换为 GPU 布局的 CPU
成本。使用 `--lights` 选择 1、16、64 或 128 个光源；Snapshot 在计时区间外构建，结果包含输出
容器分配和逐光打包，不包含可见性筛选、GPU 上传或 Draw。

`granit_lighting_gpu_benchmarks` 复用离屏 PBR 渲染链，以 Vulkan timestamp query 分别测量 Shadow、
PBR HDR、Tone Mapping 和整条 GPU 渲染链。使用 `--lights` 选择点光数量，并通过 `--iterations`、
`--samples`、`--warmup` 控制每个样本的帧数、正式样本数和预热样本数。例如：

```powershell
./build/windows-clang-release/bin/granit_lighting_gpu_benchmarks.exe `
  --lights 64 --iterations 20 --samples 20 --warmup 5
```

输出为 CSV；每个样本先取指定帧数的 GPU 时间均值，再由这些样本计算 mean、P50、P95 和 P99。
同时输出包含录制、提交、等待和 timestamp 查询的 `cpu_end_to_end`。该基准不执行像素回读，
常规 `granit_pbr_offscreen` 示例仍保留完整像素回归。

`granit_render_pipeline_benchmarks` 测量公共 Render Pipeline 自动路径的端到端 CPU 调用成本，
包含 Scene 复制、Graph 构建、资源准备、命令录制、提交和当前实现中的完成等待，不包含初始化、
材质归档构建或像素回读。基准同时运行最小阶段回调路径；它保留相同的 Graph、瞬态附件和 Tone
Mapping，但跳过自动 Shadow/Opaque Draw，用于隔离门面外壳与自动 Draw 资源准备：

```powershell
./build/windows-clang-release/bin/granit_render_pipeline_benchmarks.exe `
  --iterations 20 --samples 20 --warmup 5
```

完整链路 GPU timestamp 目前只能由 H-05 手工基准获取。统一门面内部拥有 Recorder 和提交边界，
在没有受控内部测量钩子时不把队列外部时间差冒充 GPU 忙碌时间。

已提交的基线摘要见 [results/README.md](results/README.md)。
