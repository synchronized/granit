<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 从 0.23 迁移到 0.24

0.24.0 收敛 AssetTools 的公共头目录和 C++ 命名空间，并增强 AssetTools 结果句柄的类型校验。
Core、RenderPipeline、Window 与 Input 的公共使用方式不因本版本改变。

## 更新 AssetTools 头文件

将旧公共路径：

```cpp
#include <granit/tools/shader.hpp>
```

替换为对应的 AssetTools 路径：

```cpp
#include <granit/asset_tools/shader.hpp>
```

Shader、Material、Texture 与 Environment 的其他 AssetTools 头使用相同目录规则。安装包不再提供
`<granit/tools/...>` 转发头。

## 更新 C++ 命名空间

将 `granit::tools` 替换为 `granit::asset_tools`。C ABI 函数名、`AssetTools` CMake component、
`granit::asset_tools` 导入目标以及已发布资产格式保持不变。

AssetTools 的 Compilation、Reflection、Library Builder、Material Builder、Texture Builder 和
Environment Builder 句柄现在校验资源类型与 generation。应用不应在不同 API 之间转换或复用
这些整数句柄；销毁后继续使用会返回 `GRANIT_ERROR_INVALID_HANDLE`。

完成源码替换后重新配置并完整编译 Consumer，避免旧 CMake 缓存继续引用已经删除的安装头。
