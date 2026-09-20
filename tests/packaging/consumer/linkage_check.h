// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_CONSUMER_LINKAGE_CHECK_H_
#define GRANIT_CONSUMER_LINKAGE_CHECK_H_

#if defined(GRANIT_CONSUMER_EXPECT_STATIC)
#if !defined(GRANIT_STATIC_DEFINE)
#error "静态核心库必须向使用者传播 GRANIT_STATIC_DEFINE"
#endif
#if defined(GRANIT_CONSUMER_CHECK_RENDER_PIPELINE) && !defined(GRANIT_RENDER_PIPELINE_STATIC_DEFINE)
#error "静态 RenderPipeline 必须向使用者传播 GRANIT_RENDER_PIPELINE_STATIC_DEFINE"
#endif
#elif defined(GRANIT_CONSUMER_EXPECT_SHARED)
#if defined(GRANIT_STATIC_DEFINE)
#error "共享核心库不得向使用者传播 GRANIT_STATIC_DEFINE"
#endif
#if defined(GRANIT_CONSUMER_CHECK_RENDER_PIPELINE) && defined(GRANIT_RENDER_PIPELINE_STATIC_DEFINE)
#error "共享 RenderPipeline 不得向使用者传播 GRANIT_RENDER_PIPELINE_STATIC_DEFINE"
#endif
#else
#error "Consumer 必须声明预期的 Granit 链接类型"
#endif

#endif
