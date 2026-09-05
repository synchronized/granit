<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# Environment Map

`granit_environment_map` 是 RenderPipeline component 提供的环境光资源。它拥有 Irradiance Cube、
Prefiltered Environment Cube、BRDF LUT 及对应 Texture View，并通过
`granit_environment_map_get_info` 向逐帧渲染描述提供只读借用视图。

## 创建与所有权

- `granit_environment_map_create_from_asset` 在调用期间借用一段 GRENV v3 字节；返回后不保存输入指针。
- `granit_environment_map_create_builtin` 创建确定性的低分辨率中性环境，用于缺少外部资产时降级。
- Environment Map 属于创建它的 Renderer，不得跨 Renderer 查询或销毁。
- 调用方必须先销毁 Environment Map，再销毁 Renderer；销毁会释放全部子纹理和视图并使旧句柄失效。
- C++20 的 `granit::environment_map` 是 move-only RAII 包装，不建立第二套运行时状态。

## GRENV v3

正式环境资产使用 `.grenv` 扩展名。v3 固定保存 RGBA16F Irradiance Cube、带完整 Mip 链的
Prefiltered Cube、RGBA16F BRDF LUT、推荐环境强度和曝光。96 字节文件头记录布局和 payload 大小，
其中 32 字节 SHA-256 用于在创建 GPU 资源前检测损坏或不完整内容。

运行时只解析和上传已经预处理的纹理，不执行耗时的环境卷积。文件读取、异步调度、缓存和资产
数据库仍由调用方负责。安装 RenderPipeline component 后，示例环境位于
`${granit_RENDER_PIPELINE_ASSET_DIR}/environments/studio_small_03.grenv`。

## 逐帧控制

`granit_environment_map_info.environment` 是可复制的借用描述。调用方可以在每帧副本上调整
`intensity` 和 `rotation_radians`，但不得销毁或替换其中的 Texture View。推荐曝光通过
`recommended_exposure_ev` 独立返回，是否采用由应用决定。
