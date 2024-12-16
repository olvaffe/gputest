/*
 * Copyright 2022 Google LLC
 * SPDX-License-Identifier: MIT
 */

#version 460 core

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

    gl_Position = vec4(vertices[gl_VertexIndex % 4], 0.0, 1.0);
}
