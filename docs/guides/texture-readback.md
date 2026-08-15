<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 纹理同步回读

## 适用场景

同步回读面向截图、自动化测试、编辑器工具和低频诊断。调用会提交内部复制并等待 GPU 完成，
不适合连续帧视频采集或高频运行时路径；这些场景应使用显式 Command Recorder 和 Readback
Buffer 组合多个在途操作。

## 基本流程

1. 创建带 `transfer_source` 用途的单采样颜色纹理。
2. 填写 `texture_write_region`，先调用 `query_readback` 查询布局与所需容量。
3. 按 `required_size` 分配内存，再调用 `read`。
4. 按 `bytes_per_row` 定位每一行，不能假定行数据紧密排列。

返回数据保持纹理自身格式和坐标方向，不进行 Y 翻转、颜色空间转换或通道重排。首版不支持
深度/模板和压缩格式。

## 原始文件示例

不传参数时，示例只验证回读像素，不产生文件：

```powershell
build/windows-clang-debug/bin/granit_texture_readback_example.exe
```

传入路径后写出原始像素：

```powershell
build/windows-clang-debug/bin/granit_texture_readback_example.exe clear.rgba
```

示例文件是 16×16 的 `RGBA8_UNORM` 数据，每行跨度由程序输出。`.rgba` 没有尺寸、格式等元数据，
不能直接当作 PNG 使用；图片编码和资源包由工具或上层资产模块负责，不属于 Renderer。

## C API 容量查询

调用 `granit_texture_read` 时将数据指针设为空、容量设为零，即可获得
`granit_texture_readback_info` 和所需字节数。容量不足时接口返回所需大小且不写入部分数据。
