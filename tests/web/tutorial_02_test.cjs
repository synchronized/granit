// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

const fs = require("node:fs");
const http = require("node:http");
const path = require("node:path");
const { chromium } = require("playwright-core");

const outputDirectory = path.resolve(process.argv[2] ?? "build/emscripten-release/web");
const chromePath = process.env.CHROME_PATH ?? "/usr/bin/google-chrome";
const contentTypes = new Map([
  [".html", "text/html; charset=utf-8"],
  [".js", "text/javascript; charset=utf-8"],
  [".wasm", "application/wasm"],
  [".data", "application/octet-stream"],
]);

const server = http.createServer((request, response) => {
  const requestPath = new URL(request.url, "http://127.0.0.1").pathname;
  const relativePath = requestPath === "/" ? "granit_tutorial_02_pbr_assets.html" : requestPath.slice(1);
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
  const browser = await chromium.launch({
    executablePath: chromePath,
    headless: process.env.GRANIT_BROWSER_HEADLESS !== "0",
    args: ["--enable-unsafe-webgpu", "--no-sandbox"],
  });
  const page = await browser.newPage({ viewport: { width: 640, height: 480 } });
  const errors = [];
  page.on("pageerror", (error) => errors.push(error.message));
  page.on("console", (message) => {
    if (message.type() === "error") errors.push(message.text());
  });
  try {
    await page.goto(`http://127.0.0.1:${address.port}/granit_tutorial_02_pbr_assets.html`);
    try {
      await page.waitForFunction(
        () =>
          Module.runtimeReady === true &&
          typeof Module._granit_tutorial_02_ready === "function" &&
          Module._granit_tutorial_02_ready() === 1 &&
          Module._granit_tutorial_02_rendered_frames() >= 3 &&
          Module._granit_tutorial_02_canvas_items() > 0,
        undefined,
        { timeout: 30_000 },
      );
    } catch (error) {
      const state = await page.evaluate(() => ({
        runtimeReady: Module.runtimeReady,
        hasReady: typeof Module._granit_tutorial_02_ready === "function",
        ready: Module._granit_tutorial_02_ready?.(),
        frames: Module._granit_tutorial_02_rendered_frames?.(),
        canvasItems: Module._granit_tutorial_02_canvas_items?.(),
        shutdownReason: Module._granit_tutorial_02_shutdown_reason?.(),
      }));
      throw new Error(`${error.message}; state=${JSON.stringify(state)}; errors=${errors.join(" | ")}`);
    }

    const canvas = page.locator("#canvas");
    const box = await canvas.boundingBox();
    if (box === null) throw new Error("Tutorial 02 PBR Assets Canvas 不可见");
    await page.mouse.move(box.x + 64, box.y + 64);
    await page.mouse.down({ button: "left" });
    await page.mouse.move(box.x + 120, box.y + 80);
    await page.mouse.up({ button: "left" });
    await page.mouse.wheel(0, -80);
    await page.waitForFunction(
      () => Module._granit_tutorial_02_pointer_events() >= 4,
      undefined,
      { timeout: 10_000 },
    );

    const before = await page.evaluate(() => Module._granit_tutorial_02_rendered_frames());
    await page.evaluate(() => {
      const canvas = document.querySelector("#canvas");
      canvas.style.width = "800px";
      canvas.style.height = "450px";
      canvas.width = 800;
      canvas.height = 450;
    });
    try {
      await page.waitForFunction(
        (previous) =>
          Module._granit_tutorial_02_recreate_count() >= 1 &&
          Module._granit_tutorial_02_rendered_frames() > previous,
        before,
        { timeout: 10_000 },
      );
    } catch (error) {
      const state = await page.evaluate(() => ({
        ready: Module._granit_tutorial_02_ready?.(),
        frames: Module._granit_tutorial_02_rendered_frames?.(),
        recreates: Module._granit_tutorial_02_recreate_count?.(),
        shutdownReason: Module._granit_tutorial_02_shutdown_reason?.(),
      }));
      throw new Error(`${error.message}; resize state=${JSON.stringify(state)}; errors=${errors.join(" | ")}`);
    }
    if (errors.length !== 0)
      throw new Error(`Tutorial 02 PBR Assets 浏览器错误：\n${errors.join("\n")}`);
    console.log("Tutorial 02 PBR Assets WebGPU 多帧、Canvas、统一输入与 Resize 验证通过");
  } finally {
    await browser.close();
    await new Promise((resolve) => server.close(resolve));
  }
}

main().catch((error) => {
  console.error(error);
  process.exitCode = 1;
});
