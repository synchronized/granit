// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

const fs = require("node:fs");
const http = require("node:http");
const path = require("node:path");
const zlib = require("node:zlib");
const { chromium } = require("playwright-core");

const outputDirectory = path.resolve(process.argv[2] ?? "build/emscripten-release/web");
const chromePath = process.env.CHROME_PATH ?? "/usr/bin/google-chrome";
const entryName = "granit_webgpu_fixture_example.html";

const contentTypes = new Map([
  [".data", "application/octet-stream"],
  [".gltf", "model/gltf+json"],
  [".html", "text/html; charset=utf-8"],
  [".js", "text/javascript; charset=utf-8"],
  [".wasm", "application/wasm"],
]);

function paeth(left, above, upperLeft) {
  const estimate = left + above - upperLeft;
  const leftDistance = Math.abs(estimate - left);
  const aboveDistance = Math.abs(estimate - above);
  const upperLeftDistance = Math.abs(estimate - upperLeft);
  if (leftDistance <= aboveDistance && leftDistance <= upperLeftDistance) return left;
  return aboveDistance <= upperLeftDistance ? above : upperLeft;
}

function decodePng(png) {
  const signature = Buffer.from([137, 80, 78, 71, 13, 10, 26, 10]);
  if (!png.subarray(0, 8).equals(signature)) throw new Error("截图不是有效 PNG");
  let offset = 8;
  let width = 0;
  let height = 0;
  let colorType = 0;
  const dataChunks = [];
  while (offset < png.length) {
    const length = png.readUInt32BE(offset);
    const type = png.toString("ascii", offset + 4, offset + 8);
    const data = png.subarray(offset + 8, offset + 8 + length);
    offset += 12 + length;
    if (type === "IHDR") {
      width = data.readUInt32BE(0);
      height = data.readUInt32BE(4);
      if (data[8] !== 8 || (data[9] !== 2 && data[9] !== 6) || data[12] !== 0)
        throw new Error("截图 PNG 使用了测试尚未支持的像素格式");
      colorType = data[9];
    } else if (type === "IDAT") {
      dataChunks.push(data);
    } else if (type === "IEND") {
      break;
    }
  }
  const channels = colorType === 6 ? 4 : 3;
  const stride = width * channels;
  const filtered = zlib.inflateSync(Buffer.concat(dataChunks));
  const pixels = Buffer.alloc(stride * height);
  for (let y = 0; y < height; ++y) {
    const sourceRow = y * (stride + 1);
    const targetRow = y * stride;
    const filter = filtered[sourceRow];
    for (let x = 0; x < stride; ++x) {
      const raw = filtered[sourceRow + 1 + x];
      const left = x >= channels ? pixels[targetRow + x - channels] : 0;
      const above = y > 0 ? pixels[targetRow - stride + x] : 0;
      const upperLeft = y > 0 && x >= channels ? pixels[targetRow - stride + x - channels] : 0;
      const predictor =
        filter === 0
          ? 0
          : filter === 1
            ? left
            : filter === 2
              ? above
              : filter === 3
                ? Math.floor((left + above) / 2)
                : filter === 4
                  ? paeth(left, above, upperLeft)
                  : null;
      if (predictor === null) throw new Error(`截图 PNG 使用了未知过滤器 ${filter}`);
      pixels[targetRow + x] = (raw + predictor) & 0xff;
    }
  }
  return { width, height, channels, pixels };
}

function pixelAt(image, x, y) {
  const offset = (y * image.width + x) * image.channels;
  return Array.from(image.pixels.subarray(offset, offset + image.channels));
}

function validateModelViewerPixels(png) {
  const image = decodePng(png);
  const center = pixelAt(image, Math.floor(image.width / 2), Math.floor(image.height / 2));
  const corner = pixelAt(image, 4, 4);
  if (process.platform === "linux" && center[3] === 0) {
    console.warn("Linux 无头 Chrome 未暴露 WebGPU Canvas 合成像素，跳过截图颜色断言");
    return;
  }
  if (center[0] < 40 || center[1] < 40 || center[2] < 40)
    throw new Error(`WebGPU 模型查看器中心未绘制模型：${center.join(",")}`);
  // 默认摄影棚背景经过交换链颜色空间转换后约为 (33, 49, 73)。这里保留量化与
  // 浏览器实现差异的容差，同时要求蓝色分量明显高于红色，避免纯黑清屏误通过。
  if (
    Math.abs(corner[0] - 33) > 8 ||
    Math.abs(corner[1] - 49) > 8 ||
    Math.abs(corner[2] - 73) > 8 ||
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
      "--enable-unsafe-swiftshader",
      "--enable-features=Vulkan",
      "--use-angle=swiftshader",
      "--disable-vulkan-surface",
    );
  }
  const browser = await chromium.launch({
    executablePath: chromePath,
    headless: true,
    args: browserArguments,
  });
  const page = await browser.newPage();
  const browserMessages = [];
  page.on("console", (message) => browserMessages.push(`${message.type()}: ${message.text()}`));
  page.on("pageerror", (error) => browserMessages.push(`pageerror: ${error.message}`));

  try {
    await page.goto(`http://127.0.0.1:${address.port}/${entryName}`, {
      waitUntil: "load",
    });
    await page.waitForFunction(
      () => {
        const status = document.querySelector("#granit-status")?.dataset.status;
        return status === "ready" || status === "failed";
      },
      undefined,
      { timeout: 30_000 },
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
    await page.waitForFunction(
      () =>
        typeof Module._granit_web_rendered_frame_count === "function" &&
        Module._granit_web_rendered_frame_count() >= 60,
      undefined,
      { timeout: 10_000 },
    );
    for (const assetPath of ["/model_viewer_fixture.gltf", "/model_viewer_fixture.bin"]) {
      if (!requestedPaths.has(assetPath))
        throw new Error(`浏览器资源加载链路未请求 ${assetPath}`);
    }
    validateModelViewerPixels(await page.locator("#canvas").screenshot({ type: "png" }));

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
    if (
      shutdownResult !== 0 ||
      repeatedShutdownResult !== 0 ||
      shutdownStatus !== 3 ||
      liveResources !== 0 ||
      pendingRetirements !== 0 ||
      framesAfterShutdown !== framesBeforeShutdown
    ) {
      throw new Error(
        `浏览器资源释放异常：result=${shutdownResult}, repeated=${repeatedShutdownResult}, ` +
          `status=${shutdownStatus}, live=${liveResources}, pending=${pendingRetirements}, ` +
          `frames=${framesBeforeShutdown}->${framesAfterShutdown}`,
      );
    }
    console.log(
      "浏览器 WebGPU 多帧渲染、质量切换、输入、Resize、资产 Fetch 与资源释放验证通过",
    );

    rejectExternalBuffer();
    const failurePage = await browser.newPage();
    const failureMessages = [];
    failurePage.on("console", (message) =>
      failureMessages.push(`${message.type()}: ${message.text()}`),
    );
    failurePage.on("pageerror", (error) => failureMessages.push(`pageerror: ${error.message}`));
    await failurePage.goto(
      `http://127.0.0.1:${address.port}/${entryName}?missing-external-buffer=1`,
      { waitUntil: "load" },
    );
    await failurePage.waitForFunction(
      () => document.querySelector("#granit-status")?.dataset.status === "failed",
      undefined,
      { timeout: 30_000 },
    );
    const failureText = await failurePage.locator("#granit-status").textContent();
    if (!failureText?.startsWith("failed:asset-resource-fetch:")) {
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
