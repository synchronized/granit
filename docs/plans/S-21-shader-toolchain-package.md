<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# S-21：可复现 Shader Toolchain 包

## 状态

**已完成。** Windows/Linux 预发行工具包、双层完整性校验、锁定下载和官方 CI 消费闭环均已
通过远端验证。

## 背景与目标

ShaderTools 可使用系统 DXC、glslangValidator 和 Tint，但官方 CI 与 Release 需要一份可重复下载、
可校验且不依赖开发机安装状态的工具链。目标是按宿主平台发布版本化归档，并以 SHA-256 锁定消费。

## 非目标

- 不把编译器加入 Granit 核心 SDK、运行时动态库或传递依赖。
- 不在应用启动时下载或编译 Shader。
- 不承诺未经能力探测的新编译器版本与锁定版本等价。

## 已确认决策

- Windows 与 Linux 分别发布宿主工具包，包含 DXC、glslangValidator、Tint 及必要运行库和许可证。
- 包清单记录上游版本、源码修订、文件 SHA-256、许可证和构建参数。
- 官方 CI 使用 `GRANIT_SHADER_TOOLCHAIN_POLICY=locked`；普通用户仍默认使用 `compatible`。
- CMake 下载必须提供归档 SHA-256；在真实 Release 产物生成前不预写摘要。
- 工具升级产生新包版本，不覆盖旧归档；Shader 缓存继续使用实际工具二进制身份。

## 实施顺序

1. 已建立跨平台清单生成与验证脚本，记录完整文件集合、角色、大小和 SHA-256，并拒绝篡改及
   未登记文件。
2. 已建立原子组包脚本，强制分别提供 DXC、glslang、Dawn/Tint 许可证材料，并显式收集运行库。
3. 已建立确定性的源码许可证汇总器，并将锁定 DXC、glslang 与 Dawn/Tint 的许可证、第三方声明
   和必要运行库纳入归档。
4. 已建立并完成远端验证的 Windows/Linux Tint 构建、包内验证、ShaderTools 双前端能力测试与
   归档工作流；最终 Tint 与许可证产物按宿主平台和源码修订缓存，缓存命中时不再获取或编译 Dawn。
5. 已发布独立预发行工具包，并从公开 Release 地址重新下载及校验 Windows/Linux 归档摘要。
6. 已增加锁定下载辅助脚本，按宿主选择归档、验证发布摘要和包内清单，并输出统一工具链根目录
   与 Tint 修订号。
7. 已为官方 Windows/Linux CI 增加独立的 ShaderTools 严格锁定任务，避免普通构建矩阵重复下载
   工具包。

## 测试与验收

- 两个平台从空缓存下载后均能完成 WGSL、HLSL、GLSL 双后端资产测试。
- 归档或单个二进制被修改时，下载校验或工具身份校验必须失败。
- 工具包不出现在 `granit::granit` 安装导出及普通 Consumer 依赖图中。
- 同一源码和锁定工具包重复构建得到相同缓存键及逐字节一致资产。

## 最终约束

- 工具升级时必须重新核对 DXC、glslang 与 Dawn/Tint 的许可证、第三方声明和运行库闭包。
- Tint 无稳定版本查询接口，官方包以 Dawn 修订、清单和文件摘要共同标识。
- Dawn/Tint 只在工具身份变化或缓存缺失时构建；普通 Granit CI 消费已发布工具包。
