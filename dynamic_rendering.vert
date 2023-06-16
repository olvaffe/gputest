/*
 * Copyright 2022 Google LLC
 * SPDX-License-Identifier: MIT
 */

#version 460 core

layout(location = 0) out vec3 out_color;

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
    const vec3 colors[3] = {
        vec3(1.0, 0.0, 0.0),
        vec3(0.0, 1.0, 0.0),
        vec3(0.0, 0.0, 1.0),
    };

    gl_Position = vertices[gl_VertexIndex % 3];
    out_color = colors[gl_VertexIndex % 3];
}
