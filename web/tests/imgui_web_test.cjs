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

async function main() {
  await new Promise((resolve) => server.listen(0, "127.0.0.1", resolve));
  const address = server.address();
  const browser = await chromium.launch({
    executablePath: chromePath,
    headless: true,
    args: ["--enable-unsafe-webgpu", "--enable-features=Vulkan,UseSkiaRenderer"],
  });
  const page = await browser.newPage({ viewport: { width: 960, height: 540 } });
  const failures = [];
  const messages = [];
  page.on("console", (message) => {
    messages.push(`${message.type()}: ${message.text()}`);
    if (message.type() === "error") failures.push(message.text());
  });
  page.on("pageerror", (error) => failures.push(error.message));
  try {
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
    await page.waitForFunction(
      () =>
        typeof Module._granit_web_imgui_rendered_frames === "function" &&
        Module._granit_web_imgui_rendered_frames() >= 3,
      undefined,
      { timeout: 30_000 },
    );
    if (failures.length !== 0)
      throw new Error(`Web ImGui 控制台出现异常：\n${failures.join("\n")}`);
    const shutdown = await page.evaluate(() => Module._granit_web_imgui_shutdown());
    if (shutdown !== 0) throw new Error(`Web ImGui 关闭失败：${shutdown}`);
    console.log("浏览器 SDL3 + ImGui 多帧渲染与资源释放验证通过");
  } finally {
    await page.close();
    await browser.close();
    server.close();
  }
}

main().catch((error) => {
  console.error(error);
  process.exitCode = 1;
});
