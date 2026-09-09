// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

const zlib = require("node:zlib");

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

module.exports = { decodePng, pixelAt };
