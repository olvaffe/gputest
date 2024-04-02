/*
 * Copyright 2022 Google LLC
 * SPDX-License-Identifier: MIT
 */

#version 460 core

layout(location = 0) out vec2 out_texcoord;

out gl_PerVertex {
    vec4 gl_Position;
};

void main()
{
    const vec2 vertices[4] = {
        vec2(-1.0, -1.0),
        vec2(-1.0,  1.0),
        vec2( 1.0, -1.0),
        vec2( 1.0,  1.0),
    };
    const vec2 texcoords[4] = {
        vec2(0.0, 0.0),
        vec2(0.0, 1.0),
        vec2(1.0, 0.0),
        vec2(1.0, 1.0),
    };

    gl_Position = vec4(vertices[gl_VertexIndex % 4], 0.0, 1.0);
    out_texcoord = texcoords[gl_VertexIndex % 4];
}
