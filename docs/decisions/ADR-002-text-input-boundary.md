<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# ADR-002：文字输入与字体后端边界

- 状态：已接受
- 日期：2026-08-15
- 部分取代：[ADR-001](ADR-001-debug-text-boundary.md) 中由 Text component 直接负责整形与
  栅格化的约定

## 背景

文字整形、字体回退、换行与栅格化依赖语言、平台和产品需求。把 HarfBuzz、FreeType 或平台字体
系统固定进 Render Pipeline component，会增加传递依赖，并限制上层引擎和 UI 框架复用已有结果。

## 决策

- Text Draw List 的公共输入是调用方已经完成整形和基线定位的字形实例，不接收 UTF-8 字符串。
- `font_key` 是调用方定义的非零身份，通常组合字体、字号和栅格化参数；`glyph_id` 是字体内编号。
- Granit 负责 Atlas 缓存、字形纹理上传、Canvas 四边形生成和批处理。
- Unicode 解码、字体回退、文字整形、换行、对齐和栅格化由上层或可选适配器负责。
- 首版不引入强制字体第三方库；后续 FreeType/HarfBuzz 适配目标不得成为核心传递依赖。

## 影响

- 游戏引擎、编辑器和 UI 框架可以直接提交自己的排版结果。
- 相同 `font_key + glyph_id` 的位图来源和缓存一致性由 Atlas 上传接口明确约束。
- Granit 基础文字接口不能单独把字符串变成可绘制文字，但依赖和 ABI 边界更稳定。

## 替代方案

- **Text component 内建完整字体栈**：使用简单，但引入固定依赖并复制上层排版能力，因此暂不采用。
- **只公开 Canvas、完全不提供 Text component**：最小化库职责，但每个使用者都要重复 Atlas 和批处理，
  因此拒绝。
