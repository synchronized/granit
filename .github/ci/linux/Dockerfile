# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

FROM ubuntu:24.04

ARG DEBIAN_FRONTEND=noninteractive

RUN apt-get update \
    && apt-get install --no-install-recommends -y \
      build-essential \
      ca-certificates \
      ccache \
      clang \
      cmake \
      git \
      libegl1-mesa-dev \
      libvulkan-dev \
      libvulkan1 \
      libwayland-dev \
      libx11-dev \
      libx11-xcb-dev \
      libxcb1-dev \
      libxcursor-dev \
      libxext-dev \
      libxfixes-dev \
      libxi-dev \
      libxinerama-dev \
      libxkbcommon-dev \
      libxrandr-dev \
      libxss-dev \
      libxtst-dev \
      mesa-vulkan-drivers \
      ninja-build \
      pkg-config \
      vulkan-tools \
      vulkan-validationlayers \
      wayland-protocols \
      weston \
      xauth \
      xvfb \
    && rm -rf /var/lib/apt/lists/*

LABEL org.opencontainers.image.source="https://github.com/synchronized/granit"
LABEL org.opencontainers.image.description="Granit Linux Vulkan CI 与编译缓存基础镜像"
LABEL org.opencontainers.image.licenses="MIT"
