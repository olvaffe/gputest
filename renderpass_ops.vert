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
    const vec4 vertices[3] = {
        vec4(-1.0, -1.0, 0.0, 1.0),
        vec4(-1.0,  1.0, 0.0, 1.0),
        vec4( 1.0, -1.0, 0.0, 1.0),
    };
    gl_Position = vertices[gl_VertexIndex];
}
