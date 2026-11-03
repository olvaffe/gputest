/*
 * Copyright 2026 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include <metal_stdlib>

using namespace metal;

struct VSInput {
    float2 pos [[attribute(0)]];
    float2 uv [[attribute(1)]];
};

struct FSInput {
    float4 pos [[position]];
    float2 uv;
};

vertex FSInput
vs_main(VSInput input [[stage_in]])
{
    FSInput output;
    output.pos = float4(input.pos, 0.0f, 1.0f);
    output.uv = input.uv;
    return output;
}

fragment float4
fs_main(FSInput input [[stage_in]],
        texture2d<float> tex [[texture(0)]],
        sampler samp [[sampler(0)]])
{
    return tex.sample(samp, input.uv);
}
