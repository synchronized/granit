// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

const fs = require("node:fs");
const http = require("node:http");
const path = require("node:path");
const { decodePng, pixelAt } = require("./png.cjs");
const { chromium } = require("playwright-core");

const outputDirectory = path.resolve(process.argv[2] ?? "build/emscripten-release/web");
const chromePath = process.env.CHROME_PATH ?? "/usr/bin/google-chrome";
const contentTypes = new Map([
  [".html", "text/html; charset=utf-8"],
  [".js", "text/javascript; charset=utf-8"],
  [".wasm", "application/wasm"],
]);

const server = http.createServer((request, response) => {
  const requestPath = new URL(request.url, "http://127.0.0.1").pathname;
  const relativePath = requestPath === "/" ? "granit_imgui_web.html" : requestPath.slice(1);
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

// 对固定区域验证语义，避免将未经确认的截图自动升级为基准。
function validateScene(png, ratio, enabled) {
  const image = decodePng(png);
  if (image.width !== 640 * ratio || image.height !== 480 * ratio)
    throw new Error(`DPI 截图尺寸异常：${image.width}×${image.height}`);
  function color(x, y, expected) {
    const actual = pixelAt(image, Math.floor(x * ratio), Math.floor(y * ratio));
    if (expected.some((value, channel) => Math.abs(value - actual[channel]) > 12))
      throw new Error(`验收像素 (${x}, ${y})：实际 ${actual}，预期 ${expected}`);
  }
  color(160, 90, [255, 0, 0]);
  for (const [x, y] of [[132, 90], [212, 90], [160, 56], [160, 136]])
    color(x, y, [0, 0, 0]);
  color(40, 176, enabled ? [0, 255, 0] : [255, 0, 0]);
  const bright = pixelAt(image, 36 * ratio, 76 * ratio);
  const dark = pixelAt(image, 76 * ratio, 76 * ratio);
  if (bright[0] - dark[0] < 80 || bright[2] - dark[2] < 60)
    throw new Error("自定义纹理四分区未正确显示");
  let glyphPixels = 0;
  for (let y = 24 * ratio; y < 44 * ratio; ++y)
    for (let x = 24 * ratio; x < 210 * ratio; ++x)
      if (pixelAt(image, x, y)[0] > 100) ++glyphPixels;
  if (glyphPixels < 100 * ratio * ratio || glyphPixels > 2000 * ratio * ratio)
    throw new Error(`字体覆盖异常：${glyphPixels}`);
}

async function validateVisualScene(browser, address, ratio) {
  const context = await browser.newContext({
    viewport: { width: 640, height: 480 }, deviceScaleFactor: ratio,
  });
  const page = await context.newPage();
  const errors = [];
  let completed = false;
  page.on("pageerror", (error) => errors.push(error.message));
  page.on("console", (message) => {
    if (message.type() === "error") errors.push(message.text());
  });
  try {
    await page.goto(`http://127.0.0.1:${address.port}/granit_imgui_web.html?validation=1`);
    await page.waitForFunction(() => Module._granit_web_imgui_rendered_frames?.() >= 30);
    const canvas = page.locator("#canvas");
    const artifact = path.join(outputDirectory, "validation");
    fs.mkdirSync(artifact, { recursive: true });
    const before = await canvas.screenshot({ path: path.join(artifact, `imgui-${ratio}x-before.png`) });
    validateScene(before, ratio, true);
    await canvas.click({ position: { x: 40, y: 176 } });
    await page.waitForFunction(() => Module._granit_web_imgui_validation_enabled() === 0);
    const frame = await page.evaluate(() => Module._granit_web_imgui_rendered_frames());
    await page.waitForFunction(
      (previous) => Module._granit_web_imgui_rendered_frames() > previous + 2, frame,
    );
    const after = await canvas.screenshot({ path: path.join(artifact, `imgui-${ratio}x-after.png`) });
    validateScene(after, ratio, false);
    if (errors.length) throw new Error(errors.join("\n"));
    console.log(`ImGui ${ratio}× DPI 字体、纹理、裁剪和点击状态视觉验收通过`);
    completed = true;
    return context;
  } finally {
    if (!completed) await context.close();
  }
}

async function main() {
  await new Promise((resolve) => server.listen(0, "127.0.0.1", resolve));
  const address = server.address();
  const browser = await chromium.launch({
    executablePath: chromePath,
    headless: true,
    args: ["--enable-unsafe-webgpu", "--enable-features=Vulkan,UseSkiaRenderer"],
  });
  const page = await browser.newPage({ viewport: { width: 960, height: 540 } });
  const visualContexts = [];
  const failures = [];
  const messages = [];
  page.on("console", (message) => {
    messages.push(`${message.type()}: ${message.text()}`);
    if (message.type() === "error") failures.push(message.text());
  });
  page.on("pageerror", (error) => failures.push(error.message));
  try {
    // Emscripten 的外部 WebGPU 实例在显式关闭后不可重建；先完成所有视觉页面。
    for (const ratio of [1, 2])
      visualContexts.push(await validateVisualScene(browser, address, ratio));
    await page.goto(`http://127.0.0.1:${address.port}/granit_imgui_web.html`, {
      waitUntil: "load",
    });
    try {
      await page.waitForFunction(
        () => document.querySelector("#granit-status")?.dataset.status === "ready",
        undefined,
        { timeout: 30_000 },
      );
    } catch (error) {
      const status = await page.locator("#granit-status").textContent();
      const runtime = await page.evaluate(() => ({
        ready: Module.runtimeReady === true,
        hasFrames: typeof Module._granit_web_imgui_rendered_frames === "function",
      }));
      throw new Error(
        `Web ImGui 未进入 ready：${status}，runtime=${JSON.stringify(runtime)}\n` +
          `${messages.join("\n")}\n${error}`,
      );
    }
    try {
      await page.waitForFunction(
        () =>
          typeof Module._granit_web_imgui_rendered_frames === "function" &&
          Module._granit_web_imgui_rendered_frames() >= 3,
        undefined,
        { timeout: 30_000 },
      );
    } catch (error) {
      const frames = await page.evaluate(() => Module._granit_web_imgui_rendered_frames());
      throw new Error(`Web ImGui 帧未推进：frames=${frames}\n${messages.join("\n")}\n${error}`);
    }
    const canvas = page.locator("#canvas");
    const box = await canvas.boundingBox();
    if (box === null) throw new Error("Web ImGui Canvas 不可见");
    await page.mouse.move(box.x + 48, box.y + 48);
    await page.mouse.down({ button: "left" });
    await page.mouse.move(box.x + 96, box.y + 72);
    await page.mouse.up({ button: "left" });
    await page.mouse.wheel(0, -80);
    await page.waitForFunction(
      () =>
        typeof Module._granit_web_imgui_pointer_events === "function" &&
        Module._granit_web_imgui_pointer_events() >= 4,
      undefined,
      { timeout: 10_000 },
    );
    await page.setViewportSize({ width: 800, height: 600 });
    await page.waitForFunction(
      () =>
        typeof Module._granit_web_imgui_resize_count === "function" &&
        Module._granit_web_imgui_resize_count() >= 1,
      undefined,
      { timeout: 10_000 },
    );
    if (failures.length !== 0)
      throw new Error(`Web ImGui 控制台出现异常：\n${failures.join("\n")}`);
    const shutdown = await page.evaluate(() => Module._granit_web_imgui_shutdown());
    if (shutdown !== 0) throw new Error(`Web ImGui 关闭失败：${shutdown}`);
    console.log("浏览器 SDL3 + ImGui 多帧渲染、输入、Resize 与资源释放验证通过");
  } finally {
    await page.close();
    await Promise.allSettled(visualContexts.map((context) => context.close()));
    await browser.close();
    server.close();
  }
}

main().catch((error) => {
  console.error(error);
  process.exitCode = 1;
});
