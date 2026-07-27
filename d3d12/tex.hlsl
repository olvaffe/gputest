/*
 * Copyright 2026 Google LLC
 * SPDX-License-Identifier: MIT
 */

Texture2D g_texture : register(t0);
SamplerState g_sampler : register(s0);

struct VSInput {
    float2 pos : POSITION;
    float2 uv : TEXCOORD;
};

struct PSInput {
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD;
};

PSInput vs_main(VSInput input)
{
    PSInput output;
    output.pos = float4(input.pos, 0.0f, 1.0f);
    output.uv = input.uv;
    return output;
}

float4 ps_main(PSInput input) : SV_Target
{
    return g_texture.Sample(g_sampler, input.uv);
}
