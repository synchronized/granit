// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

const fs = require("node:fs");
const http = require("node:http");
const path = require("node:path");
const { chromium } = require("playwright-core");

const outputDirectory = path.resolve(process.argv[2] ?? "build/emscripten-release/web");
const chromePath = process.env.CHROME_PATH ?? "/usr/bin/google-chrome";
const entryName = "granit_web_platform_smoke.html";

const contentTypes = new Map([
  [".data", "application/octet-stream"],
  [".html", "text/html; charset=utf-8"],
  [".js", "text/javascript; charset=utf-8"],
  [".wasm", "application/wasm"],
]);

function startServer() {
  const server = http.createServer((request, response) => {
    const requestPath = new URL(request.url, "http://127.0.0.1").pathname;
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
        "Content-Type": contentTypes.get(path.extname(filePath)) ?? "application/octet-stream",
      });
      response.end(content);
    });
  });
  return new Promise((resolve, reject) => {
    server.once("error", reject);
    server.listen(0, "127.0.0.1", () => resolve(server));
  });
}

async function main() {
  const server = await startServer();
  const address = server.address();
  const browser = await chromium.launch({
    executablePath: chromePath,
    headless: true,
    args: [
      "--enable-unsafe-webgpu",
      "--enable-unsafe-swiftshader",
      "--enable-features=Vulkan",
      "--use-angle=swiftshader",
      "--disable-vulkan-surface",
      "--no-sandbox",
    ],
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
    if (rendererState !== 1 || failureResult !== 0) {
      throw new Error(
        `WebGPU Renderer 生命周期异常，state=${rendererState}, failure=${failureResult}`,
      );
    }

    await page.keyboard.press("A");
    await page.locator("#canvas").click({ position: { x: 16, y: 16 } });
    await page.mouse.move(32, 32);
    await page.waitForFunction(
      () =>
        typeof Module._granit_web_input_event_count === "function" &&
        Module._granit_web_input_event_count() >= 2,
      undefined,
      { timeout: 5_000 },
    );
    console.log("浏览器 WebGPU 启动与输入转发验证通过");
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
