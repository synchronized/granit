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

`granit_canvas_benchmarks` 用于 H-06C，固定测量 100、1,000、10,000 个矩形的 Canvas Draw List
构建、全兼容
合批扫描、逐项交替状态扫描和复用 Buffer 的动态几何上传。输出后的计数行同时给出 Item、Batch
和预期 Draw 数；该程序当前只提供 CPU 墙钟基线，不代表 Canvas Pass 的 GPU 时间。

`granit_canvas_gpu_benchmarks` 使用 Vulkan timestamp 测量实际 Canvas Pass。相邻兼容路径合为一个 Draw；
交替路径逐项切换 Texture 与 Scissor，用于建立有意保留透明顺序时的最坏情况。目标固定为 64×64，
时间不包含 CPU 录制、提交和等待，不应当外推为其他 GPU 或分辨率的绝对预算。H-09B 额外覆盖
0、2、8、32 个完全重叠的透明层：0 层为纯清屏基线，兼容路径合为一个 Draw，交替路径保持
每层一个 Draw。CSV 同时输出实际 Item、Batch/Draw 和覆盖层数；Canvas 不执行全局透明排序，
调用顺序就是稳定合成顺序，CPU 构建与合批扫描成本由 `granit_canvas_benchmarks` 单独测量。

启用 `GRANIT_BUILD_INTEGRATION_IMGUI` 时，`granit_imgui_benchmarks` 测量 ImGui Draw Data 转换并
追加到公共 Canvas 的 CPU 成本，固定覆盖 10、100 和 1,000 个 Draw Command。字体 Atlas 上传不在
该目标内；它由应用负责，底层动态几何上传与 Canvas 合批分别由上述 Canvas 基准覆盖。

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
材质归档构建或像素回读。固定种子负载通过 `--draws`、`--materials`、`--texture-groups` 和
`--lights` 控制可见 Draw、唯一材质、纹理资源组和点光数量；当前分别支持最高 10,000、512、
512 和 128。纹理组使用独立 1×1 纹理并进入真实材质 Bind Group，但当前 Shader 不采样纹理，
因此该维度只测量资源绑定与切换成本。基准分别在方向光加点光与无灯光场景运行自动路径和最小
阶段回调路径。
最小回调保留相同的 Graph、瞬态附件和 Tone Mapping，但跳过自动 Shadow/Opaque Draw；四组
结果用于拆分 Opaque Draw、Shadow 图外壳及方向光/真实 Shadow Draw 的增量：

```powershell
./build/windows-clang-release/bin/granit_render_pipeline_benchmarks.exe `
  --draws 1000 --materials 64 --texture-groups 64 --lights 64 `
  --iterations 20 --samples 20 --warmup 5
```

H-09C 使用 `--shadow-range near|medium|far` 选择总跨度 10、40、160 的方向光阴影正交体，阴影图
均保持 1024×1024。CSV 输出覆盖跨度、世界单位/texel 和实际投影物数量，用于在相同 Draw 负载
下比较覆盖质量与 `gpu_shadow` 成本；该选项只通过仓库内部测量接口生效，不属于公共 API。

H-09D 固定其他负载后，分别使用 `--lights 1`、`16`、`64`、`128` 采样 `gpu_opaque` 和
`automatic_directional_end_to_end`，用于区分 Forward Shader 逐光成本与完整调用成本。首份曲线及
设备限定结论见 [H-09D 多光源曲线](results/2026-08-19-windows-clang-multi-light-7e14162.md)。

基准通过不安装的内部测量接口，在统一门面自己的 Recorder 内写入 Vulkan timestamp，输出
`gpu_shadow`、`gpu_opaque` 和 `gpu_tone_mapping`。CSV 每行同时记录实际 Draw、材质、纹理组、
材质切换、纹理组切换和点光数量。该接口仅用于仓库基准，不属于公共 API 或 ABI。

H-09E 的 CSV schema 5 额外输出材质 Bind Group 数、Render Pipeline 逐 Draw 缓存条目和 Opaque
批次数，并用 `material_create_with_bind_group` 与 `material_bind_group_resource_update` 分行记录准备
路径。创建指标包含归档解码、Layout、Pipeline Template 和 Bind Group，是 Bind Group 创建成本的
保守上界；资源更新会事务式迁移实例并重建 Bind Group。首份压力曲线见
[H-09E 绑定压力结果](results/2026-08-20-windows-clang-binding-pressure-7b3e246.md)。

已提交的基线摘要见 [results/README.md](results/README.md)。
H-09 的四项独立结论及重新评估条件见
[高级渲染能力评估记录](../docs/records/H-09-advanced-rendering-evaluation.md)。
