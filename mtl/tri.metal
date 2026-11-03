/*
 * Copyright 2026 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include <metal_stdlib>

using namespace metal;

struct VSInput {
    float2 pos [[attribute(0)]];
    float3 color [[attribute(1)]];
};

struct FSInput {
    float4 pos [[position]];
    float3 color;
};

vertex FSInput
vs_main(VSInput input [[stage_in]])
{
    FSInput output;
    output.pos = float4(input.pos, 0.0f, 1.0f);
    output.color = input.color;
    return output;
}

fragment float4
fs_main(FSInput input [[stage_in]])
{
    return float4(input.color, 1.0f);
}
