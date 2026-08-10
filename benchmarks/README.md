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

已提交的基线摘要见 [results/README.md](results/README.md)。
