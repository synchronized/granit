// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

function browser_args() {
  const args = [
    "--enable-unsafe-webgpu",
    "--no-sandbox",
    "--ignore-gpu-blocklist",
  ];
  if (process.platform !== "win32") {
    args.push(
      "--use-angle=swiftshader",
      "--enable-unsafe-swiftshader",
      "--enable-gpu",
      "--enable-features=Vulkan",
      "--use-vulkan=swiftshader",
    );
  }
  return args;
}

module.exports = { browser_args };
