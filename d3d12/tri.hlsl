/*
 * Copyright 2026 Google LLC
 * SPDX-License-Identifier: MIT
 */

struct VSInput {
    float2 pos : POSITION;
    float3 color : COLOR;
};

struct PSInput {
    float4 pos : SV_POSITION;
    float3 color : COLOR;
};

PSInput vs_main(VSInput input)
{
    PSInput output;
    output.pos = float4(input.pos, 0.0f, 1.0f);
    output.color = input.color;
    return output;
}

float4 ps_main(PSInput input) : SV_Target
{
    return float4(input.color, 1.0f);
}
