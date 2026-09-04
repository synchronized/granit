// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_PIPELINE_TONE_MAPPING_RECORDER_H_
#define GRANIT_PIPELINE_TONE_MAPPING_RECORDER_H_

#include "lighting/tone_mapping_resources.h"

namespace granit::pipeline::detail {

/** 录制一次全屏 Tone Mapping；Pipeline 资源可跨帧复用，绑定资源仅在本次调用内存活。 */
[[nodiscard]] granit_result
record_tone_mapping(lighting::tone_mapping_pipeline_resources& pipeline, granit_renderer renderer,
                    granit_command_recorder recorder, granit_texture_view hdr_view,
                    granit_texture_view output_view, granit_texture_format output_format,
                    std::uint32_t width, std::uint32_t height,
                    const lighting::tone_mapping_constants& constants);

} // namespace granit::pipeline::detail

#endif
