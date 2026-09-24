<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# S-58：统一 Example Asset System

## 状态

**实施中，P1。** 当前 `asset_store` 将逻辑路径、打包目录和同步读取绑定在一起，`asset_loader`
将外部位置与异步调度绑定在一起；二者在路径解析和完整 Blob 读取上职责交叉。S-58 将资产身份、
来源和读取调度拆开，以单一请求模型服务 Application Host、教程、glTF 与 Model Viewer。

## 目标

- 业务代码只使用“资产挂载 + 规范逻辑路径”，不判断文件系统、URL、MEMFS 或 Fetch；
- 所有读取返回同一种请求，统一 ready、pending、failed、cancelled、进度和诊断；
- Desktop 目录、Emscripten 预加载文件系统和 Web Fetch 作为内部 Source/Mount 实现；
- `document_loader` 使用注入的统一资产系统，外部 Buffer 与纹理继续通过相对地址解析；
- Application Host 只持有一个资产入口；Tutorial 与 Model Viewer 不再并列选择 Store 或 Loader；
- 迁移完成后删除 `asset_store`、`asset_store_resolver` 和独立 `asset_loader` 入口。

## 非目标

- 不把本任务直接提升为 Granit 安装 SDK 或公共 ABI；先在 `examples/common` 验证。
- 不实现通用 VFS、写入、目录枚举、随机访问、文件监听或全局缓存数据库。
- 不让业务代码接触 `std::filesystem`、Emscripten Fetch、HTTP 状态或平台原生句柄。
- 不改变 glTF Importer 的同步 `resource_resolver`；Importer 启动前资源仍应完整驻留内存。
- 不用统一资产系统接管 Renderer、GPU 上传线程或 Window 主循环。

## 设计

三个正交维度分别表达：

```text
资产身份：mount + logical path
资产来源：packaged directory / MEMFS / filesystem / HTTP
读取调度：立即完成 / worker / Fetch callback
```

业务入口只保存稳定逻辑身份：

```cpp
struct asset_key {
  asset_mount mount;
  std::string_view path;
};

auto request = assets.request({bundled_assets, "tutorials/01_cube/wooden_crate.png"});
auto model = assets.request({model_content, "scene.gltf"});
```

`asset_mount` 是 `asset_system` 创建的类型安全标识，不包含路径、指针或平台句柄。平台组合入口建立
挂载：Desktop 把 bundled 映射到 `<exe>/assets`，Web 映射到 `/assets`；用户选择的本地目录或基础
URL 映射到 content。挂载和 Source 类型不传入 Viewer Core、Tutorial 业务或 glTF Importer。

`request()` 采用统一异步语义：Source 可以立即把请求置为 ready，也可以稍后由 worker 或 Fetch
完成；调用方只观察请求状态。`poll()` 在应用线程发布后台完成结果，后台实现不直接调用业务代码。

```text
application / tutorial / document_loader
                  │ asset_key
                  ▼
             asset_system
       request / poll / cancel / result
                  │
          internal mount table
          ├─ packaged source
          ├─ filesystem source
          └─ fetch source
```

相对资源解析在逻辑地址层完成，保留原挂载并规范化路径；不得通过 `..` 逃离挂载。HTTP Query、
Fragment、百分号和平台路径只由对应 Source 的根位置处理，不进入逻辑路径。

## 目标结构

```text
examples/common/assets/
├─ asset_system.h/.cpp
├─ asset_request.h/.cpp
├─ asset_batch.h/.cpp
├─ asset_key.h/.cpp
├─ asset_location.h/.cpp       # 平台组合入口使用的根位置
├─ asset_source_desktop.cpp
├─ asset_source_web.cpp
├─ memory_resource_resolver.h/.cpp
└─ resource_resolver.h
```

实施中允许先由 `asset_system` 复用现有 Store/Loader 后端；最终目录以删除重复入口后的职责为准。

## 实施顺序

1. **S-58A 地址与路径契约（已完成）**：外部地址解析已从 `asset_loader.h` 移入
   `asset_location.*`，打包资产和 glTF 资源复用 `normalize_resource_path()`；URL、文件路径、NUL、
   父目录逃逸、相对资源和失败时输出不变测试已经通过。
2. **S-58B Asset System 核心**：增加类型安全 Mount、`asset_key` 和统一请求入口；请求继续复用现有
   `asset_request` 状态、进度、取消与 generation 保护。
3. **S-58C 内部 Source**：接入 Desktop packaged/filesystem 与 Web MEMFS/Fetch；平台 Source 只在
   构建期选择，不进入业务头文件。
4. **S-58D Application 与教程迁移**：Application Host 改为单一 Asset System；Cube、PBR Assets、
   Shader Library、Material、Texture 和 Environment 使用逻辑 Key 请求。
5. **S-58E glTF 与 Model Viewer**：`document_loader` 接收 Asset System 与 Mount，统一文档和外部资源
   请求；增加共享 `model_loading_session`，统一“文档 → Import → GPU Plan”的状态、进度和取消，
   Desktop/Web 仅保留任务调度、UI 与 GPU 所属线程差异。
6. **S-58F 删除与验证**：删除 Store/Loader 并列入口和重复 Resolver，更新指南与实施记录；验证
   Windows shared/static、Emscripten、Chrome、取消、失败回滚、路径逃逸和资源释放。

## 验收标准

- Tutorial、Model Viewer 和 Application Host 业务代码不判断 Desktop/Web 资产来源；
- 同一逻辑 Key 在 Desktop packaged 目录和 Web `/assets` 返回相同字节；
- 本地文件、Web URL 和 glTF 相对资源使用同一请求状态、进度、取消与错误分类；
- 逻辑路径不能逃离 Mount，失败不会修改已提交输出或泄漏后台请求；
- `asset_store`、`asset_store_resolver` 和旧 `asset_loader` 不再作为并列业务入口存在；
- Model Viewer Desktop/Web 的加载阶段、取消和诊断一致，平台线程与 Asyncify 约束保持不变；
- 示例私有资产系统不进入安装导出，Granit 公共 API/ABI 不变化；
- 相关单元测试、Desktop Smoke、Emscripten 和 Chrome 验收通过。
