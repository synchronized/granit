<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 09：Particle Generator

本教程使用一个 Storage Buffer 保存粒子状态，顶点 Shader 通过 `SV_InstanceID` 读取实例数据，
共享低面数 Mesh 绘制球体、正方体或长方体，并通过 Alpha 混合绘制。它是 S-98B 的交互式能力
示例，完整源码位于
[`examples/tutorials/09_particles`](../../examples/tutorials/09_particles)。

## 验证目标

- 动态更新 GPU Storage Buffer，不创建 Particle 专用公共 API；
- 使用实例化绘制和共享 Mesh 生成可变数量的三维粒子；Sphere 使用带法线的低面数 UV 网格，
  并通过教程内置方向光显示基本的面间明暗；
- 粒子从固定发射点出发，使用固定种子生成可复现的随机方向和三轴正负角速度；面板中的
  Direction、Acceleration 和 Angular speed 都是三维控制量；
- 普通交互从零粒子开始，支持 Burst 单次发射和 Continuous 持续发射，使用固定大小的粒子池
  回收生命周期结束的粒子；调整容量不会重置仍在运行的粒子；
- 透明粒子每帧按相机距离从后向前排序，教程仍明确不提供 OIT 或逐三角形排序；
- 通过 ImGui 调整粒子数量、形状、颜色、方向、加速度、生命周期、尺寸和淡出；
- 面板默认开启 Gizmo：发射器使用线框球并附带红/绿/蓝 XYZ 轴，方向光使用黄色方形发射面和
  向下箭头；面板单独提供 Opacity（默认 0.4）和 Ambient（默认 0.06），可将 Ambient 调到 0
  检查纯方向光；Normal debug 会将原始法线编码为 RGB 并以不透明方式显示，用于区分网格法线
  与光照方向问题，正常演示时应关闭；
  调试线参与深度测试，关闭后不录制；
- 验证 Alpha 混合与跨后端资源绑定；
- Smoke 模式固定数量和时间，便于桌面与浏览器回归。

普通交互启动时粒子数量为零；点击 Emit burst 或使用 Continuous 模式后才从发射器生成粒子。
Smoke 模式为了回归验证会固定生成一批粒子。点击 Reset 会从同一发射点重新生成方向。Gizmo
中的黄色方形表示方向光发射面，箭头表示光线传播方向；Shader 对所有粒子使用同一组平行光线，
不使用光源位置或距离衰减。粒子积分和颜色布局属于教程本地逻辑。GPU
simulation、粒子排序、OIT、透明阴影和虚拟纹理不属于本教程。

## 构建与验证

```powershell
cmake --preset windows-clang-debug
cmake --build --preset windows-clang-debug --target granit_tutorial_09_particles
ctest --test-dir build/windows-clang-debug -R "^granit\.tutorial\.09_particles$" --output-on-failure
```

浏览器构建使用 `emscripten-debug` preset，页面入口为
`granit_tutorial_09_particles.html`。验收应检查多帧推进、非空 Canvas、Resize 和 WebGPU
Validation 消息。
