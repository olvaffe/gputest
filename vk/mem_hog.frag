#version 460 core

/*
 * Copyright 2026 Google LLC
 * SPDX-License-Identifier: MIT
 */

layout(push_constant) uniform CONSTS {
    uint vs_loop;
    uint fs_loop;
    float val;
} consts;

layout(location = 0) out vec4 out_color;

void main()
{
    vec4 val = gl_FragCoord;

    for (uint i = 0; i < consts.fs_loop; i++)
        val.x += consts.val;

    out_color = val;
}
