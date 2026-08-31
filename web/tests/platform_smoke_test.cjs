// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

const fs = require("node:fs");
const http = require("node:http");
const path = require("node:path");
const zlib = require("node:zlib");
const { chromium } = require("playwright-core");

const outputDirectory = path.resolve(process.argv[2] ?? "build/emscripten-release/web");
const chromePath = process.env.CHROME_PATH ?? "/usr/bin/google-chrome";
const entryName = "granit_webgpu_triangle_example.html";

const contentTypes = new Map([
  [".data", "application/octet-stream"],
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

function validateCanvasPixels(png) {
  const image = decodePng(png);
  const center = pixelAt(image, Math.floor(image.width / 2), Math.floor(image.height / 2));
  const corner = pixelAt(image, 4, 4);
  if (process.platform === "linux" && center[3] === 0) {
    console.warn("Linux 无头 Chrome 未暴露 WebGPU Canvas 合成像素，跳过截图颜色断言");
    return;
  }
  if (!(center[2] >= 180 && center[0] <= 80 && center[1] <= 80))
    throw new Error(`WebGPU 深度遮挡中心像素异常：${center.join(",")}`);
  if (!(corner[0] <= 40 && corner[1] <= 40 && corner[2] <= 40))
    throw new Error(`WebGPU 清屏角落像素异常：${corner.join(",")}`);
}

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
    // GRANIT_RENDERER_STATE_READY 的公共 ABI 数值为 2。
    if (rendererState !== 2 || failureResult !== 0) {
      throw new Error(
        `WebGPU Renderer 生命周期异常，state=${rendererState}, failure=${failureResult}`,
      );
    }
    validateCanvasPixels(await page.locator("#canvas").screenshot({ type: "png" }));

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
    console.log("浏览器 WebGPU 深度遮挡绘制闭环与输入转发验证通过");
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
