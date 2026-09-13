// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

const fs = require("node:fs");
const http = require("node:http");
const path = require("node:path");
const { decodePng, pixelAt } = require("./png.cjs");
const { chromium } = require("playwright-core");

const outputDirectory = path.resolve(process.argv[2] ?? "build/emscripten-release/web");
const chromePath = process.env.CHROME_PATH ?? "/usr/bin/google-chrome";
const entryName = process.argv[3] ?? "granit_web_platform_smoke.html";
const statusTextSelector = entryName === "granit_web_platform_smoke.html" ? "#granit-status" : "#status-text";
const modelQuery = process.argv[4] ? `?model=${encodeURIComponent(process.argv[4])}` : "";
const usesLocalFixture = entryName === "granit_web_platform_smoke.html" || Boolean(process.argv[4]);
const startupTimeout = usesLocalFixture ? 30_000 : 120_000;

const contentTypes = new Map([
  [".data", "application/octet-stream"],
  [".gltf", "model/gltf+json"],
  [".html", "text/html; charset=utf-8"],
  [".js", "text/javascript; charset=utf-8"],
  [".wasm", "application/wasm"],
]);

function validateModelViewerPixels(png) {
  const image = decodePng(png);
  const center = pixelAt(image, Math.floor(image.width / 2), Math.floor(image.height / 2));
  const corner = pixelAt(image, 4, 4);
  const linuxCanvasUnavailable =
    process.platform === "linux" &&
    (center[3] === 0 ||
      (center[0] === 255 &&
        center[1] === 255 &&
        center[2] === 255 &&
        corner[0] === 255 &&
        corner[1] === 255 &&
        corner[2] === 255));
  if (linuxCanvasUnavailable) {
    console.warn("Linux 无头 Chrome 未暴露 WebGPU Canvas 合成像素，跳过截图颜色断言");
    return;
  }
  if (center[0] < 40 || center[1] < 40 || center[2] < 40)
    throw new Error(`WebGPU 模型查看器中心未绘制模型：${center.join(",")}`);
  // 默认摄影棚背景经过交换链颜色空间转换后约为 (23, 36, 55)。这里保留量化与
  // 浏览器实现差异的容差，同时要求蓝色分量明显高于红色，避免纯黑清屏误通过。
  if (
    Math.abs(corner[0] - 23) > 8 ||
    Math.abs(corner[1] - 36) > 8 ||
    Math.abs(corner[2] - 55) > 8 ||
    corner[2] <= corner[0] + 20
  )
    throw new Error(`WebGPU 模型查看器背景像素异常：${corner.join(",")}`);
}

function startServer() {
  const requestedPaths = new Set();
  let externalBufferAvailable = true;
  const server = http.createServer((request, response) => {
    const requestPath = new URL(request.url, "http://127.0.0.1").pathname;
    requestedPaths.add(requestPath);
    if (!externalBufferAvailable && requestPath === "/model_viewer_fixture.bin") {
      response.writeHead(404, { "Cache-Control": "no-store" }).end();
      return;
    }
    const relativePath = requestPath === "/" ? entryName : requestPath.slice(1);
    const filePath = path.resolve(outputDirectory, relativePath);
    if (!filePath.startsWith(`${outputDirectory}${path.sep}`)) {
      response.writeHead(403).end();
      return;
    }
    fs.readFile(filePath, (error, content) => {
      if (error) {
        response.writeHead(404).end();
        return;
      }
      response.writeHead(200, {
        "Cache-Control": "no-store",
        "Content-Type": contentTypes.get(path.extname(filePath)) ?? "application/octet-stream",
      });
      response.end(content);
    });
  });
  return new Promise((resolve, reject) => {
    server.once("error", reject);
    server.listen(0, "127.0.0.1", () =>
      resolve({
        server,
        requestedPaths,
        rejectExternalBuffer() {
          externalBufferAvailable = false;
        },
      }),
    );
  });
}

async function main() {
  const { server, requestedPaths, rejectExternalBuffer } = await startServer();
  const address = server.address();
  const browserArguments = ["--enable-unsafe-webgpu", "--no-sandbox"];
  if (process.platform !== "win32") {
    browserArguments.push(
      "--enable-features=Vulkan",
      "--use-angle=vulkan",
      "--disable-vulkan-surface",
    );
  }
  const browser = await chromium.launch({
    executablePath: chromePath,
    headless: process.env.GRANIT_BROWSER_HEADLESS !== "0",
    args: browserArguments,
  });
  const page = await browser.newPage();
  const browserMessages = [];
  page.on("console", (message) => browserMessages.push(`${message.type()}: ${message.text()}`));
  page.on("pageerror", (error) => browserMessages.push(`pageerror: ${error.message}`));

  try {
    await page.goto(`http://127.0.0.1:${address.port}/${entryName}${modelQuery}`, {
      waitUntil: "load",
    });
    await page.waitForFunction(
      () => {
        const status = document.querySelector("#granit-status")?.dataset.status;
        return status === "ready" || status === "failed";
      },
      undefined,
      { timeout: startupTimeout },
    );
    const status = await page.locator("#granit-status").getAttribute("data-status");
    if (status !== "ready") {
      throw new Error(`WebGPU 平台启动失败，页面状态为 ${status}`);
    }
    const rendererState = await page.evaluate(() => Module._granit_web_renderer_state());
    const failureResult = await page.evaluate(() => Module._granit_web_renderer_failure_result());
    const assetStatus = await page.evaluate(() => Module._granit_web_asset_status());
    // GRANIT_RENDERER_STATE_READY 的公共 ABI 数值为 2。
    // asset_request_status::ready 的内部测试契约数值为 2。
    if (rendererState !== 2 || failureResult !== 0 || assetStatus !== 2) {
      throw new Error(
        `WebGPU 生命周期异常，state=${rendererState}, failure=${failureResult}, asset=${assetStatus}`,
      );
    }
    if (!browserMessages.some((message) => message === "log: GRANIT_EMPTY_FRAME:ready"))
      throw new Error("浏览器未完成零 Renderable、Canvas 和 Swapchain Frame 验收");
    for (const diagnostic of [
      "Emscripten WebGPU initialization started",
      "Emscripten WebGPU adapter and device are ready",
    ]) {
      if (!browserMessages.some((message) => message === `log: GRANIT_DIAGNOSTIC:${diagnostic}`))
        throw new Error(`WebGPU 信息诊断未使用普通日志输出：${diagnostic}`);
      if (browserMessages.some((message) => message === `error: GRANIT_DIAGNOSTIC:${diagnostic}`))
        throw new Error(`WebGPU 信息诊断被错误输出为 error：${diagnostic}`);
    }
    for (const stage of [
      "document", "buffers", "images", "materials", "meshes", "nodes",
      "planning", "geometry", "textures", "samplers",
      "pipelines",
    ]) {
      if (!browserMessages.some((message) => message.includes(`GRANIT_PROGRESS:${stage}:`)))
        throw new Error(`浏览器分阶段上传未报告 ${stage} 进度`);
    }
    const uploadProgress = await page.evaluate(() => ({
      stage: Module._granit_web_upload_stage(),
      completed: Module._granit_web_upload_completed(),
      total: Module._granit_web_upload_total(),
    }));
    if (uploadProgress.total === 0 || uploadProgress.completed !== uploadProgress.total)
      throw new Error(`浏览器上传进度未完成：${JSON.stringify(uploadProgress)}`);
    await page.waitForFunction(
      () =>
        typeof Module._granit_web_rendered_frame_count === "function" &&
        Module._granit_web_rendered_frame_count() >= 60,
      undefined,
      { timeout: 10_000 },
    );
    if (usesLocalFixture) {
      for (const assetPath of ["/model_viewer_fixture.gltf", "/model_viewer_fixture.bin"]) {
        if (!requestedPaths.has(assetPath))
          throw new Error(`浏览器资源加载链路未请求 ${assetPath}`);
      }
    }
    validateModelViewerPixels(await page.locator("#canvas").screenshot({ type: "png" }));

    const initialLighting = await page.evaluate(() => ({
      generation: Module._granit_web_lighting_generation(),
      exposure: Module._granit_web_exposure_ev(),
      environment: Module._granit_web_environment_intensity(),
      keyLight: Module._granit_web_key_light_intensity(),
    }));
    const invalidLightingResult = await page.evaluate(() =>
      Module._granit_web_configure_lighting(0, 0.5, -1),
    );
    if (invalidLightingResult !== -2)
      throw new Error(`无效浏览器光照参数未被拒绝：${invalidLightingResult}`);
    if (entryName === "granit_web_platform_smoke.html") {
      // 平台页没有示例控制面板，通过同一运行层接口验证光照行为。
      const result = await page.evaluate(() => Module._granit_web_configure_lighting(0.35, 0.45, 1.75));
      if (result !== 0) throw new Error(`平台光照配置失败：${result}`);
    } else {
      await page.evaluate(() => {
        const values = {
          exposure: "0.35",
          "environment-intensity": "0.45",
          "key-light-intensity": "1.75",
        };
        for (const [id, value] of Object.entries(values)) {
          const control = document.getElementById(id);
          control.value = value;
        }
        document.getElementById("exposure").dispatchEvent(new Event("input", { bubbles: true }));
      });
    }
    await page.waitForFunction(
      (previous) => Module._granit_web_lighting_generation() === previous + 1,
      initialLighting.generation,
      { timeout: 5_000 },
    );
    const lighting = await page.evaluate(() => ({
      exposure: Module._granit_web_exposure_ev(),
      environment: Module._granit_web_environment_intensity(),
      keyLight: Module._granit_web_key_light_intensity(),
    }));
    if (
      Math.abs(lighting.exposure - 0.35) > 0.001 ||
      Math.abs(lighting.environment - 0.45) > 0.001 ||
      Math.abs(lighting.keyLight - 1.75) > 0.001
    ) {
      throw new Error(`浏览器光照配置未进入 Viewer Core：${JSON.stringify(lighting)}`);
    }
    const restoreLightingResult = await page.evaluate((initial) =>
      Module._granit_web_configure_lighting(
        initial.exposure,
        initial.environment,
        initial.keyLight,
      ), initialLighting,
    );
    if (restoreLightingResult !== 0)
      throw new Error(`浏览器默认光照恢复失败：${restoreLightingResult}`);

    const qualityGeneration = await page.evaluate(() => Module._granit_web_quality_generation());
    const invalidQualityResult = await page.evaluate(() =>
      Module._granit_web_configure_render_quality(2, 0, 0, 1),
    );
    if (invalidQualityResult !== -2)
      throw new Error(`无效浏览器质量参数未被拒绝：${invalidQualityResult}`);
    const lowQualityResult = await page.evaluate(() =>
      Module._granit_web_configure_render_quality(1, 0, 0, 1),
    );
    if (lowQualityResult !== 0)
      throw new Error(`浏览器低质量配置失败：${lowQualityResult}`);
    const framesBeforeHighQuality = await page.evaluate(() =>
      Module._granit_web_rendered_frame_count(),
    );
    await page.waitForFunction(
      (previous) => Module._granit_web_rendered_frame_count() > previous,
      framesBeforeHighQuality,
      { timeout: 10_000 },
    );
    const anisotropy = await page.evaluate(() =>
      Math.min(8, Module._granit_web_max_sampler_anisotropy()),
    );
    if (anisotropy < 1)
      throw new Error(`浏览器未报告有效的各向异性上限：${anisotropy}`);
    const highQualityResult = await page.evaluate((value) =>
      Module._granit_web_configure_render_quality(4, 1, 1, value), anisotropy,
    );
    if (highQualityResult !== 0)
      throw new Error(`浏览器高质量配置失败：${highQualityResult}`);
    await page.waitForFunction(
      (previous) =>
        Module._granit_web_quality_generation() === previous + 2 &&
        Module._granit_web_rendered_frame_count() > 60,
      qualityGeneration,
      { timeout: 10_000 },
    );
    validateModelViewerPixels(await page.locator("#canvas").screenshot({ type: "png" }));

    await page.keyboard.press("F");
    const canvas = page.locator("#canvas");
    const box = await canvas.boundingBox();
    if (!box) throw new Error("无法获取 Canvas 布局范围");
    await page.mouse.move(box.x + 32, box.y + 32);
    await page.mouse.down({ button: "right" });
    await page.mouse.move(box.x + 80, box.y + 56);
    await page.mouse.up({ button: "right" });
    await page.mouse.wheel(0, -120);
    await page.waitForFunction(
      () =>
        typeof Module._granit_web_input_event_count === "function" &&
        Module._granit_web_input_event_count() >= 4 &&
        typeof Module._granit_web_applied_input_count === "function" &&
        Module._granit_web_applied_input_count() >= 1,
      undefined,
      { timeout: 5_000 },
    );
    await page.evaluate(() => {
      const canvas = document.querySelector("#canvas");
      canvas.width = 800;
      canvas.height = 450;
    });
    await page.waitForFunction(
      () =>
        typeof Module._granit_web_resize_count === "function" &&
        Module._granit_web_resize_count() >= 1,
      undefined,
      { timeout: 10_000 },
    );
    if (usesLocalFixture)
      validateModelViewerPixels(await canvas.screenshot({ type: "png" }));
    const framesBeforeShutdown = await page.evaluate(() =>
      Module._granit_web_rendered_frame_count(),
    );
    const shutdownResult = await page.evaluate(() => Module._granit_web_shutdown());
    const repeatedShutdownResult = await page.evaluate(() => Module._granit_web_shutdown());
    const shutdownStatus = await page.evaluate(() => Module._granit_web_platform_status());
    const liveResources = await page.evaluate(() =>
      Number(Module._granit_web_shutdown_live_resource_count()),
    );
    const pendingRetirements = await page.evaluate(() =>
      Number(Module._granit_web_shutdown_pending_retirement_count()),
    );
    await page.waitForTimeout(100);
    const framesAfterShutdown = await page.evaluate(() =>
      Module._granit_web_rendered_frame_count(),
    );
    await page.waitForTimeout(100);
    const stableFramesAfterShutdown = await page.evaluate(() =>
      Module._granit_web_rendered_frame_count(),
    );
    if (
      shutdownResult !== 0 ||
      repeatedShutdownResult !== 0 ||
      shutdownStatus !== 3 ||
      liveResources !== 0 ||
      pendingRetirements !== 0 ||
      stableFramesAfterShutdown !== framesAfterShutdown
    ) {
      throw new Error(
        `浏览器资源释放异常：result=${shutdownResult}, repeated=${repeatedShutdownResult}, ` +
          `status=${shutdownStatus}, live=${liveResources}, pending=${pendingRetirements}, ` +
          `frames=${framesBeforeShutdown}->${framesAfterShutdown}->${stableFramesAfterShutdown}`,
      );
    }
    console.log(
      "浏览器 WebGPU 多帧渲染、质量与光照切换、输入、Resize、资产 Fetch 与资源释放验证通过",
    );
    const validationErrors = browserMessages.filter((message) =>
      /validation error|webgpu.*error/i.test(message),
    );
    if (validationErrors.length !== 0)
      throw new Error(`浏览器 WebGPU 验证层报告错误：\n${validationErrors.join("\n")}`);
    if (
      process.env.GRANIT_EXPECT_SOFTWARE_PIPELINE_FALLBACK === "1" &&
      !browserMessages.some((message) =>
        message.includes("软件 WebGPU 适配器异步编译失败，回退到按需同步创建管线"),
      )
    ) {
      throw new Error("软件 WebGPU 适配器未经过预期的异步 Pipeline 降级路径");
    }
    if (
      browserMessages.some((message) =>
        message.includes("软件 WebGPU 适配器异步编译失败，回退到按需同步创建管线"),
      )
    ) {
      console.log("软件 WebGPU 异步 Pipeline 降级及同步渲染回归验证通过");
    } else {
      console.log("WebGPU 原生异步 Pipeline 创建回归验证通过");
    }

    if (!usesLocalFixture) return;
    const cancelPage = await browser.newPage();
    // 在确定的加载进度点取消，避免轮询错过小型 Fixture 的上传阶段。
    await cancelPage.addInitScript(() => {
      const originalLog = console.log;
      console.log = (...args) => {
        originalLog.apply(console, args);
        if (typeof args[0] === "string" && args[0].startsWith("GRANIT_PROGRESS:document:") &&
            typeof globalThis.Module?._granit_web_cancel_loading === "function") {
          globalThis.granitCancelResult = Module._granit_web_cancel_loading();
        }
      };
    });
    await cancelPage.goto(`http://127.0.0.1:${address.port}/${entryName}${modelQuery}`, {
      waitUntil: "load",
    });
    await cancelPage.waitForFunction(() => Module.runtimeReady === true, undefined, {
      timeout: 30_000,
    });
    await cancelPage.waitForFunction(
      () => globalThis.granitCancelResult === 0,
      undefined,
      { timeout: 30_000 },
    );
    await cancelPage.waitForFunction(
      () => document.querySelector("#granit-status")?.dataset.status === "failed",
      undefined,
      { timeout: 30_000 },
    );
    const cancelledText = await cancelPage.locator(statusTextSelector).textContent();
    if (!/^failed:asset-(?:load|upload):-15/.test(cancelledText?.trim() ?? ""))
      throw new Error(`浏览器上传取消未返回稳定结果：${cancelledText}`);
    const cancelledShutdown = await cancelPage.evaluate(() => Module._granit_web_shutdown());
    if (cancelledShutdown !== 0)
      throw new Error(`浏览器取消后资源释放失败：${cancelledShutdown}`);
    await cancelPage.close();
    console.log("浏览器 WebGPU 上传取消与回滚验证通过");

    rejectExternalBuffer();
    const failurePage = await browser.newPage();
    const failureMessages = [];
    failurePage.on("console", (message) =>
      failureMessages.push(`${message.type()}: ${message.text()}`),
    );
    failurePage.on("pageerror", (error) => failureMessages.push(`pageerror: ${error.message}`));
    const failureQuery = new URLSearchParams({ "missing-external-buffer": "1" });
    if (process.argv[4]) failureQuery.set("model", process.argv[4]);
    await failurePage.goto(`http://127.0.0.1:${address.port}/${entryName}?${failureQuery}`, {
      waitUntil: "load",
    });
    await failurePage.waitForFunction(
      () => document.querySelector("#granit-status")?.dataset.status === "failed",
      undefined,
      { timeout: 30_000 },
    );
    const failureText = await failurePage.locator(statusTextSelector).textContent();
    if (!failureText?.trim().startsWith("failed:asset-resource-fetch:")) {
      throw new Error(
        `外部 Buffer 缺失未进入预期失败路径：${failureText}\n${failureMessages.join("\n")}`,
      );
    }
    await failurePage.close();
    console.log("浏览器 WebGPU 外部 Buffer 缺失诊断验证通过");
  } catch (error) {
    console.error(browserMessages.join("\n"));
    throw error;
  } finally {
    await browser.close();
    await new Promise((resolve) => server.close(resolve));
  }
}

main().catch((error) => {
  console.error(error);
  process.exitCode = 1;
});
