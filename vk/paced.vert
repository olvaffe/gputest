#version 460 core

/*
 * Copyright 2025 Google LLC
 * SPDX-License-Identifier: MIT
 */

layout(push_constant) uniform CONSTS {
    uint vs_loop;
    uint fs_loop;
    uint cs_loop;
    float val;
} consts;

out gl_PerVertex {
    vec4 gl_Position;
};

void main()
{
    const vec4 vertices[3] = {
        vec4(-1.0, -1.0, 0.0, 1.0),
        vec4(-1.0,  1.0, 0.0, 1.0),
        vec4( 1.0, -1.0, 0.0, 1.0),
    };
    vec4 val = vertices[gl_VertexIndex % 3];

    for (uint i = 0; i < consts.vs_loop; i++)
        val.x += consts.val;

    gl_Position = val;
}
