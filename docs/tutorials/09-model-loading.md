<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 09：加载 glTF 模型

前几章使用源码内置立方体。本章从 `.gltf` 或 `.glb` 读取真实资产，把外部 Buffer、Primitive、节点
变换和 PBR Material 映射到 Granit 的 Mesh、Material、Scene 与 Draw Binding。完成后，程序可以
显示默认教程立方体，也可以从命令行加载其他 glTF 2.0 模型。

本章先使用同步桌面文件加载，使资产映射和资源所有权保持清晰。浏览器 Fetch、异步状态、环境光
和编辑器面板将在下一章加入。

## 1. 读取文档和相对资源

glTF 文档可能把 Buffer 与 Image 放在相邻文件中。Resolver 以主文档目录为根，只处理 Loader 已经
验证和规范化的相对路径：

```cpp
class file_resolver final : public gltf::resource_resolver {
public:
  explicit file_resolver(std::filesystem::path base) : base_(std::move(base)) {}

  bool resolve(std::string_view path, std::vector<std::byte>& bytes) const override {
    return read_file(base_ / std::filesystem::path{path}, bytes);
  }

private:
  std::filesystem::path base_;
};
```

读取主文档后，把 Resolver 交给私有示例 Loader。Loader 输出不含 GPU 句柄的 CPU Scene：

```cpp
std::vector<std::byte> document;
read_file(asset_path, document);

file_resolver resolver{asset_path.parent_path()};
granit::example::gltf::scene cpu_scene;
const auto loaded = granit::example::gltf::load(document, &resolver, cpu_scene);
```

CPU Scene 保存节点、Mesh、Primitive、Material、Image 和 Sampler。解析失败时 `diagnostic` 提供文件或
数据错误；失败结果不会留下半初始化 GPU 资源。

## 2. 从 Primitive 建立 GPU Scene

[`examples/common/model_viewer`](../../examples/common/model_viewer) 是教程和完整 Model Viewer 共用的
仓库私有适配层。它执行以下确定性映射：

```text
glTF Primitive attributes   → 合并 Vertex / Index Buffer
glTF Primitive draw range   → granit::mesh
glTF Image + Sampler        → Texture / Texture View / Sampler
glTF PBR material           → granit::material_instance
glTF Node world transform   → scene_renderable
Renderable payload          → render_pipeline_draw_binding
```

应用只需在 Renderer Ready 后上传一次：

```cpp
granit::example::model_viewer::gpu_scene gpu_scene;
check(gpu_scene.initialize(renderer, cpu_scene));
```

`gpu_scene` 拥有上传后的 Buffer、Texture、Mesh 和 Material。CPU Scene 可以继续用于名称、层级和材质
检查，也可以在不再需要这些信息时释放；GPU 资源不会借用 CPU 数组。

## 3. 根据模型 Bounds 放置相机

上传计划为每个 Node 展开的 Draw 保存世界空间包围球。本章合并这些 Bounds，得到模型中心和半径，
再按半径选择相机距离与 Near/Far Plane。不同尺度和原点的模型因此不需要硬编码相机位置。

每帧只更新 View、Light 和 Snapshot，模型资源保持不变：

```cpp
check(gpu_scene.create_snapshot(
    std::span{&scene_view, 1}, std::span{&light, 1},
    std::span<const granit_scene_point_light>{},
    std::span<const granit_scene_spot_light>{}, snapshot));

granit::render_pipeline_render_desc desc{};
desc.scene = snapshot.ref();
desc.draw_bindings = gpu_scene.draw_bindings();
check(pipeline.render(desc));
```

Render Pipeline 继续负责 Shadow、HDR、Depth、默认 IBL 和 Tone Mapping。glTF Loader 与 GPU Scene
适配仍属于示例层，不进入 Granit 安装 SDK。

## 4. 构建和运行

完整源码位于
[`examples/tutorials/09_model_loading`](../../examples/tutorials/09_model_loading)。默认资产是仓库内
生成的外部 Buffer glTF 立方体：24 个 PBR 顶点、36 个索引和一个 Metallic-Roughness Material。

```powershell
cmake --build --preset windows-clang-debug --target granit_tutorial_09_model_loading
./build/windows-clang-debug/bin/granit_tutorial_09_model_loading.exe
```

传入其他 `.gltf` 或 `.glb`：

```powershell
./build/windows-clang-debug/bin/granit_tutorial_09_model_loading.exe `
  path/to/model.gltf
```

`.gltf` 的外部 `.bin` 和图片必须保持相对目录结构。当前示例 Loader 支持教程与 Model Viewer 使用的
glTF 2.0 PBR 子集；遇到不支持的扩展会明确失败，不静默忽略影响几何或材质的必要数据。

自动 Smoke 使用默认离线资产，验证解析、外部 Buffer、GPU 上传、PBR 绘制和 Swapchain Recreate：

```powershell
ctest --preset windows-clang-debug `
  -R granit.tutorial.09_model_loading --output-on-failure
```

## 5. 生命周期与下一步

关闭时先停止帧循环，再依次释放 Snapshot、Render Pipeline 和 GPU Scene，最后释放 Swapchain、
Renderer 与 Window。GPU Scene 内部按 Material、Mesh、Buffer 和 Texture 的依赖顺序释放资源。

本章刻意不加入 UI 和异步加载。下一章将现有跨后端 Model Viewer 迁入教程，增加轨道相机、完整
PBR 纹理、环境光、节点与材质检查器、桌面异步文件加载和浏览器 Fetch。

[上一章：接入 ImGui](08-imgui.md) · [下一章：完成 Model Viewer](10-model-viewer.md)
