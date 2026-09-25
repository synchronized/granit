// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

const fs = require("node:fs");
const http = require("node:http");
const path = require("node:path");
const { chromium } = require("playwright-core");
const { decodePng, pixelAt } = require("./png.cjs");

const outputDirectory = path.resolve(process.argv[2] ?? "build/emscripten-release/web");
const target = process.argv[3];
const tutorialNumber = process.argv[4];
if (!target || !tutorialNumber) {
  console.error("用法：tutorial_feature_test.cjs <输出目录> <目标名> <教程编号>");
  process.exit(2);
}

const chromePath = process.env.CHROME_PATH ?? "/usr/bin/google-chrome";
const contentTypes = new Map([
  [".html", "text/html; charset=utf-8"],
  [".js", "text/javascript; charset=utf-8"],
  [".wasm", "application/wasm"],
]);

const server = http.createServer((request, response) => {
  const requestPath = new URL(request.url, "http://127.0.0.1").pathname;
  const relativePath = requestPath === "/" ? `${target}.html` : requestPath.slice(1);
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

async function main() {
  await new Promise((resolve, reject) => {
    server.once("error", reject);
    server.listen(0, "127.0.0.1", resolve);
  });
  const address = server.address();
  const arguments = ["--enable-unsafe-webgpu", "--no-sandbox"];
  if (process.platform !== "win32")
    arguments.push("--enable-features=Vulkan", "--use-angle=vulkan", "--disable-vulkan-surface");
  const browser = await chromium.launch({
    executablePath: chromePath,
    headless: process.env.GRANIT_BROWSER_HEADLESS !== "0",
    args: arguments,
  });
  // Example Application 默认创建 1280×720 窗口。视口与 Canvas CSS 尺寸一致，避免 Locator
  // 截图把视口之外的区域补成透明像素，进而误判中心像素。
  const page = await browser.newPage({ viewport: { width: 1280, height: 720 } });
  const errors = [];
  page.on("pageerror", (error) => errors.push(error.message));
  page.on("console", (message) => {
    const text = message.text();
    if (message.type() === "error" ||
        (message.type() === "warning" && /is invalid|too small|validation/i.test(text))) {
      errors.push(text);
    }
  });
  const exportName = (suffix) => `_granit_tutorial_${tutorialNumber}_${suffix}`;
  try {
    await page.goto(`http://127.0.0.1:${address.port}/${target}.html`);
    await page.waitForFunction(
      ({ ready, frames, feature }) =>
        Module.runtimeReady === true &&
        typeof Module[ready] === "function" &&
        Module[ready]() === 1 &&
        Module[frames]() >= 3 &&
        Module[feature]() > 0,
      {
        ready: exportName("ready"),
        frames: exportName("rendered_frames"),
        feature: exportName("feature_value"),
      },
      { timeout: 30_000 },
    );

    const canvas = page.locator("#canvas");
    await page.waitForTimeout(500);
    const image = decodePng(await canvas.screenshot());
    const center = pixelAt(image, Math.floor(image.width / 2), Math.floor(image.height / 2));
    const corner = pixelAt(image, 4, 4);
    const colorDistance = Math.abs(center[0] - corner[0]) + Math.abs(center[1] - corner[1]) +
      Math.abs(center[2] - corner[2]);
    if (colorDistance < 60)
      throw new Error(`教程 ${tutorialNumber} 中心像素与背景差异不足：${center} / ${corner}`);

    const before = await page.evaluate((name) => Module[name](), exportName("rendered_frames"));
    await page.evaluate(() => {
      const targetCanvas = document.querySelector("#canvas");
      targetCanvas.style.width = "800px";
      targetCanvas.style.height = "450px";
      targetCanvas.width = 800;
      targetCanvas.height = 450;
    });
    await page.waitForFunction(
      ({ previous, frames, recreates }) =>
        Module[recreates]() >= 1 && Module[frames]() > previous,
      {
        previous: before,
        frames: exportName("rendered_frames"),
        recreates: exportName("recreate_count"),
      },
      { timeout: 10_000 },
    );
    if (errors.length !== 0)
      throw new Error(`教程 ${tutorialNumber} 浏览器错误：\n${errors.join("\n")}`);
    console.log(`教程 ${tutorialNumber} WebGPU 多帧、像素与 Resize 验证通过`);
  } finally {
    await browser.close();
    await new Promise((resolve) => server.close(resolve));
  }
}

main().catch((error) => {
  console.error(error);
  process.exitCode = 1;
});
